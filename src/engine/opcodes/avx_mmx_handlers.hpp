#pragma once
#include "../cpu.hpp"
#include <cstdint>
#include <vector>

// AVX Packed Single handlers (0xC0-0xC9)
void handle_VADDPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VSUBPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VMULPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VDIVPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VSQRTPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VMAXPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VMINPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VANDPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VORPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VXORPS(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// AVX Packed Double handlers (0xCA-0xDD)
void handle_VADDPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VSUBPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VMULPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VDIVPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VSQRTPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VMAXPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VMINPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_VANDPD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);

// MMX handlers (0xE0-0xEA)
void handle_MMX_ADD(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_MMX_SUB(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_MMX_MUL(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_MMX_AND(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_MMX_OR(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
void handle_MMX_XOR(CPU& cpu, const std::vector<uint8_t>& program, bool& running);
