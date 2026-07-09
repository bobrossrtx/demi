#include "../config.hpp"
#include "cpu.hpp"  // Must come before device_factory to avoid CR0 macro conflict
#include "device_factory.hpp"
#include "device_manager.hpp"
#include "../debug/debug_handler.hpp"

void initialize_devices(CPU* cpu) {
    using namespace vhw;

    DeviceManager::instance().reset();  // Reset device manager to clear any previous state
    
    // Console on port 0x01
    auto console = DeviceFactory::createConsoleDevice(0x01);  
    
    // Counter on port 0x02
    auto counter = DeviceFactory::createCounterDevice(0x02);
    // Set up initial counter value
    counter->setCounter(42);

    // Create a file device for virtual file I/O
    auto file = DeviceFactory::createFileDevice("virtual_storage/vhd.dat", 0x04);    
    
    // Create a RAM disk device for block storage
    Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_RAMDISK, "About to create RAMDisk...", Logging::DebugLevel::DETAIL);
    auto ramdisk = DeviceFactory::createRamDiskDevice(8192, 0x05, 0x06);
    Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_RAMDISK, "RAMDisk created successfully", Logging::DebugLevel::DETAIL);

    // Timer on port 0x08 for periodic interrupts
    auto timer = DeviceFactory::createTimerDevice(0x08);
    if (cpu) {
        timer->set_interrupt_controller(&cpu->get_interrupt_controller());
        cpu->set_timer_device(timer);
        Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_DEVICE,
            "Timer device wired to CPU interrupt controller", Logging::DebugLevel::INFO);
    }

    Logging::DebugHandler::instance().report(Logging::DebugCategory::IO_DEVICE, "Device system initialized with standard and storage devices", Logging::DebugLevel::INFO);
}
