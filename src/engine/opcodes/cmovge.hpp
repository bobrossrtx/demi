#pragma once
#include "../cpu.hpp"
#include <vector>
void handle_cmovge(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
