module;

#include <array>
#include <climits>
#include <cstdio>
#include <expected>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "registry.h"
#include <dynamips/macros.h>

export module registry;

export namespace dynamips::registry {

enum class object_type : int {
  vm = OBJ_TYPE_VM,
  nio = OBJ_TYPE_NIO,
  nio_bridge = OBJ_TYPE_NIO_BRIDGE,
  frame_relay_switch = OBJ_TYPE_FRSW,
  atm_switch = OBJ_TYPE_ATMSW,
  atm_bridge = OBJ_TYPE_ATM_BRIDGE,
  ethernet_switch = OBJ_TYPE_ETHSW,
  store = OBJ_TYPE_STORE,
};

enum class error {
  invalid_argument,
  not_initialized,
  not_found,
  duplicate,
  reference_overflow,
  negative_references,
  action_in_progress,
  out_of_memory,
  internal,
};

template <typename T> using result = std::expected<T, error>;

struct key {
  const char *name;
  object_type type;
};

struct entry_view {
  const char *name;
  void *data;
  object_type type;
  int references;
};

namespace detail {
struct node;
}

// Release before runtime teardown.
class reference final {
public:
  reference() noexcept = default;
  reference(const reference &) = delete;
  fn operator=(const reference &)->reference & = delete;
  reference(reference &&) noexcept = default;
  fn operator=(reference &&other) noexcept -> reference &;
  ~reference();

  [[nodiscard]] fn get() const noexcept -> void *;
  explicit operator bool() const noexcept { return bool(entry_); }
  fn reset() noexcept -> void;

private:
  friend fn retain(key object) noexcept -> result<reference>;
  explicit reference(std::shared_ptr<detail::node> entry) noexcept
      : entry_(std::move(entry)) {}
  std::shared_ptr<detail::node> entry_;
};

[[nodiscard]] fn retain(key object) noexcept -> result<reference>;

enum class action_result { referenced, retained, removed, executed };
using action = bool (*)(void *data, void *argument);
using visitor = void (*)(entry_view entry, void *argument);

} // namespace dynamips::registry

namespace dynamips::registry::detail {

// Shared metadata only; native references stay explicit.
using index = std::map<std::string, std::shared_ptr<node>, std::less<>>;
using order = std::list<node *>;

struct node {
  registry_entry_t entry{};
  index::iterator indexed;
  order::iterator ordered;
  bool linked = true;
  bool acting = false;

  node(key object, void *data) {
    entry.name = const_cast<char *>(object.name);
    entry.data = data;
    entry.object_type = static_cast<int>(object.type);
    entry.ref_count = 1;
  }
};

struct bucket {
  index names;
  order newest;
};

struct state {
  std::recursive_mutex mutex;
  bool initialized = false;
  std::array<bucket, REGISTRY_MAX_TYPES> types;

  fn bucket_for(object_type type) -> bucket & {
    return types[static_cast<std::size_t>(type)];
  }

  fn find(key object) -> std::shared_ptr<node> {
    let &names = bucket_for(object.type).names;
    const let found = names.find(object.name);
    return found == names.end() ? nullptr : found->second;
  }

  fn remove(node &value) -> void {
    value.linked = false;
    let &bucket = bucket_for(static_cast<object_type>(value.entry.object_type));
    bucket.newest.erase(value.ordered);
    bucket.names.erase(value.indexed);
  }

  template <typename Callback>
  fn visit(object_type type, Callback callback) -> int {
    std::vector<std::shared_ptr<node>> snapshot;
    for (let *value : bucket_for(type).newest)
      snapshot.push_back(value->indexed->second);
    int count = 0;
    for (const let &value : snapshot)
      if (value->linked)
        count += callback(*value);
    return count;
  }
};

fn global() -> state & {
  static state instance;
  return instance;
}

fn valid(object_type type) -> bool {
  return static_cast<int>(type) >= 0 &&
         static_cast<int>(type) < REGISTRY_MAX_TYPES;
}

fn valid(key object) -> bool { return object.name && valid(object.type); }

template <typename T, typename Operation>
fn locked(Operation operation, bool require_initialized = true) noexcept
    -> result<T> {
  try {
    let &registry = global();
    const std::lock_guard lock{registry.mutex};
    if (require_initialized && !registry.initialized)
      return std::unexpected(error::not_initialized);
    return operation(registry);
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

template <typename T, typename Operation>
fn with_entry(key object, Operation operation) noexcept -> result<T> {
  if (!valid(object))
    return std::unexpected(error::invalid_argument);
  return locked<T>([&](state &registry) -> result<T> {
    const let value = registry.find(object);
    if (!value)
      return std::unexpected(error::not_found);
    return operation(registry, *value);
  });
}

fn decrement(node &value) -> result<void> {
  if (value.entry.ref_count == INT_MIN)
    return std::unexpected(error::reference_overflow);
  if (--value.entry.ref_count < 0) {
    std::fprintf(stderr, "Registry: object %s (type %d): negative ref_count.\n",
                 value.indexed->first.c_str(), value.entry.object_type);
    return std::unexpected(error::negative_references);
  }
  return {};
}

struct action_scope {
  bool &acting;
  explicit action_scope(bool &value) noexcept : acting(value) { acting = true; }
  action_scope(const action_scope &) = delete;
  fn operator=(const action_scope &)->action_scope & = delete;
  ~action_scope() { acting = false; }
};

fn run_action(state &registry, node &value, int maximum, bool remove,
              action callback, void *argument) -> result<action_result> {
  if (value.acting)
    return std::unexpected(error::action_in_progress);
  if (value.entry.ref_count > maximum)
    return action_result::referenced;
  const action_scope scope{value.acting};
  const bool accepted = !callback || callback(value.entry.data, argument);
  if (remove && accepted) {
    registry.remove(value);
    return action_result::removed;
  }
  return accepted ? action_result::executed : action_result::retained;
}

} // namespace dynamips::registry::detail

export namespace dynamips::registry {

fn reference::operator=(reference &&other) noexcept -> reference & {
  if (this != &other) {
    reset();
    entry_ = std::move(other.entry_);
  }
  return *this;
}

reference::~reference() { reset(); }

fn reference::get() const noexcept -> void * {
  return entry_ ? entry_->entry.data : nullptr;
}

fn reference::reset() noexcept -> void {
  if (!entry_)
    return;
  const let released =
      detail::locked<void>([&](detail::state &) -> result<void> {
        const let value = std::move(entry_);
        return value->linked ? detail::decrement(*value) : result<void>{};
      });
  if (!released)
    std::fprintf(stderr, "Registry: scoped reference release failed (%d).\n",
                 static_cast<int>(released.error()));
}

[[nodiscard]] fn retain(key object) noexcept -> result<reference> {
  return detail::with_entry<reference>(
      object, [](detail::state &, detail::node &value) -> result<reference> {
        if (value.entry.ref_count == INT_MAX)
          return std::unexpected(error::reference_overflow);
        ++value.entry.ref_count;
        return reference{value.indexed->second};
      });
}

[[nodiscard]] fn initialize() noexcept -> result<void> {
  return detail::locked<void>(
      [](detail::state &registry) -> result<void> {
        registry.initialized = true;
        return {};
      },
      false);
}

// Borrowed names/data; add starts at one reference.
[[nodiscard]] fn add(key object, void *data) noexcept -> result<void> {
  if (!detail::valid(object))
    return std::unexpected(error::invalid_argument);
  return detail::locked<void>([&](detail::state &registry) -> result<void> {
    let value = std::make_shared<detail::node>(object, data);
    let &bucket = registry.bucket_for(object.type);
    const let[position, inserted] =
        bucket.names.try_emplace(object.name, value);
    if (!inserted)
      return std::unexpected(error::duplicate);
    value->indexed = position;
    let &sequence = bucket.newest;
    try {
      sequence.push_front(value.get());
    } catch (...) {
      bucket.names.erase(position);
      throw;
    }
    value->ordered = sequence.begin();
    return {};
  });
}

[[nodiscard]] fn rename(key object, const char *new_name) noexcept
    -> result<void> {
  if (!new_name)
    return std::unexpected(error::invalid_argument);
  return detail::with_entry<void>(
      object,
      [&](detail::state &registry, detail::node &value) -> result<void> {
        if (value.acting)
          return std::unexpected(error::action_in_progress);
        if (registry.find({new_name, object.type}))
          return std::unexpected(error::duplicate);
        std::string replacement{new_name};
        let &bucket = registry.bucket_for(object.type);
        let indexed = bucket.names.extract(value.indexed);
        indexed.key().swap(replacement);
        const let inserted = bucket.names.insert(std::move(indexed));
        value.indexed = inserted.position;
        value.entry.name = const_cast<char *>(new_name);
        let &sequence = bucket.newest;
        sequence.splice(sequence.begin(), sequence, value.ordered);
        return {};
      });
}

[[nodiscard]] fn acquire(key object) noexcept -> result<void *> {
  return detail::with_entry<void *>(
      object, [](detail::state &, detail::node &value) -> result<void *> {
        if (value.entry.ref_count == INT_MAX)
          return std::unexpected(error::reference_overflow);
        ++value.entry.ref_count;
        return value.entry.data;
      });
}

[[nodiscard]] fn lookup(key object) noexcept -> result<void *> {
  return detail::with_entry<void *>(
      object,
      [](detail::state &, detail::node &value) { return value.entry.data; });
}

[[nodiscard]] fn release(key object) noexcept -> result<void> {
  return detail::with_entry<void>(object,
                                  [](detail::state &, detail::node &value) {
                                    return detail::decrement(value);
                                  });
}

[[nodiscard]] fn erase(key object) noexcept -> result<void> {
  return detail::with_entry<void>(
      object, [](detail::state &registry, detail::node &value) -> result<void> {
        if (value.acting)
          return std::unexpected(error::action_in_progress);
        if (value.entry.ref_count == INT_MIN)
          return std::unexpected(error::reference_overflow);
        if (--value.entry.ref_count <= 0)
          registry.remove(value);
        return {};
      });
}

[[nodiscard]] fn execute(key object, int maximum, bool remove, action callback,
                         void *argument) noexcept -> result<action_result> {
  return detail::with_entry<action_result>(
      object, [&](detail::state &registry, detail::node &value) {
        return detail::run_action(registry, value, maximum, remove, callback,
                                  argument);
      });
}

// Snapshot newest first; skip removed entries.
[[nodiscard]] fn for_each(object_type type, visitor callback,
                          void *argument) noexcept -> result<int> {
  if (!detail::valid(type))
    return std::unexpected(error::invalid_argument);
  return detail::locked<int>([&](detail::state &registry) {
    return registry.visit(type, [&](detail::node &value) {
      const let &entry = value.entry;
      if (callback)
        callback({entry.name, entry.data, type, entry.ref_count}, argument);
      return 1;
    });
  });
}

[[nodiscard]] fn erase_type(object_type type, action destructor,
                            void *argument) noexcept -> result<int> {
  if (!detail::valid(type))
    return std::unexpected(error::invalid_argument);
  return detail::locked<int>([&](detail::state &registry) {
    return registry.visit(type, [&](detail::node &value) {
      if (value.entry.ref_count != 0) {
        std::fprintf(stderr,
                     "registry_delete_type: object \"%s\" (type %d) still "
                     "referenced (count=%d)\n",
                     value.indexed->first.c_str(), static_cast<int>(type),
                     value.entry.ref_count);
        return 0;
      }
      const let removed =
          detail::run_action(registry, value, 0, true, destructor, argument);
      return removed && *removed == action_result::removed ? 1 : 0;
    });
  });
}

fn dump() noexcept -> void {
  const let dumped =
      detail::locked<void>([](detail::state &registry) -> result<void> {
        std::println("Registry dump:\n  Objects (by type/name):");
        for (const let &bucket : registry.types)
          for (const let &[ name, value ] : bucket.names)
            std::println("     {} (type {}, ref_count={})", name,
                         value->entry.object_type, value->entry.ref_count);
        std::println("\n  Objects classed by types:");
        for (std::size_t type = 0; type < registry.types.size(); ++type) {
          std::print("     Type {}: ", type);
          for (const let *value : registry.types[type].newest)
            std::print("{}({}) ", value->indexed->first,
                       value->entry.ref_count);
          std::println("");
        }
        return {};
      });
  if (!dumped) {
    try {
      std::println(stderr, "Registry: dump failed ({}).",
                   static_cast<int>(dumped.error()));
    } catch (...) {
    }
  }
}

} // namespace dynamips::registry

namespace {
namespace reg = dynamips::registry;
fn object_key(const char *name, int type) -> reg::key {
  return {name, static_cast<reg::object_type>(type)};
}

struct legacy_action {
  registry_exec callback;
  void *argument;
};

fn invoke_legacy(void *data, void *context) -> bool {
  const let &action = *static_cast<legacy_action *>(context);
  return !action.callback || action.callback(data, action.argument) != 0;
}
} // namespace

cfn registry_init() -> int { return reg::initialize() ? 0 : -1; }
cfn registry_add(char *name, int type, void *data) -> int {
  return reg::add(object_key(name, type), data) ? 0 : -1;
}
cfn registry_delete(char *name, int type) -> int {
  return reg::erase(object_key(name, type)) ? 0 : -1;
}
cfn registry_rename(char *name, char *new_name, int type) -> int {
  return reg::rename(object_key(name, type), new_name) ? 0 : -1;
}
cfn registry_find(char *name, int type) -> void * {
  return reg::acquire(object_key(name, type)).value_or(nullptr);
}
cfn registry_exists(char *name, int type) -> void * {
  return reg::lookup(object_key(name, type)).value_or(nullptr);
}
cfn registry_unref(char *name, int type) -> int {
  return reg::release(object_key(name, type)) ? 0 : -1;
}
cfn registry_exec_refcount(char *name, int type, int maximum, int remove,
                           registry_exec callback, void *argument) -> int {
  legacy_action context{callback, argument};
  const let result = reg::execute(object_key(name, type), maximum, remove != 0,
                                  invoke_legacy, &context);
  if (!result)
    return -1;
  return *result == reg::action_result::referenced ? 0 : 1;
}
cfn registry_delete_if_unused(char *name, int type, registry_exec destructor,
                              void *argument) -> int {
  return registry_exec_refcount(name, type, 0, 1, destructor, argument);
}
cfn registry_foreach_type(int type, registry_foreach callback, void *argument,
                          int *error) -> int {
  const let category = static_cast<reg::object_type>(type);
  if (!reg::detail::valid(category))
    return -1;
  return reg::detail::locked<int>([&](reg::detail::state &registry) {
           return registry.visit(category, [&](reg::detail::node &value) {
             if (callback)
               callback(&value.entry, argument, error);
             return 1;
           });
         })
      .value_or(-1);
}
cfn registry_delete_type(int type, registry_exec destructor, void *argument)
    -> int {
  legacy_action context{destructor, argument};
  return reg::erase_type(static_cast<reg::object_type>(type), invoke_legacy,
                         &context)
      .value_or(-1);
}
cfn registry_dump() -> void { reg::dump(); }
