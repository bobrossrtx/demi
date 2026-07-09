#pragma once
#include "../cpu.hpp"
#include <vector>
void handle_movsb(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
