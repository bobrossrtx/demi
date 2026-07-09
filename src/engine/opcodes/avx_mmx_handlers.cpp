#include "avx_mmx_handlers.hpp"
#include "../cpu.hpp"
#include "../../debug/debug_handler.hpp"
#include "../cpu_registers.hpp"
#include <algorithm>
#include <cmath>

using namespace DemiEngine_Registers;

struct Vec4 { uint32_t e[4]; };
static Vec4 read_vec(CPU& cpu) { auto& r=cpu.get_registers(); Vec4 v; v.e[0]=r[0];v.e[1]=r[1];v.e[2]=r[2];v.e[3]=r[3]; return v; }
static Vec4 read_src(CPU& cpu) { auto& r=cpu.get_registers(); Vec4 v; v.e[0]=r[4];v.e[1]=r[5];v.e[2]=r[6];v.e[3]=r[7]; return v; }
static void write_vec(CPU& cpu, const Vec4& v) { auto& r=cpu.get_registers(); r[0]=v.e[0];r[1]=v.e[1];r[2]=v.e[2];r[3]=v.e[3]; }

#define VEC(a,b,c,d) do { Vec4 _v; _v.e[0]=(a);_v.e[1]=(b);_v.e[2]=(c);_v.e[3]=(d); write_vec(cpu,_v); } while(0)

void handle_VADDPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]+b.e[0],a.e[1]+b.e[1],a.e[2]+b.e[2],a.e[3]+b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VSUBPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]-b.e[0],a.e[1]-b.e[1],a.e[2]-b.e[2],a.e[3]-b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VMULPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]*b.e[0],a.e[1]*b.e[1],a.e[2]*b.e[2],a.e[3]*b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VDIVPS(CPU& cpu, const std::vector<uint8_t>&, bool& r) { auto a=read_vec(cpu),b=read_src(cpu); if(b.e[0]==0||b.e[1]==0||b.e[2]==0||b.e[3]==0){r=false;return;} VEC(a.e[0]/b.e[0],a.e[1]/b.e[1],a.e[2]/b.e[2],a.e[3]/b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VSQRTPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu); VEC((uint32_t)sqrtf((float)a.e[0]),(uint32_t)sqrtf((float)a.e[1]),(uint32_t)sqrtf((float)a.e[2]),(uint32_t)sqrtf((float)a.e[3])); cpu.set_pc(cpu.get_pc()+1); }
void handle_VMAXPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(std::max(a.e[0],b.e[0]),std::max(a.e[1],b.e[1]),std::max(a.e[2],b.e[2]),std::max(a.e[3],b.e[3])); cpu.set_pc(cpu.get_pc()+1); }
void handle_VMINPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(std::min(a.e[0],b.e[0]),std::min(a.e[1],b.e[1]),std::min(a.e[2],b.e[2]),std::min(a.e[3],b.e[3])); cpu.set_pc(cpu.get_pc()+1); }
void handle_VANDPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]&b.e[0],a.e[1]&b.e[1],a.e[2]&b.e[2],a.e[3]&b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VORPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]|b.e[0],a.e[1]|b.e[1],a.e[2]|b.e[2],a.e[3]|b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VXORPS(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]^b.e[0],a.e[1]^b.e[1],a.e[2]^b.e[2],a.e[3]^b.e[3]); cpu.set_pc(cpu.get_pc()+1); }

void handle_VADDPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]+b.e[0],a.e[1]+b.e[1],a.e[2]+b.e[2],a.e[3]+b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VSUBPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]-b.e[0],a.e[1]-b.e[1],a.e[2]-b.e[2],a.e[3]-b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VMULPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]*b.e[0],a.e[1]*b.e[1],a.e[2]*b.e[2],a.e[3]*b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VDIVPD(CPU& cpu, const std::vector<uint8_t>&, bool& r) { auto a=read_vec(cpu),b=read_src(cpu); if(b.e[0]==0||b.e[1]==0||b.e[2]==0||b.e[3]==0){r=false;return;} VEC(a.e[0]/b.e[0],a.e[1]/b.e[1],a.e[2]/b.e[2],a.e[3]/b.e[3]); cpu.set_pc(cpu.get_pc()+1); }
void handle_VSQRTPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu); VEC((uint32_t)sqrt((double)a.e[0]),(uint32_t)sqrt((double)a.e[1]),(uint32_t)sqrt((double)a.e[2]),(uint32_t)sqrt((double)a.e[3])); cpu.set_pc(cpu.get_pc()+1); }
void handle_VMAXPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(std::max(a.e[0],b.e[0]),std::max(a.e[1],b.e[1]),std::max(a.e[2],b.e[2]),std::max(a.e[3],b.e[3])); cpu.set_pc(cpu.get_pc()+1); }
void handle_VMINPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(std::min(a.e[0],b.e[0]),std::min(a.e[1],b.e[1]),std::min(a.e[2],b.e[2]),std::min(a.e[3],b.e[3])); cpu.set_pc(cpu.get_pc()+1); }
void handle_VANDPD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto a=read_vec(cpu),b=read_src(cpu); VEC(a.e[0]&b.e[0],a.e[1]&b.e[1],a.e[2]&b.e[2],a.e[3]&b.e[3]); cpu.set_pc(cpu.get_pc()+1); }

void handle_MMX_ADD(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto& r=cpu.get_registers(); r[0]+=r[4]; r[1]+=r[5]; cpu.set_pc(cpu.get_pc()+1); }
void handle_MMX_SUB(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto& r=cpu.get_registers(); r[0]-=r[4]; r[1]-=r[5]; cpu.set_pc(cpu.get_pc()+1); }
void handle_MMX_MUL(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto& r=cpu.get_registers(); r[0]*=r[4]; r[1]*=r[5]; cpu.set_pc(cpu.get_pc()+1); }
void handle_MMX_AND(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto& r=cpu.get_registers(); r[0]&=r[4]; r[1]&=r[5]; cpu.set_pc(cpu.get_pc()+1); }
void handle_MMX_OR(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto& r=cpu.get_registers(); r[0]|=r[4]; r[1]|=r[5]; cpu.set_pc(cpu.get_pc()+1); }
void handle_MMX_XOR(CPU& cpu, const std::vector<uint8_t>&, bool&) { auto& r=cpu.get_registers(); r[0]^=r[4]; r[1]^=r[5]; cpu.set_pc(cpu.get_pc()+1); }
