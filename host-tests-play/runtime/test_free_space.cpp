#include <SDCardManager.h>

#include <cassert>
#include <cstdio>

int main() {
  SDCardManager storage;
  uint64_t freeBytes = 12345;
  assert(!storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 12345);
  assert(storage.begin());

  fakeSd::clusters = 2000000;
  fakeSd::clusterBytes = 32768;
  fakeSd::freeClusters = 1500000;
  assert(storage.sdFreeBytes(freeBytes));
  assert(freeBytes == uint64_t(1500000) * 32768);  // No 32-bit overflow.

  // Unlike usedBytes(), this query cannot return a previously cached value.
  fakeSd::freeClusters = 2;
  assert(storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 65536);
  fakeSd::freeClusters = 0;
  assert(storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 0);  // Full is a successful query.

  freeBytes = 12345;
  fakeSd::freeClusters = -1;
  assert(!storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 12345);  // A failed FAT read does not mean an empty card.
  fakeSd::freeClusters = 2000001;
  assert(!storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 12345);
  fakeSd::freeClusters = 1;
  fakeSd::clusterBytes = 0;
  assert(!storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 12345);
  fakeSd::clusterBytes = 32768;
  fakeSd::clusters = 0;
  assert(!storage.sdFreeBytes(freeBytes));
  assert(freeBytes == 12345);
  std::puts("Play free-space query: mounted, full, I/O error, fresh count and 64-bit cases passed");
}
