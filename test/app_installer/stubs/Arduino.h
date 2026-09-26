#pragma once

#include <cstdint>

struct FakeEsp {
  uint32_t getFreeHeap() const { return 100000; }
  uint32_t getMaxAllocHeap() const { return 80000; }
};

inline FakeEsp ESP;
