#pragma once
#include <cstdint>

// CPU Flag definitions
constexpr uint32_t FLAG_ZERO = 1 << 0;
constexpr uint32_t FLAG_SIGN = 1 << 1;
constexpr uint32_t FLAG_CARRY = 1 << 2;
constexpr uint32_t FLAG_OVERFLOW = 1 << 3;
constexpr uint32_t FLAG_DIRECTION = 1 << 4;
constexpr uint32_t FLAG_INTERRUPT = 1 << 5;
