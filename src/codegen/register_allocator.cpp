#include "register_allocator.hpp"
#include <cassert>
#include <iostream>

namespace CodeGen {

RegisterAllocator::RegisterAllocator()
    : next_spill_offset(0)
    , lru_counter(0)
    , spill_count(0)
    , allocation_count(0)
{
}

[[maybe_unused]] static bool is_gp_reg(uint8_t virt_reg) {
    return virt_reg < 16;
}

[[maybe_unused]] static bool is_simd_xmm_reg(uint8_t virt_reg) {
    return virt_reg >= 50 && virt_reg <= 81;
}

[[maybe_unused]] static bool is_fpu_st_reg(uint8_t virt_reg) {
    return virt_reg >= 82 && virt_reg <= 97;
}

[[maybe_unused]] static bool is_system_reg(uint8_t virt_reg) {
    return (virt_reg >= 16 && virt_reg <= 49) ||
           (virt_reg >= 98 && virt_reg <= 101) ||
           virt_reg >= 134;
}

X86Register RegisterAllocator::allocate_register(uint8_t virt_reg) {
    auto it = virt_to_phys.find(virt_reg);
    if (it != virt_to_phys.end()) {
        update_lru(virt_reg);
        return it->second;
    }

    auto free_reg = find_free_register();
    if (free_reg) {
        virt_to_phys[virt_reg] = *free_reg;
        used_regs.insert(*free_reg);
        dirty_regs[virt_reg] = false;
        allocation_count++;
        update_lru(virt_reg);
        return *free_reg;
    }

    X86Register evicted = evict_least_recently_used();
    virt_to_phys[virt_reg] = evicted;
    used_regs.insert(evicted);
    dirty_regs[virt_reg] = false;
    allocation_count++;
    update_lru(virt_reg);
    return evicted;
}

void RegisterAllocator::free_register(uint8_t virt_reg) {
    auto it = virt_to_phys.find(virt_reg);
    if (it == virt_to_phys.end()) return;

    used_regs.erase(it->second);
    virt_to_phys.erase(it);
    dirty_regs.erase(virt_reg);
    lru_timestamps.erase(virt_reg);
}

bool RegisterAllocator::is_allocated(uint8_t virt_reg) const {
    return virt_to_phys.find(virt_reg) != virt_to_phys.end();
}

X86Register RegisterAllocator::get_physical_register(uint8_t virt_reg, X86Encoder& encoder) {
    auto it = virt_to_phys.find(virt_reg);
    if (it != virt_to_phys.end()) {
        update_lru(virt_reg);
        return it->second;
    }

    X86Register phys = allocate_register(virt_reg);

    if (spill_slots.find(virt_reg) != spill_slots.end()) {
        reload_register(virt_reg, encoder);
    }

    return phys;
}

void RegisterAllocator::spill_register(uint8_t virt_reg, X86Encoder& encoder) {
    auto it = virt_to_phys.find(virt_reg);
    if (it == virt_to_phys.end()) return;
    if (!dirty_regs[virt_reg]) return;

    if (spill_slots.find(virt_reg) == spill_slots.end()) {
        spill_slots[virt_reg] = allocate_spill_slot();
    }

    int32_t offset = spill_slots[virt_reg];
    encoder.emit_mov_mem_reg(X86Register::RSP, offset, it->second);
    dirty_regs[virt_reg] = false;
    spill_count++;
}

void RegisterAllocator::reload_register(uint8_t virt_reg, X86Encoder& encoder) {
    auto it = virt_to_phys.find(virt_reg);
    if (it == virt_to_phys.end()) return;

    auto spill_it = spill_slots.find(virt_reg);
    if (spill_it == spill_slots.end()) return;

    encoder.emit_mov_reg_mem(it->second, X86Register::RSP, spill_it->second);
}

void RegisterAllocator::spill_all_dirty(X86Encoder& encoder) {
    for (auto& pair : dirty_regs) {
        if (pair.second) {
            spill_register(pair.first, encoder);
        }
    }
}

void RegisterAllocator::mark_dirty(uint8_t virt_reg) {
    dirty_regs[virt_reg] = true;
}

void RegisterAllocator::mark_clean(uint8_t virt_reg) {
    dirty_regs[virt_reg] = false;
}

X86Register RegisterAllocator::evict_least_recently_used() {
    uint64_t oldest_time = UINT64_MAX;
    uint8_t evict_virt = 0;

    for (const auto& pair : virt_to_phys) {
        auto time_it = lru_timestamps.find(pair.first);
        uint64_t time = (time_it != lru_timestamps.end()) ? time_it->second : 0;
        if (time < oldest_time) {
            oldest_time = time;
            evict_virt = pair.first;
        }
    }

    X86Register freed_phys = virt_to_phys[evict_virt];
    used_regs.erase(freed_phys);
    virt_to_phys.erase(evict_virt);
    dirty_regs.erase(evict_virt);
    lru_timestamps.erase(evict_virt);

    return freed_phys;
}

void RegisterAllocator::reset_for_new_function() {
    virt_to_phys.clear();
    used_regs.clear();
    dirty_regs.clear();
    spill_slots.clear();
    lru_timestamps.clear();
    next_spill_offset = 0;
    lru_counter = 0;
    spill_count = 0;
    allocation_count = 0;
}

void RegisterAllocator::save_caller_saved_regs(X86Encoder& encoder) {
    std::vector<uint8_t> to_spill;
    for (auto& pair : virt_to_phys) {
        if (is_caller_saved(pair.second) && dirty_regs[pair.first]) {
            to_spill.push_back(pair.first);
        }
    }
    for (uint8_t virt_reg : to_spill) {
        spill_register(virt_reg, encoder);
    }
}

void RegisterAllocator::restore_caller_saved_regs(X86Encoder& encoder) {
    for (auto& pair : virt_to_phys) {
        if (is_caller_saved(pair.second) && spill_slots.find(pair.first) != spill_slots.end()) {
            reload_register(pair.first, encoder);
        }
    }
}

void RegisterAllocator::print_allocation_state() const {
    std::cout << "RegisterAllocator state:\n";
    std::cout << "  Allocations: " << allocation_count << "\n";
    std::cout << "  Spills: " << spill_count << "\n";
    std::cout << "  Current mappings:\n";
    for (const auto& pair : virt_to_phys) {
        std::cout << "    VM[" << static_cast<int>(pair.first) << "] -> x86[" 
                  << static_cast<int>(pair.second) << "]"
                  << (dirty_regs.at(pair.first) ? " [dirty]" : "")
                  << "\n";
    }
}

std::optional<X86Register> RegisterAllocator::find_free_register() {
    for (X86Register reg : ALLOCATABLE_REGS) {
        // In 32-bit mode, R8-R15 alias with RAX-RDI (no REX prefix)
        if (!is_64bit_ && static_cast<uint8_t>(reg) >= 8) continue;
        if (used_regs.find(reg) == used_regs.end()) {
            return reg;
        }
    }
    return std::nullopt;
}

int32_t RegisterAllocator::allocate_spill_slot() {
    next_spill_offset -= 8;
    return next_spill_offset;
}

void RegisterAllocator::update_lru(uint8_t virt_reg) {
    lru_timestamps[virt_reg] = ++lru_counter;
}

void RegisterAllocator::update_lru_custom(uint8_t virt_reg) {
    if (virt_to_phys.find(virt_reg) != virt_to_phys.end()) {
        update_lru(virt_reg);
    }
}

bool RegisterAllocator::is_caller_saved(X86Register reg) {
    switch (reg) {
        case X86Register::RAX:
        case X86Register::RCX:
        case X86Register::RDX:
        case X86Register::RSI:
        case X86Register::RDI:
        case X86Register::R8:
        case X86Register::R9:
        case X86Register::R10:
        case X86Register::R11:
            return true;
        default:
            return false;
    }
}

} // namespace CodeGen
