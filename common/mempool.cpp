#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>

#include "mempool.h"
#include <dynamips/macros.h>

namespace {

static_assert(offsetof(memblock_t, data) == sizeof(memblock_t));

enum class error { invalid_argument, out_of_memory, system_failure, internal };
template <typename T> using result = std::expected<T, error>;

struct pool_state {
  std::pmr::unsynchronized_pool_resource memory;
};

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

template <typename T, typename Operation>
fn with_pool(mempool_t *pool, Operation operation) noexcept -> result<T> {
  if (!pool || !pool->backend)
    return std::unexpected(error::invalid_argument);
  if (pthread_mutex_lock(&pool->lock) != 0)
    return std::unexpected(error::system_failure);
  let value = attempt<T>(operation);
  if (pthread_mutex_unlock(&pool->lock) != 0)
    return std::unexpected(error::system_failure);
  return value;
}

fn state(mempool_t *pool) -> pool_state & {
  return *static_cast<pool_state *>(pool->backend);
}

fn block_from(void *address) -> memblock_t * {
  return reinterpret_cast<memblock_t *>(static_cast<std::byte *>(address) -
                                        sizeof(memblock_t));
}

fn allocate(mempool_t *pool, std::size_t size, bool zeroed) -> result<void *> {
  if (size > std::numeric_limits<std::size_t>::max() - sizeof(memblock_t) ||
      size > std::numeric_limits<std::size_t>::max() - pool->total_size ||
      pool->nr_blocks == INT_MAX)
    return std::unexpected(error::invalid_argument);

  let *storage = state(pool).memory.allocate(sizeof(memblock_t) + size,
                                             alignof(memblock_t));
  let *block = std::construct_at(static_cast<memblock_t *>(storage));
  block->tag = static_cast<int>(MEMBLOCK_TAG);
  block->block_size = size;
  block->pool = pool;
  block->next = pool->block_list;
  if (block->next)
    block->next->prev = block;
  pool->block_list = block;
  ++pool->nr_blocks;
  pool->total_size += size;
  if (zeroed)
    std::memset(block->data, 0, size);
  return static_cast<void *>(block->data);
}

fn release_block(memblock_t *block) -> void {
  let *pool = block->pool;
  if (block->prev)
    block->prev->next = block->next;
  else
    pool->block_list = block->next;
  if (block->next)
    block->next->prev = block->prev;
  --pool->nr_blocks;
  pool->total_size -= block->block_size;
  const let size = sizeof(memblock_t) + block->block_size;
  block->tag = 0;
  std::destroy_at(block);
  state(pool).memory.deallocate(block, size, alignof(memblock_t));
}

fn valid(memblock_t *block) -> bool {
  return static_cast<unsigned>(block->tag) == MEMBLOCK_TAG;
}

fn release_all(mempool_t *pool) -> result<void> {
  return with_pool<void>(pool, [pool]() -> result<void> {
    state(pool).memory.release();
    pool->block_list = nullptr;
    pool->nr_blocks = 0;
    pool->total_size = 0;
    return {};
  });
}

} // namespace

cfn mp_create_fixed_pool(mempool_t *pool, char *name) -> mempool_t * {
  if (!pool)
    return nullptr;
  return attempt<mempool_t *>([=]() -> result<mempool_t *> {
           let backend = std::make_unique<pool_state>();
           std::memset(pool, 0, sizeof(*pool));
           if (pthread_mutex_init(&pool->lock, nullptr) != 0)
             return std::unexpected(error::system_failure);
           pool->name = name;
           pool->flags = MEMPOOL_FIXED;
           pool->backend = backend.release();
           return pool;
         })
      .value_or(nullptr);
}

cfn mp_create_pool(char *name) -> mempool_t * {
  let *pool = static_cast<mempool_t *>(std::calloc(1, sizeof(mempool_t)));
  if (!pool)
    return nullptr;
  if (!mp_create_fixed_pool(pool, name)) {
    std::free(pool);
    return nullptr;
  }
  pool->flags = 0;
  return pool;
}

cfn mp_alloc(mempool_t *pool, std::size_t size) -> void * {
  return with_pool<void *>(pool, [=] { return allocate(pool, size, true); })
      .value_or(nullptr);
}

cfn mp_alloc_n0(mempool_t *pool, std::size_t size) -> void * {
  return with_pool<void *>(pool, [=] { return allocate(pool, size, false); })
      .value_or(nullptr);
}

cfn mp_free(void *address) -> int {
  if (!address)
    return 0;
  let *block = block_from(address);
  if (!valid(block))
    return -1;
  const let freed = with_pool<void>(block->pool, [block]() -> result<void> {
    release_block(block);
    return {};
  });
  return freed ? 0 : -1;
}

cfn mp_free_ptr(void *address) -> int {
  if (!address)
    return -1;
  void *value;
  std::memcpy(&value, address, sizeof(value));
  const let freed = mp_free(value);
  if (freed == 0) {
    value = nullptr;
    std::memcpy(address, &value, sizeof(value));
  }
  return freed;
}

cfn mp_realloc(void *address, std::size_t size) -> void * {
  if (!address)
    return nullptr;
  let *block = block_from(address);
  if (!valid(block))
    return nullptr;
  let *pool = block->pool;
  return with_pool<void *>(pool,
                           [=]() -> result<void *> {
                             const let replacement =
                                 allocate(pool, size, false);
                             if (!replacement)
                               return replacement;
                             std::memcpy(*replacement, address,
                                         std::min(block->block_size, size));
                             release_block(block);
                             return replacement;
                           })
      .value_or(nullptr);
}

cfn mp_dup(mempool_t *pool, void *data, std::size_t size) -> void * {
  if (!data && size != 0)
    return nullptr;
  let *copy = mp_alloc_n0(pool, size);
  if (copy && size != 0)
    std::memcpy(copy, data, size);
  return copy;
}

cfn mp_strdup(mempool_t *pool, char *string) -> char * {
  if (!string)
    return nullptr;
  const let size = std::strlen(string);
  if (size == std::numeric_limits<std::size_t>::max())
    return nullptr;
  return static_cast<char *>(mp_dup(pool, string, size + 1));
}

cfn mp_free_all_blocks(mempool_t *pool) -> void {
  if (!release_all(pool))
    std::fputs("mp_free_all_blocks: unable to release the pool.\n", stderr);
}

cfn mp_free_pool(mempool_t *pool) -> void {
  if (!pool)
    return;
  if (!release_all(pool)) {
    std::fputs("mp_free_pool: unable to release the pool.\n", stderr);
    return;
  }
  let *backend = static_cast<pool_state *>(pool->backend);
  pool->backend = nullptr;
  delete backend;
  if (pthread_mutex_destroy(&pool->lock) != 0) {
    std::fputs("mp_free_pool: unable to destroy the pool mutex.\n", stderr);
    return;
  }
  if (!(pool->flags & MEMPOOL_FIXED))
    std::free(pool);
}
