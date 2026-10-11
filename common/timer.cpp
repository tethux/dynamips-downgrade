#include <asio/io_context.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <expected>
#include <future>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>

// Legacy headers define _Unused, which collides with standard-library names.
#include "timer.h"

#include <dynamips/macros.h>

struct timer_entry {
  long interval;
  m_tmcnt_t expire, offset;
  timer_proc callback;
  void *user_arg;
  int flags;
  timer_id id;
  int level;
};

namespace {

constexpr int TIMER_RUNNING = 2;
constexpr int TIMER_BOUNDARY = 4;

enum class error {
  invalid_argument,
  out_of_memory,
  id_exhausted,
  not_found,
  system_failure,
  internal,
};

template <typename T> using result = std::expected<T, error>;
using clock = std::chrono::steady_clock;
using work_guard = asio::executor_work_guard<asio::io_context::executor_type>;

struct entry {
  timer_entry_t metadata{};
  asio::steady_timer timer;
  clock::time_point deadline{};
  std::uint64_t generation = 0;
  bool cancelled = false;

  explicit entry(asio::io_context &context) : timer{context} {}
};

using entry_ptr = std::shared_ptr<entry>;

struct scheduler {
  asio::io_context context;
  std::optional<work_guard> work;
  std::thread worker;
  std::unordered_map<timer_id, entry_ptr> entries;
  timer_id next_id = 1;
  std::uint64_t generation = 0;
  bool started = false;

  template <typename T, typename Operation>
  fn invoke(Operation operation) noexcept -> result<T>;

  fn start() noexcept -> result<void>;
  fn add(m_tmcnt_t interval, m_tmcnt_t offset, bool boundary, int level,
         timer_proc callback, void *user_arg) noexcept -> result<timer_id>;
  fn remove(timer_id id) noexcept -> result<void>;
  fn set_interval(timer_id id, long interval) noexcept -> result<void>;
  fn advance(const entry_ptr &value, bool from_now) noexcept -> result<void>;
  fn arm(const entry_ptr &value) noexcept -> result<void>;
  fn on_wait(const entry_ptr &value, std::uint64_t cycle,
             const std::error_code &failure) noexcept -> result<void>;
  fn stop() noexcept -> result<void>;
};

scheduler state;

fn scheduler::start() noexcept -> result<void> {
  if (started)
    return {};

  if (worker.joinable())
    return std::unexpected(error::system_failure);

  if (generation == std::numeric_limits<std::uint64_t>::max())
    return std::unexpected(error::id_exhausted);

  // registered after states construction so cleanup runs before destruction
  static const int cleanup_registered =
      std::atexit([] { timer_flush_queues(); });

  if (cleanup_registered != 0)
    return std::unexpected(error::system_failure);

  try {
    ++generation;
    context.restart();
    work.emplace(context.get_executor());

    worker = std::thread{[this]() noexcept {
      try {
        context.run();
      } catch (...) {
        std::fputs("timer: worker failed.\n", stderr);
        context.stop();
      }
    }};

    started = true;
    return {};
  } catch (const std::bad_alloc &) {
    work.reset();
    context.stop();
    return std::unexpected(error::out_of_memory);
  } catch (const std::system_error &) {
    work.reset();
    context.stop();
    return std::unexpected(error::system_failure);
  } catch (...) {
    work.reset();
    context.stop();
    return std::unexpected(error::internal);
  }
}

template <typename T, typename Operation>
fn scheduler::invoke(Operation operation) noexcept -> result<T> {
  try {
    if (!started || context.stopped())
      return std::unexpected(error::system_failure);

    if (context.get_executor().running_in_this_thread())
      return operation();

    const let cycle = generation;
    let task = std::packaged_task<result<T>()>{
        [this, cycle, operation = std::move(operation)]() mutable -> result<T> {
          if (cycle != generation)
            return std::unexpected(error::system_failure);
          return operation();
        }};
    let completion = task.get_future();
    asio::post(context, std::move(task));

    // A failed worker must not leave a C caller waiting indefinitely.
    while (completion.wait_for(std::chrono::milliseconds{10}) !=
           std::future_status::ready) {
      if (context.stopped())
        return std::unexpected(error::system_failure);
    }
    return completion.get();
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn scheduler::add(m_tmcnt_t interval, m_tmcnt_t offset, bool boundary,
                  int level, timer_proc callback, void *user_arg) noexcept
    -> result<timer_id> {
  if (interval == 0 ||
      interval > static_cast<m_tmcnt_t>(std::numeric_limits<long>::max()) ||
      callback == nullptr)
    return std::unexpected(error::invalid_argument);

  return invoke<timer_id>([this, interval, offset, boundary, level, callback,
                           user_arg]() -> result<timer_id> {
    if (next_id == 0)
      return std::unexpected(error::id_exhausted);

    let value = std::make_shared<entry>(context);
    value->metadata.id = next_id;
    value->metadata.interval = static_cast<long>(interval);
    value->metadata.offset = offset;
    value->metadata.flags = boundary ? TIMER_BOUNDARY : 0;
    value->metadata.level = level;
    value->metadata.callback = callback;
    value->metadata.user_arg = user_arg;
    const let prepared = advance(value, true);
    if (!prepared)
      return std::unexpected(prepared.error());

    const let id = next_id;
    const let inserted = entries.emplace(id, value).second;
    if (!inserted)
      return std::unexpected(error::id_exhausted);

    const let armed = arm(value);
    if (!armed) {
      entries.erase(id);
      return std::unexpected(armed.error());
    }

    ++next_id;
    return id;
  });
}

fn scheduler::advance(const entry_ptr &value, bool from_now) noexcept
    -> result<void> {
  const let interval = value->metadata.interval;
  if (interval <= 0)
    return std::unexpected(error::invalid_argument);

  const let boundary = (value->metadata.flags & TIMER_BOUNDARY) != 0;

  const let now = clock::now();
  const let base = from_now || boundary ? now : value->deadline;
  const let available = std::chrono::duration_cast<std::chrono::milliseconds>(
                            clock::time_point::max() - base)
                            .count();

  let delay = static_cast<m_tmcnt_t>(interval);

  const let wall_now = std::chrono::system_clock::now();
  const let wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          wall_now.time_since_epoch())
                          .count();

  if (boundary) {
    const let seconds = std::chrono::system_clock::to_time_t(wall_now);
    std::tm local{};
    if (::localtime_r(&seconds, &local) == nullptr)
      return std::unexpected(error::system_failure);

    const let local_ms =
        wall_ms + static_cast<std::int64_t>(local.tm_gmtoff) * 1000;

    let phase = local_ms % interval;
    if (phase < 0)
      phase += interval;

    delay = static_cast<m_tmcnt_t>(interval - phase);
  }

  if (available < 0 || delay > static_cast<m_tmcnt_t>(available))
    return std::unexpected(error::invalid_argument);

  if (boundary) {
    if (value->metadata.offset > static_cast<m_tmcnt_t>(available) - delay)
      return std::unexpected(error::invalid_argument);

    delay += value->metadata.offset;
  }

  value->deadline =
      base + std::chrono::milliseconds{
                 static_cast<std::chrono::milliseconds::rep>(delay)};

  if (from_now || boundary)
    value->metadata.expire = static_cast<m_tmcnt_t>(wall_ms) + delay;
  else
    value->metadata.expire += delay;

  return {};
}

fn scheduler::remove(timer_id id) noexcept -> result<void> {
  return invoke<void>([this, id]() -> result<void> {
    const let found = entries.find(id);
    if (found == entries.end())
      return std::unexpected(error::not_found);

    const let value = found->second;
    value->cancelled = true;
    value->timer.cancel();
    entries.erase(found);
    return {};
  });
}

fn scheduler::set_interval(timer_id id, long interval) noexcept
    -> result<void> {
  if (interval <= 0)
    return std::unexpected(error::invalid_argument);

  return invoke<void>([this, id, interval]() -> result<void> {
    const let found = entries.find(id);
    if (found == entries.end())
      return std::unexpected(error::not_found);

    const let value = found->second;
    const let previous_interval = value->metadata.interval;
    value->metadata.interval = interval;

    const let advanced = advance(value, true);
    if (!advanced) {
      value->metadata.interval = previous_interval;
      return std::unexpected(advanced.error());
    }

    const let armed = arm(value);
    if (!armed) {
      value->cancelled = true;
      entries.erase(id);
      return std::unexpected(armed.error());
    }

    return {};
  });
}

fn scheduler::arm(const entry_ptr &value) noexcept -> result<void> {
  if (!context.get_executor().running_in_this_thread())
    return std::unexpected(error::invalid_argument);

  if (value->cancelled)
    return std::unexpected(error::not_found);

  if (value->generation == std::numeric_limits<std::uint64_t>::max())
    return std::unexpected(error::id_exhausted);

  try {
    const let cycle = ++value->generation;
    value->timer.expires_at(value->deadline);
    value->timer.async_wait(
        [this, value, cycle](const std::error_code &failure) noexcept {
          if (failure == asio::error::operation_aborted || value->cancelled ||
              cycle != value->generation)
            return;

          const let handled = on_wait(value, cycle, failure);
          if (!handled)
            std::fputs("timer: wait handler failed.\n", stderr);
        });
    return {};
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn scheduler::on_wait(const entry_ptr &value, std::uint64_t cycle,
                      const std::error_code &failure) noexcept -> result<void> {
  if (failure == asio::error::operation_aborted || value->cancelled ||
      cycle != value->generation)
    return {};

  let retire = [this, &value](error reason) -> result<void> {
    value->metadata.flags &= ~TIMER_RUNNING;
    value->cancelled = true;
    entries.erase(value->metadata.id);
    return std::unexpected(reason);
  };

  if (failure)
    return retire(error::system_failure);

  try {
    value->metadata.flags |= TIMER_RUNNING;

    const let repeat =
        value->metadata.callback(value->metadata.user_arg, &value->metadata);

    value->metadata.flags &= ~TIMER_RUNNING;

    // the callback may have removed or rescheduled its timer.
    if (value->cancelled || cycle != value->generation)
      return {};

    if (repeat == 0) {
      value->cancelled = true;
      entries.erase(value->metadata.id);
      return {};
    }

    const let advanced = advance(value, false);
    if (!advanced)
      return retire(advanced.error());

    const let armed = arm(value);
    if (!armed)
      return retire(armed.error());

    return {};
  } catch (const std::bad_alloc &) {
    return retire(error::out_of_memory);
  } catch (const std::system_error &) {
    return retire(error::system_failure);
  } catch (...) {
    return retire(error::internal);
  }
}

fn scheduler::stop() noexcept -> result<void> {
  if (!started)
    return {};

  if (worker.get_id() == std::this_thread::get_id())
    return std::unexpected(error::invalid_argument);

  try {
    context.stop();

    if (worker.joinable())
      worker.join();

    // worker stopped so timers canbe cancelled
    for (let &[ id, value ] : entries) {
      value->cancelled = true;
      value->timer.cancel();
    }

    entries.clear();
    work.reset();
    started = false;
    return {};
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

} // namespace

cfn timer_init() -> int {
  const let started = state.start();
  if (!started) {
    std::fputs("timer_init: unable to start the Asio timer scheduler.\n",
               stderr);
    return -1;
  }
  return 0;
}

cfn timer_create_entry(m_tmcnt_t interval, int boundary, int level,
                       timer_proc callback, void *user_arg) -> timer_id {
  const let added =
      state.add(interval, 0, boundary != 0, level, callback, user_arg);
  return added.value_or(0);
}

cfn timer_create_with_offset(m_tmcnt_t interval, m_tmcnt_t offset, int level,
                             timer_proc callback, void *user_arg) -> timer_id {
  const let added =
      state.add(interval, offset, true, level, callback, user_arg);
  return added.value_or(0);
}

cfn timer_remove(timer_id id) -> int {
  const let removed = state.remove(id);
  return removed ? 0 : -1;
}

cfn timer_set_interval(timer_id id, long interval) -> int {
  const let changed = state.set_interval(id, interval);
  return changed ? 0 : -1;
}

cfn timer_flush_queues() -> void {
  const let stopped = state.stop();
  if (!stopped)
    std::fputs("timer_flush_queues: unable to stop the Asio timer scheduler.\n",
               stderr);
}
