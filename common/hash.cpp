#include <cstddef>
#include <expected>
#include <new>
#include <unordered_map>

#include "hash.h"
#include <dynamips/macros.h>

namespace {
struct key_hash {
  hash_fcompute compute;

  fn operator()(void *key) const->std::size_t { return compute(key); }
};

struct key_equal {
  hash_fcompare compare;

  fn operator()(void *left, void *right) const->bool {
    return compare(left, right) != 0;
  }
};

enum class error { out_of_memory, internal };

template <typename T> using result = std::expected<T, error>;

template <typename T, typename Operation>
fn attempt(Operation operation) noexcept -> result<T> {
  try {
    return operation();
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

} // namespace

struct hash_table {
  std::unordered_map<void *, void *, key_hash, key_equal> entries;

  hash_table(hash_fcompute compute, hash_fcompare compare, int size)
      : entries(static_cast<std::size_t>(size), key_hash{compute},
                key_equal{compare}) {}
};

cfn str_hash(void *string) -> unsigned {
  unsigned hash = 0;
  for (const char *text = static_cast<const char *>(string); *text != '\0';
       ++text) {
    hash = (hash << 4) + static_cast<unsigned>(*text);
    const let high = hash & 0xf0000000U;
    if (high != 0) {
      hash ^= high >> 24;
      hash ^= high;
    }
  }
  return hash;
}

cfn hash_table_create(hash_fcompute compute, hash_fcompare compare, int size)
    -> hash_table_t * {
  if (!compute || !compare || size <= 0)
    return nullptr;

  return attempt<hash_table_t *>([=]() -> hash_table_t * {
           return new hash_table{compute, compare, size};
         })
      .value_or(nullptr);
}

cfn hash_table_delete(hash_table_t *table) -> void { delete table; }

cfn hash_table_insert(hash_table_t *table, void *key, void *value) -> int {
  if (!table)
    return -1;

  const let inserted = attempt<void>([&]() -> result<void> {
    table->entries.insert_or_assign(key, value);
    return {};
  });

  return inserted ? 0 : -1;
}

cfn hash_table_lookup(hash_table_t *table, void *key) -> void * {
  if (!table)
    return nullptr;

  return attempt<void *>([&]() -> void * {
           const let found = table->entries.find(key);
           return found == table->entries.end() ? nullptr : found->second;
         })
      .value_or(nullptr);
}

cfn hash_table_lookup_dcmp(hash_table_t *table, void *key) -> void * {
  if (!table)
    return nullptr;

  return attempt<void *>([&]() -> void * {
           const let found = table->entries.find(key);
           return found != table->entries.end() && found->first == key
                      ? found->second
                      : nullptr;
         })
      .value_or(nullptr);
}

cfn hash_table_remove(hash_table_t *table, void *key) -> void * {
  if (!table)
    return nullptr;

  return attempt<void *>([&]() -> void * {
           const let found = table->entries.find(key);
           if (found == table->entries.end())
             return nullptr;

           let *value = found->second;
           table->entries.erase(found);
           return value;
         })
      .value_or(nullptr);
}

cfn hash_table_foreach(hash_table_t *table, hash_fforeach callback,
                       void *argument) -> int {
  if (!table || !callback)
    return -1;

  const let visited = attempt<void>([&]() -> result<void> {
    for (const let &[ key, value ] : table->entries)
      callback(key, value, argument);
    return {};
  });

  return visited ? 0 : -1;
}
