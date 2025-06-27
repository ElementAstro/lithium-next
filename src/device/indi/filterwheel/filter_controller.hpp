#ifndef LITHIUM_INDI_FILTERWHEEL_FILTER_CONTROLLER_HPP
#define LITHIUM_INDI_FILTERWHEEL_FILTER_CONTROLLER_HPP

#include "component_base.hpp"
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <chrono>
#include <optional>

namespace lithium::device::indi::filterwheel {

/**
 * @brief Controls filter selection and movement for INDI FilterWheel
 * 
 * This component handles all filter wheel movement operations, including
 * position changes, validation, and movement state tracking.
 */
class FilterController : public FilterWheelComponentBase {
public:
    explicit FilterController(std::shared_ptr<INDIFilterWheelCore> core);
    ~FilterController() override = default;

    bool initialize() override;
    void shutdown() override;
    std::string getComponentName() const override { return "FilterController"; }

    // Filter control methods
    bool setPosition(int position);
    std::optional<int> getPosition() const;
    bool isMoving() const;
    bool abortMove();

    // Filter information
    int getMaxPosition() const;
    int getMinPosition() const;
    std::vector<std::string> getFilterNames() const;
    std::optional<std::string> getFilterName(int position) const;
    bool setFilterName(int position, const std::string& name);

    // Status checking
    bool isValidPosition(int position) const;
    std::chrono::milliseconds getLastMoveDuration() const;

private:
    bool sendFilterChangeCommand(int position);
    void recordMoveStart();
    void recordMoveEnd();

    bool initialized_{false};
    std::chrono::steady_clock::time_point moveStartTime_;
    std::chrono::milliseconds lastMoveDuration_{0};
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_FILTER_CONTROLLER_HPP
