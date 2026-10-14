#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

#include "mempool.h"
#include <dynamips/macros.h>

cfn mempool_test_record_size(unsigned index) -> std::size_t;

namespace {
using clock = std::chrono::steady_clock;

fn require(bool condition, const char *message) -> void {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

fn rss_kib() -> std::size_t {
  let *file = std::fopen("/proc/self/status", "r");
  require(file != nullptr, "open process memory stats");
  std::size_t rss = 0;
  char line[256];
  while (std::fgets(line, sizeof(line), file))
    if (std::sscanf(line, "VmRSS: %zu kB", &rss) == 1)
      break;
  require(std::fclose(file) == 0, "close process memory stats");
  return rss;
}

fn visit(memblock_t *block, void *argument) -> void {
  let *count = static_cast<unsigned *>(argument);
  require(static_cast<unsigned>(block->tag) == MEMBLOCK_TAG,
          "foreach block metadata");
  ++*count;
}

fn correctness() -> void {
  char name[] = "comparison";
  mempool_t pool{};
  require(mp_create_fixed_pool(&pool, name) == &pool, "fixed pool creation");
  require(pool.flags & MEMPOOL_FIXED, "fixed pool ownership");
  require(mp_free(nullptr) == 0, "free null");
  for (const let size :
       std::array<std::size_t, 9>{0, 1, 7, 16, 56, 128, 1024, 4097, 65536}) {
    let *block = static_cast<unsigned char *>(mp_alloc(&pool, size));
    require(block != nullptr, "allocate");
    require(reinterpret_cast<std::uintptr_t>(block) % alignof(max_align_t) == 0,
            "payload alignment");
    for (std::size_t index = 0; index < size; ++index)
      require(block[index] == 0, "zeroed payload");
    require(pool.nr_blocks == 1 && pool.total_size == size, "accounting");
    require(mp_free(block) == 0, "individual free");
    require(pool.nr_blocks == 0 && pool.total_size == 0, "free accounting");
  }
  let *data = static_cast<unsigned char *>(mp_alloc_n0(&pool, 64));
  require(data != nullptr, "uninitialized allocation");
  std::memset(data, 0x5a, 64);
  data = static_cast<unsigned char *>(mp_realloc(data, 128));
  require(data != nullptr, "grow realloc");
  for (unsigned index = 0; index < 64; ++index)
    require(data[index] == 0x5a, "grow preserves the prefix");
  data = static_cast<unsigned char *>(mp_realloc(data, 13));
  require(data != nullptr, "shrink realloc");
  for (unsigned index = 0; index < 13; ++index)
    require(data[index] == 0x5a, "shrink preserves the prefix");
  require(pool.nr_blocks == 1 && pool.total_size == 13, "realloc accounting");
#ifdef MEMPOOL_TEST_PMR
  require(mp_realloc(data, std::numeric_limits<std::size_t>::max()) == nullptr,
          "realloc overflow rejected");
  require(data[0] == 0x5a && pool.total_size == 13,
          "failed realloc retains the original allocation");
  require(mp_alloc(&pool, std::numeric_limits<std::size_t>::max()) == nullptr,
          "allocation overflow rejected");
#endif
  let *copy = static_cast<unsigned char *>(mp_dup(&pool, data, 13));
  require(copy && std::memcmp(copy, data, 13) == 0, "duplicate bytes");
  char text[] = "registry-name";
  let *string = mp_strdup(&pool, text);
  require(string && std::strcmp(string, text) == 0, "duplicate string");
  require(mp_free_ptr(&string) == 0 && string == nullptr, "free and clear");
  unsigned visited = 0;
  mp_foreach(&pool, visit, &visited);
  require(visited == 2, "foreach live blocks");
  mp_free_all_blocks(&pool);
  require(!pool.block_list && pool.nr_blocks == 0 && pool.total_size == 0,
          "bulk release accounting");
  require(mp_alloc(&pool, 32) != nullptr, "reuse after bulk release");
  mp_free_pool(&pool);
  let *dynamic = mp_create_pool(name);
  require(dynamic != nullptr && !(dynamic->flags & MEMPOOL_FIXED),
          "dynamic pool creation");
  require(mp_alloc(dynamic, 123) != nullptr, "dynamic pool allocation");
  mp_free_pool(dynamic);
}

fn concurrent() -> void {
  char name[] = "shared";
  mempool_t pool{};
  require(mp_create_fixed_pool(&pool, name) != nullptr, "shared pool creation");
  const let start = clock::now();
  {
    std::vector<std::jthread> workers;
    for (unsigned worker = 0; worker < 4; ++worker)
      workers.emplace_back([&pool, worker] {
        for (unsigned index = 0; index < 10000; ++index) {
          const let size = mempool_test_record_size(index);
          let *data = static_cast<unsigned char *>(mp_alloc(&pool, size));
          require(data != nullptr, "shared allocation");
          std::memset(data, static_cast<int>(worker + 1), size);
          require(data[size - 1] == worker + 1, "shared payload ownership");
          require(mp_free(data) == 0, "shared free");
        }
      });
  }
  const let elapsed = clock::now() - start;
  require(pool.nr_blocks == 0 && pool.total_size == 0, "shared accounting");
  void *remote = nullptr;
  {
    std::jthread producer{[&] { remote = mp_alloc(&pool, 56); }};
  }
  require(remote != nullptr, "cross-thread allocation");
  {
    std::jthread consumer{
        [&] { require(mp_free(remote) == 0, "cross-thread free"); }};
  }
  mp_free_pool(&pool);
  std::printf("%s: shared_4_threads=%.1f ns/op\n", MEMPOOL_IMPLEMENTATION,
              std::chrono::duration<double, std::nano>(elapsed).count() /
                  80000);
}

fn benchmark() -> void {
  char name[] = "churn";
  mempool_t pool{};
  require(mp_create_fixed_pool(&pool, name) != nullptr, "benchmark pool");
  constexpr unsigned count = 8192, rounds = 64;
  std::vector<void *> blocks(count);
  const let start = clock::now();
  for (unsigned round = 0; round < rounds; ++round) {
    for (unsigned index = 0; index < count; ++index) {
      blocks[index] = mp_alloc(&pool, mempool_test_record_size(index));
      require(blocks[index] != nullptr, "benchmark allocate");
    }
    for (unsigned index = 0; index < count; ++index)
      require(mp_free(blocks[(index * 4051) % count]) == 0, "benchmark free");
  }
  const let elapsed = clock::now() - start;
  std::printf("%s: record_sizes=%zu/%zu/%zu/%zu churn=%.1f ns/op\n",
              MEMPOOL_IMPLEMENTATION, mempool_test_record_size(0),
              mempool_test_record_size(1), mempool_test_record_size(2),
              mempool_test_record_size(3),
              std::chrono::duration<double, std::nano>(elapsed).count() /
                  (2.0 * count * rounds));
  mp_free_pool(&pool);
}

fn memory_and_release() -> void {
  char name[] = "memory";
  mempool_t pool{};
  require(mp_create_fixed_pool(&pool, name) != nullptr, "memory pool");
  std::vector<void *> blocks(65536);
  const let before = rss_kib();
  for (unsigned index = 0; index < blocks.size(); ++index) {
    blocks[index] = mp_alloc(&pool, mempool_test_record_size(index));
    require(blocks[index] != nullptr, "memory allocation");
  }
  const let live = rss_kib();
  for (let *block : blocks)
    require(mp_free(block) == 0, "memory free");
  const let retained = rss_kib();
  mp_free_all_blocks(&pool);
  const let released = rss_kib();
  for (unsigned index = 0; index < blocks.size(); ++index)
    require(mp_alloc(&pool, mempool_test_record_size(index)) != nullptr,
            "bulk allocation");
  const let start = clock::now();
  mp_free_all_blocks(&pool);
  const let elapsed = clock::now() - start;
  std::printf("%s: RSS_KiB before=%zu live=%zu individually_freed=%zu "
              "released=%zu bulk_release=%.3f ms\n",
              MEMPOOL_IMPLEMENTATION, before, live, retained, released,
              std::chrono::duration<double, std::milli>(elapsed).count());
  mp_free_pool(&pool);
}
} // namespace

fn main() -> int {
  correctness();
  benchmark();
  memory_and_release();
  concurrent();
  std::puts("PASS: ownership, zeroing, alignment, realloc, duplication, "
            "iteration, bulk reuse and concurrent callers");
}
