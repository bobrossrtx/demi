#pragma once
#include "../cpu.hpp"
#include <vector>
void handle_syscall(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
