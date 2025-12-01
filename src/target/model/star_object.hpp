// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LITHIUM_TARGET_MODEL_STAR_OBJECT_HPP
#define LITHIUM_TARGET_MODEL_STAR_OBJECT_HPP

#include <string>
#include <vector>

#include "atom/macro.hpp"
#include "atom/type/expected.hpp"
#include "atom/type/json_fwd.hpp"

#include "celestial_object.hpp"

namespace lithium::target::model {

/**
 * @brief Represents a star object with reference to CelestialObject data
 *
 * This class provides additional metadata like alternative names (aliases)
 * and usage statistics (click count) on top of the celestial object data.
 * It combines catalog information with user engagement metrics.
 */
class StarObject {
private:
    std::string name_;                  ///< Primary name of the star
    std::vector<std::string> aliases_;  ///< Alternative names
    int clickCount_;                    ///< Usage count for popularity ranking
    CelestialObject celestialObject_;   ///< Associated celestial data

public:
    /**
     * @brief Constructs a star object with name and aliases
     *
     * @param name Primary name of the star
     * @param aliases Alternative names for the star
     * @param clickCount Usage count, defaults to 0
     */
    StarObject(std::string name, std::vector<std::string> aliases,
               int clickCount = 0);

    /**
     * @brief Default constructor
     */
    StarObject() : name_(""), aliases_({}), clickCount_(0) {}

    /**
     * @brief Get the primary name of the star
     *
     * @return Star's primary name
     */
    [[nodiscard]] const std::string& getName() const;

    /**
     * @brief Get all alternative names (aliases) of the star
     *
     * @return Vector of alias strings
     */
    [[nodiscard]] const std::vector<std::string>& getAliases() const;

    /**
     * @brief Get the popularity count of the star
     *
     * @return Click count integer
     */
    [[nodiscard]] int getClickCount() const;

    /**
     * @brief Check if a given name matches this star
     *
     * Checks both primary name and aliases for exact match.
     *
     * @param name Name to check
     * @return true if name matches
     */
    [[nodiscard]] bool matchesName(const std::string& name) const;

    /**
     * @brief Check if name matches with fuzzy matching
     *
     * @param name Name to check
     * @param maxEditDistance Maximum allowed edit distance
     * @return Edit distance if matches, nullopt otherwise
     */
    [[nodiscard]] auto fuzzyMatchName(const std::string& name,
                                     int maxEditDistance = 2) const
        -> std::optional<int>;

    /**
     * @brief Set the primary name of the star
     *
     * @param name New primary name
     * @return true if successful
     */
    [[nodiscard]] auto setName(const std::string& name)
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Set all alternative names (aliases) of the star
     *
     * @param aliases New vector of aliases
     * @return true if successful
     */
    [[nodiscard]] auto setAliases(const std::vector<std::string>& aliases)
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Add a single alias
     *
     * @param alias Alias to add
     * @return true if successful, false if already exists
     */
    [[nodiscard]] auto addAlias(const std::string& alias)
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Remove an alias
     *
     * @param alias Alias to remove
     * @return true if removed, false if not found
     */
    [[nodiscard]] bool removeAlias(const std::string& alias);

    /**
     * @brief Set the popularity count of the star
     *
     * @param clickCount New click count value
     * @return Error if clickCount is negative
     */
    [[nodiscard]] auto setClickCount(int clickCount)
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Increment click count by 1
     *
     * @return New click count value
     */
    auto incrementClickCount() -> int;

    /**
     * @brief Associate celestial object data with this star
     *
     * @param celestialObject CelestialObject containing detailed astronomical
     * data
     */
    void setCelestialObject(const CelestialObject& celestialObject);

    /**
     * @brief Get the associated celestial object data
     *
     * @return CelestialObject with detailed astronomical data
     */
    [[nodiscard]] const CelestialObject& getCelestialObject() const;

    /**
     * @brief Get the associated celestial object data (mutable)
     *
     * @return CelestialObject with detailed astronomical data
     */
    [[nodiscard]] CelestialObject& getCelestialObject();

    /**
     * @brief Serialize the star object to JSON
     *
     * @return JSON object representation of the star
     */
    [[nodiscard]] auto toJson() const
        -> atom::type::Expected<nlohmann::json, std::string>;

    /**
     * @brief Deserialize from JSON
     *
     * @param j JSON object
     * @return StarObject or error message
     */
    static auto fromJson(const nlohmann::json& j)
        -> atom::type::Expected<StarObject, std::string>;

    /**
     * @brief Get visual summary of the star
     *
     * @return String with key information
     */
    [[nodiscard]] auto getSummary() const -> std::string;

} ATOM_ALIGNAS(128);

}  // namespace lithium::target::model

#endif  // LITHIUM_TARGET_MODEL_STAR_OBJECT_HPP
