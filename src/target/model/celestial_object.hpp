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

#ifndef LITHIUM_TARGET_MODEL_CELESTIAL_OBJECT_HPP
#define LITHIUM_TARGET_MODEL_CELESTIAL_OBJECT_HPP

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "atom/type/json_fwd.hpp"
#include "atom/type/expected.hpp"

namespace lithium::target::model {

/**
 * @brief Celestial coordinates with both string and decimal representations
 *
 * Represents a point in the celestial sphere using the J2000.0 equatorial
 * coordinate system.
 */
struct CelestialCoordinates {
    double raDecimal = 0.0;              ///< Right ascension in decimal degrees (0-360)
    std::string raString;                ///< Right ascension in HH:MM:SS format
    double decDecimal = 0.0;             ///< Declination in decimal degrees (-90 to +90)
    std::string decString;               ///< Declination in DD:MM:SS format

    /**
     * @brief Calculate the angular distance to another coordinate using
     * Haversine formula
     *
     * @param other Target coordinates
     * @return Angular distance in degrees
     */
    [[nodiscard]] auto angularDistance(
        const CelestialCoordinates& other) const -> double {
        // Convert to radians
        double ra1 = raDecimal * M_PI / 180.0;
        double dec1 = decDecimal * M_PI / 180.0;
        double ra2 = other.raDecimal * M_PI / 180.0;
        double dec2 = other.decDecimal * M_PI / 180.0;

        // Haversine formula
        double dRa = ra2 - ra1;
        double dDec = dec2 - dec1;

        double a = std::sin(dDec / 2.0) * std::sin(dDec / 2.0) +
                   std::cos(dec1) * std::cos(dec2) * std::sin(dRa / 2.0) *
                       std::sin(dRa / 2.0);
        double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
        double distance = 180.0 / M_PI * c;

        return distance;
    }

    /**
     * @brief Check if coordinates are valid
     *
     * @return true if RA is in [0, 360) and Dec is in [-90, 90]
     */
    [[nodiscard]] auto isValid() const -> bool {
        return raDecimal >= 0.0 && raDecimal < 360.0 && decDecimal >= -90.0 &&
               decDecimal <= 90.0;
    }

    /**
     * @brief Serialize to JSON
     *
     * @return JSON object representation
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Deserialize from JSON
     *
     * @param j JSON object
     * @return Deserialized coordinates or error
     */
    static auto fromJson(const nlohmann::json& j)
        -> atom::type::Expected<CelestialCoordinates, std::string>;
};

/**
 * @brief Represents a celestial astronomical object with detailed properties
 *
 * This class stores information about celestial objects like stars, galaxies,
 * nebulae, etc., including their catalog information, positional data,
 * physical characteristics, and descriptive details.
 */
class CelestialObject {
public:
    /**
     * @brief Constructs a celestial object with all properties
     *
     * @param id Unique identifier
     * @param identifier Primary catalog identifier
     * @param mIdentifier Messier catalog identifier
     * @param extensionName Extended name
     * @param component Component information
     * @param className Classification name
     * @param amateurRank Observer difficulty ranking
     * @param chineseName Chinese name of the object
     * @param type Object type (galaxy, nebula, star cluster, etc.)
     * @param duplicateType Type including duplicates
     * @param morphology Morphological classification
     * @param constellationCn Constellation name in Chinese
     * @param constellationEn Constellation name in English
     * @param raJ2000 Right ascension (J2000) in string format
     * @param raDJ2000 Right ascension (J2000) in decimal degrees
     * @param decJ2000 Declination (J2000) in string format
     * @param decDJ2000 Declination (J2000) in decimal degrees
     * @param visualMagnitudeV Visual magnitude (V band)
     * @param photographicMagnitudeB Photographic magnitude (B band)
     * @param bMinusV B-V color index
     * @param surfaceBrightness Surface brightness in mag/arcmin²
     * @param majorAxis Major axis size in arcmin
     * @param minorAxis Minor axis size in arcmin
     * @param positionAngle Position angle in degrees
     * @param detailedDescription Detailed object description
     * @param briefDescription Brief object description
     */
    CelestialObject(std::string id, std::string identifier,
                    std::string mIdentifier, std::string extensionName,
                    std::string component, std::string className,
                    std::string amateurRank, std::string chineseName,
                    std::string type, std::string duplicateType,
                    std::string morphology, std::string constellationCn,
                    std::string constellationEn, std::string raJ2000,
                    double raDJ2000, std::string decJ2000, double decDJ2000,
                    double visualMagnitudeV, double photographicMagnitudeB,
                    double bMinusV, double surfaceBrightness, double majorAxis,
                    double minorAxis, int positionAngle,
                    std::string detailedDescription,
                    std::string briefDescription)
        : ID(std::move(id)),
          Identifier(std::move(identifier)),
          MIdentifier(std::move(mIdentifier)),
          ExtensionName(std::move(extensionName)),
          Component(std::move(component)),
          ClassName(std::move(className)),
          AmateurRank(std::move(amateurRank)),
          ChineseName(std::move(chineseName)),
          Type(std::move(type)),
          DuplicateType(std::move(duplicateType)),
          Morphology(std::move(morphology)),
          ConstellationCn(std::move(constellationCn)),
          ConstellationEn(std::move(constellationEn)),
          RAJ2000(std::move(raJ2000)),
          RADJ2000(raDJ2000),
          DecJ2000(std::move(decJ2000)),
          DecDJ2000(decDJ2000),
          VisualMagnitudeV(visualMagnitudeV),
          PhotographicMagnitudeB(photographicMagnitudeB),
          BMinusV(bMinusV),
          SurfaceBrightness(surfaceBrightness),
          MajorAxis(majorAxis),
          MinorAxis(minorAxis),
          PositionAngle(positionAngle),
          DetailedDescription(std::move(detailedDescription)),
          BriefDescription(std::move(briefDescription)) {}

    /**
     * @brief Default constructor
     */
    CelestialObject() = default;

    /**
     * @brief Deserialize a celestial object from JSON data
     *
     * @param j JSON object containing celestial object data
     * @return CelestialObject instance or error message
     */
    static auto fromJson(const nlohmann::json& j)
        -> atom::type::Expected<CelestialObject, std::string>;

    /**
     * @brief Serialize the celestial object to JSON
     *
     * @return JSON object representation of the celestial object or error
     */
    [[nodiscard]] auto toJson() const
        -> atom::type::Expected<nlohmann::json, std::string>;

    /**
     * @brief Get the name (identifier) of the celestial object
     *
     * @return The object's primary identifier
     */
    [[nodiscard]] const std::string& getName() const { return Identifier; }

    /**
     * @brief Get celestial coordinates
     *
     * @return Coordinates structure with RA and Dec
     */
    [[nodiscard]] auto getCoordinates() const -> CelestialCoordinates {
        return CelestialCoordinates{RADJ2000, RAJ2000, DecDJ2000, DecJ2000};
    }

    /**
     * @brief Check if this object is visible from given latitude
     *
     * @param observerLatitude Observer latitude in degrees (-90 to 90)
     * @return true if object can be observed from that latitude
     */
    [[nodiscard]] auto isVisibleFrom(double observerLatitude) const -> bool {
        // Object is visible if its declination is within observer's observable
        // range
        double minDec = observerLatitude - 90.0;
        double maxDec = observerLatitude + 90.0;
        return DecDJ2000 >= minDec && DecDJ2000 <= maxDec;
    }

    /**
     * @brief Calculate altitude angle for given observer location and time
     *
     * @param observerLatitude Observer latitude in degrees
     * @param observerLongitude Observer longitude in degrees
     * @param localHourAngle Local hour angle in degrees
     * @return Altitude angle in degrees, or error if calculation fails
     */
    [[nodiscard]] auto calculateAltitude(double observerLatitude,
                                        double observerLongitude,
                                        double localHourAngle) const
        -> atom::type::Expected<double, std::string>;

    /**
     * @brief Get human-readable object type description
     *
     * @return Type string (e.g., "Galaxy", "Nebula", "Star Cluster")
     */
    [[nodiscard]] const std::string& getTypeDescription() const {
        return Type;
    }

    /**
     * @brief Get object's morphological classification
     *
     * @return Morphology string
     */
    [[nodiscard]] const std::string& getMorphology() const {
        return Morphology;
    }

    // Object properties
    std::string ID;               ///< Unique identifier
    std::string Identifier;       ///< Primary catalog identifier
    std::string MIdentifier;      ///< Messier catalog identifier
    std::string ExtensionName;    ///< Extended name
    std::string Component;        ///< Component information
    std::string ClassName;        ///< Classification name
    std::string AmateurRank;      ///< Observer difficulty ranking
    std::string ChineseName;      ///< Chinese name of the object
    std::string Type;             ///< Object type
    std::string DuplicateType;    ///< Type including duplicates
    std::string Morphology;       ///< Morphological classification
    std::string ConstellationCn;  ///< Constellation name in Chinese
    std::string ConstellationEn;  ///< Constellation name in English
    std::string RAJ2000;          ///< Right ascension (J2000) in string format
    double RADJ2000;              ///< Right ascension (J2000) in decimal degrees
    std::string DecJ2000;         ///< Declination (J2000) in string format
    double DecDJ2000;             ///< Declination (J2000) in decimal degrees
    double VisualMagnitudeV;      ///< Visual magnitude (V band)
    double PhotographicMagnitudeB;    ///< Photographic magnitude (B band)
    double BMinusV;                   ///< B-V color index
    double SurfaceBrightness;         ///< Surface brightness in mag/arcmin²
    double MajorAxis;                 ///< Major axis size in arcmin
    double MinorAxis;                 ///< Minor axis size in arcmin
    int PositionAngle;                ///< Position angle in degrees
    std::string DetailedDescription;  ///< Detailed object description
    std::string BriefDescription;     ///< Brief object description
};

}  // namespace lithium::target::model

#endif  // LITHIUM_TARGET_MODEL_CELESTIAL_OBJECT_HPP
