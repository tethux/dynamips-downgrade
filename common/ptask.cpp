#include "ptask.h"

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <expected>
#include <list>
#include <limits>
#include <new>
#include <system_error>

#include <dynamips/macros.h>

namespace {

enum class error {
  out_of_memory,
  id_exhausted,
  not_found,
  system_failure,
  internal,
};

template <typename T> using result = std::expected<T, error>;

struct task {
  ptask_id_t id;
  ptask_callback callback;
  void *object;
  void *arg;
};

struct scheduler {
  asio::io_context context;
  asio::steady_timer timer{context};
  std::mutex mutex;
  std::thread worker;

  std::list<task> tasks;
  ptask_id_t next_id = 0;
  std::uint64_t generation = 0;
  bool running = false;
  bool started = false;

  fn add(ptask_callback callback, void *object, void *arg) noexcept
      -> result<ptask_id_t>;

  fn remove(ptask_id_t id) noexcept -> result<void>;
  fn start(u_int interval) noexcept -> result<void>;
  fn sweep(std::uint64_t cycle) noexcept -> result<void>;
  fn stop() noexcept -> result<void>;
};

scheduler state;

fn scheduler::add(ptask_callback callback, void *object, void *arg) noexcept
    -> result<ptask_id_t> {
  try {
    let lock = std::lock_guard{mutex};

    if (next_id == std::numeric_limits<ptask_id_t>::max())
      return std::unexpected(error::id_exhausted);

    const let id = next_id + 1;
    tasks.push_front(task{id, callback, object, arg});
    next_id = id;

    return id;
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn scheduler::remove(ptask_id_t id) noexcept -> result<void> {
  try {
    let lock = std::lock_guard{mutex};

    for (let position = tasks.begin(); position != tasks.end(); ++position) {
      if (position->id != id)
        continue;

      tasks.erase(position);
      return {};
    }

    return std::unexpected(error::not_found);
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn scheduler::start(u_int interval) noexcept -> result<void> {
  try {
    let lock = std::lock_guard{mutex};

    if (started)
      return {};

    if (generation == std::numeric_limits<std::uint64_t>::max())
      return std::unexpected(error::id_exhausted);

    if (interval != 0)
      ptask_sleep_time = interval;

    context.restart();

    const let cycle = ++generation;
    running = true;

    try {
      asio::post(context, [this, cycle]() noexcept {
        const let swept = sweep(cycle);
        if (!swept) {
          std::fputs("ptask: initial sweep failed.\n", stderr);
          context.stop();
        }
      });

      worker = std::thread{[this]() noexcept {
        try {
          context.run();
        } catch (...) {
          std::fputs("ptask: worker failed.\n", stderr);
          context.stop();
        }
      }};
    } catch (...) {
      running = false;
      context.stop();
      throw;
    }

    started = true;
    return {};
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn scheduler::sweep(std::uint64_t cycle) noexcept -> result<void> {
  try {
    let lock = std::lock_guard{mutex};

    if (!running || cycle != generation)
      return {};

    for (let &entry : tasks)
      entry.callback(entry.object, entry.arg);

    timer.expires_after(std::chrono::milliseconds{ptask_sleep_time});

    timer.async_wait([this, cycle](const std::error_code &failure) noexcept {
      if (failure == asio::error::operation_aborted)
        return;

      if (failure) {
        std::fputs("ptask: timer wait failed.\n", stderr);
        context.stop();
        return;
      }

      const let swept = sweep(cycle);
      if (!swept) {
        std::fputs("ptask: callback sweep failed.\n", stderr);
        context.stop();
      }
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

fn scheduler::stop() noexcept -> result<void> {

  if (worker.get_id() == std::this_thread::get_id())
    return std::unexpected(error::system_failure);

  try {
    {
      let lock = std::lock_guard{mutex};

      if (!started)
        return {};

      running = false;
    }

    context.stop();

    if (worker.joinable())
      worker.join();

    timer.cancel();

    {
      let lock = std::lock_guard{mutex};
      started = false;
    }

    return {};
  } catch (const std::system_error &) {
    return std::unexpected(error::system_failure);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

} // namespace

u_int ptask_sleep_time = 10;

cfn ptask_add(ptask_callback callback, void *object, void *arg) -> ptask_id_t {
  const let added = state.add(callback, object, arg);
  return added.value_or(-1);
}

cfn ptask_remove(ptask_id_t id) -> int {
  const let removed = state.remove(id);
  return removed ? 0 : -1;
}

cfn ptask_init(u_int interval) -> int {
  const let started = state.start(interval);
  if (!started) {
    std::fputs("ptask_init: unable to start the Asio scheduler.\n", stderr);
    return -1;
  }
  return 0;
}

cfn ptask_shutdown() -> void {
  const let stopped = state.stop();
  if (!stopped)
    std::fputs("ptask_shutdown: unable to stop the Asio scheduler.\n", stderr);
}
