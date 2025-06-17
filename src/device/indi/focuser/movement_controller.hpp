#ifndef LITHIUM_INDI_FOCUSER_MOVEMENT_CONTROLLER_HPP
#define LITHIUM_INDI_FOCUSER_MOVEMENT_CONTROLLER_HPP

#include "types.hpp"
#include <libindi/baseclient.h>
#include <chrono>

namespace lithium::device::indi::focuser {

/**
 * @brief Controls focuser movement operations
 */
class MovementController : public IFocuserComponent {
public:
    MovementController() = default;
    ~MovementController() override = default;

    bool initialize(FocuserState& state) override;
    void cleanup() override;
    std::string getComponentName() const override { return "MovementController"; }

    // Movement control methods
    bool moveSteps(int steps);
    bool moveToPosition(int position);
    bool moveInward(int steps);
    bool moveOutward(int steps);
    bool moveForDuration(int durationMs);
    bool abortMove();
    bool syncPosition(int position);

    // Speed control
    bool setSpeed(double speed);
    std::optional<double> getSpeed() const;
    int getMaxSpeed() const;
    std::pair<int, int> getSpeedRange() const;

    // Direction control
    bool setDirection(FocusDirection direction);
    std::optional<FocusDirection> getDirection() const;

    // Position queries
    std::optional<int> getPosition() const;
    bool isMoving() const;

    // Limits
    bool setMaxLimit(int maxLimit);
    std::optional<int> getMaxLimit() const;
    bool setMinLimit(int minLimit);
    std::optional<int> getMinLimit() const;

    // Reverse motion
    bool setReversed(bool reversed);
    std::optional<bool> isReversed() const;

private:
    FocuserState* state_{nullptr};
    INDI::BaseClient* client_{nullptr};
    
    // Helper methods
    bool sendPropertyUpdate(const std::string& propertyName, double value);
    bool sendPropertyUpdate(const std::string& propertyName, const std::vector<bool>& states);
    void updateStatistics(int steps);
    
    std::chrono::steady_clock::time_point lastMoveStart_;
};

} // namespace lithium::device::indi::focuser

#endif // LITHIUM_INDI_FOCUSER_MOVEMENT_CONTROLLER_HPP
