#pragma once
#include "../cpu.hpp"
#include <vector>
void handle_stosb(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
