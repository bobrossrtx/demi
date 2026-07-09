#pragma once

#include "../device.hpp"
#include "../interrupt_controller.hpp"

#include <fmt/format.h>
#include <cstdint>
#include <string>

namespace vhw {

/**
 * Timer device that fires periodic hardware interrupts.
 *
 * Port 0x08 — Timer control/data
 *   Read:  current tick count (low byte), or 0 if disabled
 *   Write: control byte
 *     0x00 = disable timer
 *     0x01 = enable timer (use previously set interval or default)
 *     0x02-0xFE = set interval to (value - 1) ticks (minimum 1, maximum 253)
 *     0xFF = set interval to 255 ticks
 *
 * One tick = one call to tick(), which happens once per instruction.
 * When the counter reaches zero, it fires HardwareInterrupt::TIMER (vector 0x20)
 * via the interrupt controller and resets.
 *
 * The timer is NOT a wall-clock timer — it counts VM instruction cycles.
 * This keeps it deterministic and testable.
 */
class TimerDevice : public VirtualDevice {
public:
    static constexpr uint8_t DEFAULT_PORT = 0x08;
    static constexpr uint8_t DEFAULT_INTERVAL = 10; // 10 instruction ticks

    TimerDevice()
        : enabled_(false)
        , interval_(DEFAULT_INTERVAL)
        , counter_(DEFAULT_INTERVAL)
        , interrupt_controller_(nullptr)
    {}

    ~TimerDevice() override = default;

    /**
     * Set the interrupt controller reference so the timer can fire interrupts.
     * Must be called after construction and before use.
     */
    void set_interrupt_controller(DemiEngine_Interrupts::InterruptController* ic) {
        interrupt_controller_ = ic;
    }

    uint8_t read() override {
        return static_cast<uint8_t>(counter_ & 0xFF);
    }

    void write(uint8_t value) override {
        switch (value) {
        case 0x00:
            enabled_ = false;
            break;
        case 0x01:
            enabled_ = true;
            counter_ = interval_;
            break;
        default:
            // Set interval: value = desired interval (1-255)
            interval_ = value;
            counter_ = value;
            enabled_ = true;
            break;
        }
    }

    std::string getName() const override {
        return "Timer Device";
    }

    void reset() override {
        enabled_ = false;
        interval_ = DEFAULT_INTERVAL;
        counter_ = DEFAULT_INTERVAL;
    }

    /**
     * Called once per VM instruction cycle.
     * Decrements the counter; when it reaches zero, fires a timer interrupt
     * and resets the counter.
     */
    void tick() {
        if (!enabled_) return;
        if (counter_ == 0) return;

        counter_--;

        if (counter_ == 0) {
            // Fire timer interrupt
            if (interrupt_controller_) {
                interrupt_controller_->trigger_hardware_interrupt(
                    DemiEngine_Interrupts::HardwareInterrupt::TIMER);
            }
            // Reset counter for next period
            counter_ = interval_;
        }
    }

    bool is_enabled() const { return enabled_; }
    uint8_t get_interval() const { return interval_; }
    uint8_t get_counter() const { return counter_; }

private:
    bool enabled_;
    uint8_t interval_;
    uint8_t counter_;
    DemiEngine_Interrupts::InterruptController* interrupt_controller_;
};

} // namespace vhw
