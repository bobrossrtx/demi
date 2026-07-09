#pragma once

#include "device_manager.hpp"
#include "devices/console_device.hpp"
#include "devices/counter_device.hpp"
#include "devices/serial_port_device.hpp"
#include "devices/file_device.hpp"
#include "devices/ramdisk_device.hpp"
#include "devices/timer_device.hpp"

#include <memory>

namespace vhw {

/**
 * A factory class to create and register common devices
 */
class DeviceFactory {
public:
    /**
     * Create and register a console device
     * @param port The port to register the device at
     * @return The created device
     */
    static std::shared_ptr<ConsoleDevice> createConsoleDevice(uint8_t port = ConsoleDevice::DEFAULT_PORT) {
        auto device = std::make_shared<ConsoleDevice>();
        DeviceManager::instance().registerDevice(port, device);
        return device;
    }
    
    /**
     * Create and register a counter device
     * @param port The port to register the device at
     * @return The created device
     */
    static std::shared_ptr<CounterDevice> createCounterDevice(uint8_t port = CounterDevice::DEFAULT_PORT) {
        auto device = std::make_shared<CounterDevice>();
        DeviceManager::instance().registerDevice(port, device);
        return device;
    }
    
    /**
     * Create and register a serial port device
     * @param portName The name of the serial port (e.g., "/dev/ttyUSB0" on Linux or "COM1" on Windows)
     * @param port The port to register the device at
     * @param autoConnect Whether to automatically connect to the serial port
     * @return The created device
     */
    static std::shared_ptr<SerialPortDevice> createSerialPortDevice(
        const std::string& portName,
        uint8_t port = SerialPortDevice::DEFAULT_PORT,
        bool autoConnect = true
    ) {
        auto device = std::make_shared<SerialPortDevice>(portName);
        DeviceManager::instance().registerDevice(port, device);
        
        if (autoConnect) {
            device->connect();
        }
        
        return device;
    }
    
    /**
     * Create and register a file device
     * @param filepath The path to the file
     * @param port The port to register the device at
     * @return The created device
     */
    static std::shared_ptr<FileDevice> createFileDevice(
        const std::string& filepath,
        uint8_t port = FileDevice::DEFAULT_PORT
    ) {
        auto device = std::make_shared<FileDevice>(filepath);
        DeviceManager::instance().registerDevice(port, device);
        return device;
    }
    
    /**
     * Create and register a RAM disk device
     * @param sizeInBytes The size of the RAM disk in bytes
     * @param dataPort The port to register the data interface at
     * @param ctrlPort The port to register the control interface at
     * @return The created device (control instance)
     */
    static std::shared_ptr<RamDiskDevice> createRamDiskDevice(
        size_t sizeInBytes = 8192,
        uint8_t dataPort = RamDiskDevice::DEFAULT_DATA_PORT,
        uint8_t ctrlPort = RamDiskDevice::DEFAULT_CTRL_PORT
    ) {
        // Create a single RAM disk instance that will be shared between data and control ports
        auto device = std::make_shared<RamDiskDevice>(sizeInBytes);
        
        // Create a second reference to the same device for the data port
        auto dataDevice = device;
        
        // Set the first instance as the control port
        device->setAsControlPort(true);
        
        // Register both ports
        DeviceManager::instance().registerDevice(ctrlPort, device);
        DeviceManager::instance().registerDevice(dataPort, dataDevice);
        
        return device;  // Return the control instance
    }
    /**
     * Create and register a timer device
     * @param port The port to register the device at
     * @return The created device
     */
    static std::shared_ptr<TimerDevice> createTimerDevice(uint8_t port = TimerDevice::DEFAULT_PORT) {
        auto device = std::make_shared<TimerDevice>();
        DeviceManager::instance().registerDevice(port, device);
        return device;
    }
};

} // namespace vhw
