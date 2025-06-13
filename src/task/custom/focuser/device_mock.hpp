// mock device::Focuser 和 device::TemperatureSensor 头文件
#pragma once
#include <memory>
#include <string>

namespace device {

class Focuser {
public:
    Focuser() = default;
    virtual ~Focuser() = default;
    virtual int position() const { return 0; }
    virtual void move(int /*steps*/) {}
};

class TemperatureSensor {
public:
    TemperatureSensor() = default;
    virtual ~TemperatureSensor() = default;
    virtual double temperature() const { return 20.0; }
    virtual std::string name() const { return "MockSensor"; }
};

} // namespace device