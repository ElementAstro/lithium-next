#ifndef LITHIUM_INDI_FOCUSER_PRESET_MANAGER_HPP
#define LITHIUM_INDI_FOCUSER_PRESET_MANAGER_HPP

#include "types.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Manages preset positions for the focuser.
 *
 * This class provides interfaces for saving, loading, deleting, and querying
 * preset positions for the focuser. Presets allow users to quickly move the
 * focuser to predefined positions, improving efficiency and repeatability in
 * astrophotography workflows.
 */
class PresetManager : public IFocuserComponent {
public:
    /**
     * @brief Default constructor.
     */
    PresetManager() = default;
    /**
     * @brief Virtual destructor.
     */
    ~PresetManager() override = default;

    /**
     * @brief Initialize the preset manager with the shared focuser state.
     * @param state Reference to the shared FocuserState structure.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize(FocuserState& state) override;

    /**
     * @brief Cleanup resources and detach from the focuser state.
     */
    void cleanup() override;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    std::string getComponentName() const override { return "PresetManager"; }

    // Preset management

    /**
     * @brief Save a preset position to the specified slot.
     * @param slot The preset slot index to save to.
     * @param position The focuser position to save.
     * @return true if the preset was saved successfully, false otherwise.
     */
    bool savePreset(int slot, int position);

    /**
     * @brief Load a preset position from the specified slot.
     * @param slot The preset slot index to load from.
     * @return true if the preset was loaded successfully, false otherwise.
     */
    bool loadPreset(int slot);

    /**
     * @brief Get the preset value at the specified slot.
     * @param slot The preset slot index to query.
     * @return Optional value containing the preset position, or std::nullopt if
     * empty.
     */
    std::optional<int> getPreset(int slot) const;

    /**
     * @brief Delete the preset at the specified slot.
     * @param slot The preset slot index to delete.
     * @return true if the preset was deleted successfully, false otherwise.
     */
    bool deletePreset(int slot);

    // Preset operations

    /**
     * @brief Get a list of all used preset slots.
     * @return Vector of slot indices that currently have presets.
     */
    std::vector<int> getUsedSlots() const;

    /**
     * @brief Get the number of available (empty) preset slots.
     * @return Number of available slots.
     */
    int getAvailableSlots() const;

    /**
     * @brief Check if a preset exists at the specified slot.
     * @param slot The preset slot index to check.
     * @return true if a preset exists, false otherwise.
     */
    bool hasPreset(int slot) const;

    // Preset utilities

    /**
     * @brief Save the current focuser position as a preset in the specified
     * slot.
     * @param slot The preset slot index to save to.
     * @return true if the current position was saved successfully, false
     * otherwise.
     */
    bool saveCurrentPosition(int slot);

    /**
     * @brief Find the nearest preset slot to a given position within a
     * tolerance.
     * @param position The target position to search near.
     * @param tolerance The maximum allowed distance (default: 50).
     * @return Optional value containing the nearest slot index, or std::nullopt
     * if none found.
     */
    std::optional<int> findNearestPreset(int position,
                                         int tolerance = 50) const;

private:
    /**
     * @brief Pointer to the shared focuser state structure.
     */
    FocuserState* state_{nullptr};

    /**
     * @brief Check if the given slot index is valid for the preset array.
     * @param slot The slot index to check.
     * @return true if the slot is valid, false otherwise.
     */
    bool isValidSlot(int slot) const;
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_PRESET_MANAGER_HPP
