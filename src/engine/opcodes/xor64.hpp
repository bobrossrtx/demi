#pragma once
#include "../cpu.hpp"
#include <cstdint>
#include <vector>

void handle_xor64(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
