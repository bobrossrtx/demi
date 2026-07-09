#pragma once
#include "../cpu.hpp"
#include <vector>
void handle_cmovnc(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
