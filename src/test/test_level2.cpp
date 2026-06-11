#include <iostream>
#include <sstream>
#include <functional>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include "../src/assembler/lexer.hpp"
#include "../src/assembler/parser.hpp"
#include "../src/assembler/lowering.hpp"
#include "../src/assembler/x86_backend.hpp"

[[noreturn]] void fail(const std::string& msg) { throw std::runtime_error(msg); }
std::string err_str(const std::vector<std::string>& e) { std::ostringstream o; for(auto&s:e)o<<"\n  "<<s; return o.str(); }

uint32_t r32(const std::vector<uint8_t>&b,size_t o){return (uint32_t)b[o]|((uint32_t)b[o+1]<<8)|((uint32_t)b[o+2]<<16)|((uint32_t)b[o+3]<<24);}
uint16_t r16(const std::vector<uint8_t>&b,size_t o){return (uint16_t)b[o]|((uint16_t)b[o+1]<<8);}

auto parse(const std::string& src) {
    Assembler::Lexer l(src); auto t=l.tokenize(); if(l.has_errors())fail("lex:"+l.get_errors()[0]);
    Assembler::Parser p(t); auto a=p.parse(); if(p.has_errors())fail("parse:"+p.get_errors()[0]);
    return a;
}
std::vector<uint8_t> text_section(const std::vector<uint8_t>& elf) {
    uint32_t shoff=r32(elf,32); uint16_t shnum=r16(elf,48),shstrndx=r16(elf,50);
    for(uint16_t i=0;i<shnum;i++){size_t b=shoff+i*40;uint32_t no=r32(elf,b),so=r32(elf,b+16),ss=r32(elf,b+20);
        uint32_t sso=r32(elf,shoff+shstrndx*40+16);std::string n;for(size_t j=sso+no;j<elf.size()&&elf[j];j++) n+=(char)elf[j];
        if(n==".text"&&ss>0)return std::vector<uint8_t>(elf.begin()+so,elf.begin()+so+ss);}
    return {};
}
auto assemble32(const std::string& src) {
    auto ast=parse(src); auto low=Assembler::lower_program(*ast);
    low.target=Assembler::IRTarget::X86Elf32;
    Assembler::X86Backend be(Assembler::X86BackendMode::X86_32);
    auto a=be.emit(low); if(!a.ok())fail(err_str(a.errors));
    return a;
}
void check(const std::vector<uint8_t>& t,size_t o,uint8_t v,const char* n){
    if(o>=t.size()||t[o]!=v){char buf[128];snprintf(buf,sizeof(buf),"%s[%zu] expected %02X got %02X",n,o,v,o<t.size()?t[o]:0);fail(buf);}
}

struct Test{const char* name;void(*fn)();};
Test g_tests[50]; int g_count=0;
#define T(name) void t_##name(); static struct R_##name { R_##name(){ g_tests[g_count++]={#name,t_##name}; } } r_##name; void t_##name()

T(rotates_imm8) {
    auto a=assemble32(".text\n_start:\nROL EAX,1\nROR EBX,CL\nRCL ECX,3\nRCR EDX,CL\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0xC1,"ROL"); check(t,1,0x00,"ROL/modrm"); check(t,2,0x01,"ROL/imm8");
    check(t,3,0xD3,"ROR"); check(t,4,0x0B,"ROR/modrm");
    check(t,5,0xC1,"RCL"); check(t,6,0x11,"RCL/modrm"); check(t,7,0x03,"RCL/imm8");
    check(t,8,0xD3,"RCR"); check(t,9,0x1A,"RCR/modrm");
}

T(idiv) {
    auto a=assemble32(".text\n_start:\nIDIV EBX\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0xF7,"IDIV"); check(t,1,0xFB,"IDIV/modrm");
}

T(xchg_bswap) {
    auto a=assemble32(".text\n_start:\nXCHG EAX,EBX\nBSWAP ECX\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0x87,"XCHG"); check(t,1,0xD8,"XCHG/modrm");
    check(t,2,0x0F,"BSWAP/0F"); check(t,3,0xC9,"BSWAP/reg");
}

T(zero_operand_flags) {
    auto a=assemble32(".text\n_start:\nCLC\nSTC\nCMC\nCLD\nSTD\nLAHF\nSAHF\nCBW\nCWD\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0xF8,"CLC"); check(t,1,0xF9,"STC"); check(t,2,0xF5,"CMC");
    check(t,3,0xFC,"CLD"); check(t,4,0xFD,"STD"); check(t,5,0x9F,"LAHF");
    check(t,6,0x9E,"SAHF"); check(t,7,0x98,"CBW"); check(t,8,0x99,"CWD");
}

T(enter) {
    auto a=assemble32(".text\n_start:\nENTER 256,0\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0xC8,"ENTER"); check(t,1,0x00,"ENTER/lo"); check(t,2,0x01,"ENTER/hi"); check(t,3,0x00,"ENTER/nest");
}

T(bit_test) {
    auto a=assemble32(".text\n_start:\nBT EAX,EBX\nBTS EAX,ECX\nBTR EDX,EBX\nBTC ECX,EDX\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0x0F,"BT/0F"); check(t,1,0xA3,"BT/op");
    check(t,3,0x0F,"BTS/0F"); check(t,4,0xAB,"BTS/op");
    check(t,6,0x0F,"BTR/0F"); check(t,7,0xB3,"BTR/op");
    check(t,9,0x0F,"BTC/0F"); check(t,10,0xBB,"BTC/op");
}

T(cmpxchg_xadd) {
    auto a=assemble32(".text\n_start:\nCMPXCHG EAX,EBX\nXADD EAX,ECX\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0x0F,"CMPXCHG/0F"); check(t,1,0xB1,"CMPXCHG/op");
    check(t,3,0x0F,"XADD/0F"); check(t,4,0xC1,"XADD/op");
}

T(cmovcc) {
    auto a=assemble32(".text\n_start:\nCMOVZ EAX,EBX\nCMOVNZ ECX,EDX\nCMOVG EAX,ECX\nCMOVL EBX,EDX\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0x0F,"CMOVZ/0F"); check(t,1,0x44,"CMOVZ/44");
    check(t,3,0x0F,"CMOVNZ/0F"); check(t,4,0x45,"CMOVNZ/45");
    check(t,6,0x0F,"CMOVG/0F"); check(t,7,0x4F,"CMOVG/4F");
    check(t,9,0x0F,"CMOVL/0F"); check(t,10,0x4C,"CMOVL/4C");
}

T(string_ops) {
    auto a=assemble32(".text\n_start:\nMOVSB\nMOVSD\nSTOSB\nSTOSD\nLODSB\nLODSD\nREP\nMOVSB\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0xA4,"MOVSB"); check(t,1,0xA5,"MOVSD");
    check(t,2,0xAA,"STOSB"); check(t,3,0xAB,"STOSD");
    check(t,4,0xAC,"LODSB"); check(t,5,0xAD,"LODSD");
    check(t,6,0xF3,"REP"); check(t,7,0xA4,"MOVSB2");
}

T(system_ops) {
    auto a=assemble32(".text\n_start:\nCPUID\nRDTSC\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0x0F,"CPUID/0F"); check(t,1,0xA2,"CPUID/A2");
    check(t,2,0x0F,"RDTSC/0F"); check(t,3,0x31,"RDTSC/31");
}

T(sysex) {
    auto a=assemble32(".text\n_start:\nSYSCALL\nRET\n");
    auto t=text_section(a.bytes);
    check(t,0,0x0F,"SYSCALL/0F"); check(t,1,0x05,"SYSCALL/05");
}

int main() {
    int p=0;
    for(int i=0;i<g_count;i++){
        try{g_tests[i].fn(); std::cout<<"[PASS] "<<g_tests[i].name<<"\n"; p++;}
        catch(std::exception& e){std::cout<<"[FAIL] "<<g_tests[i].name<<"\n  "<<e.what()<<"\n";}
    }
    std::cout<<"\nLevel 2 unit tests: "<<p<<"/"<<g_count<<" passed\n";
    return p==g_count?0:1;
}
