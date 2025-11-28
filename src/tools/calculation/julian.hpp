/**
 * @file julian.hpp
 * @brief Julian Date calculations and conversions.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CALCULATION_JULIAN_HPP
#define LITHIUM_TOOLS_CALCULATION_JULIAN_HPP

#include <chrono>
#include <cmath>
#include <concepts>

#include "tools/astronomy/constants.hpp"

namespace lithium::tools::calculation {

using namespace lithium::tools::astronomy;

// ============================================================================
// DateTime Structure
// ============================================================================

/**
 * @struct DateTime
 * @brief Structure to hold date and time information.
 */
struct alignas(32) DateTime {
    int year{2000};
    int month{1};
    int day{1};
    int hour{0};
    int minute{0};
    double second{0.0};

    DateTime() = default;

    DateTime(int y, int m, int d, int h = 0, int min = 0, double sec = 0.0)
        : year(y), month(m), day(d), hour(h), minute(min), second(sec) {}

    /**
     * @brief Create from system time point.
     */
    [[nodiscard]] static DateTime fromTimePoint(
        std::chrono::system_clock::time_point tp) {
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_val{};
#ifdef _WIN32
        gmtime_s(&tm_val, &time_t_val);
#else
        gmtime_r(&time_t_val, &tm_val);
#endif
        return {tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
                tm_val.tm_hour, tm_val.tm_min, static_cast<double>(tm_val.tm_sec)};
    }

    /**
     * @brief Convert to system time point.
     */
    [[nodiscard]] std::chrono::system_clock::time_point toTimePoint() const {
        std::tm tm_val{};
        tm_val.tm_year = year - 1900;
        tm_val.tm_mon = month - 1;
        tm_val.tm_mday = day;
        tm_val.tm_hour = hour;
        tm_val.tm_min = minute;
        tm_val.tm_sec = static_cast<int>(second);
#ifdef _WIN32
        auto time_t_val = _mkgmtime(&tm_val);
#else
        auto time_t_val = timegm(&tm_val);
#endif
        return std::chrono::system_clock::from_time_t(time_t_val);
    }
};

// ============================================================================
// Julian Date Calculations
// ============================================================================

/**
 * @brief Calculate Julian Date from DateTime.
 * @tparam T Floating-point type.
 * @param dt DateTime structure.
 * @return Julian Date.
 */
template <std::floating_point T = double>
[[nodiscard]] auto calculateJulianDate(const DateTime& dt) -> T {
    constexpr int DAYS_IN_MONTH = 12;
    constexpr int HOURS_IN_DAY = 24;
    constexpr int MINUTES_IN_DAY = 1440;
    constexpr int SECONDS_IN_DAY = 86400;
    constexpr int DAYS_OFFSET = 32045;
    constexpr int EPOCH_OFFSET = 4800;
    constexpr int MONTH_ADJUSTMENT_DIVISOR = 12;
    constexpr int MONTH_OFFSET = 3;
    constexpr int MONTH_DAYS_COEF = 153;
    constexpr int MONTH_DAYS_DIVISOR = 5;
    constexpr int DAYS_IN_YEAR = 365;
    constexpr int LEAP_YEAR_DIVISOR = 4;
    constexpr int CENTURY_DIVISOR = 100;
    constexpr int LEAP_CENTURY_DIVISOR = 400;
    constexpr int NOON_HOUR = 12;

    int a = (MONTH_ADJUSTMENT_DIVISOR - dt.month) / DAYS_IN_MONTH;
    int y = dt.year + EPOCH_OFFSET - a;
    int m = dt.month + DAYS_IN_MONTH * a - MONTH_OFFSET;

    T julianDate =
        static_cast<T>(dt.day) +
        static_cast<T>(MONTH_DAYS_COEF * m + 2) / MONTH_DAYS_DIVISOR +
        static_cast<T>(DAYS_IN_YEAR) * y +
        static_cast<T>(y) / LEAP_YEAR_DIVISOR -
        static_cast<T>(y) / CENTURY_DIVISOR +
        static_cast<T>(y) / LEAP_CENTURY_DIVISOR - DAYS_OFFSET +
        static_cast<T>(dt.hour - NOON_HOUR) / HOURS_IN_DAY +
        static_cast<T>(dt.minute) / MINUTES_IN_DAY +
        static_cast<T>(dt.second) / SECONDS_IN_DAY;

    return julianDate;
}

/**
 * @brief Convert system time to Julian Date.
 * @param time System time point.
 * @return Julian Date.
 */
[[nodiscard]] inline double timeToJD(
    const std::chrono::system_clock::time_point& time) {
    auto duration = time.time_since_epoch();
    auto seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(duration)
            .count();
    return JD_UNIX_EPOCH + seconds / SECONDS_IN_DAY;
}

/**
 * @brief Convert Julian Date to system time.
 * @param jd Julian Date.
 * @return System time point.
 */
[[nodiscard]] inline std::chrono::system_clock::time_point jdToTime(double jd) {
    double seconds = (jd - JD_UNIX_EPOCH) * SECONDS_IN_DAY;
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<double>(seconds)));
}

/**
 * @brief Convert Julian Date to Modified Julian Date.
 * @param jd Julian Date.
 * @return Modified Julian Date.
 */
[[nodiscard]] constexpr double jdToMJD(double jd) noexcept {
    return jd - MJD_OFFSET;
}

/**
 * @brief Convert Modified Julian Date to Julian Date.
 * @param mjd Modified Julian Date.
 * @return Julian Date.
 */
[[nodiscard]] constexpr double mjdToJD(double mjd) noexcept {
    return mjd + MJD_OFFSET;
}

/**
 * @brief Calculate centuries since J2000.0.
 * @param jd Julian Date.
 * @return Julian centuries since J2000.0.
 */
[[nodiscard]] constexpr double centuriesSinceJ2000(double jd) noexcept {
    return (jd - JD_J2000) / JULIAN_CENTURY;
}

/**
 * @brief Calculate Barycentric Julian Date (BJD) from JD.
 * @param jd Julian Date.
 * @param ra Right Ascension in degrees.
 * @param dec Declination in degrees.
 * @param longitude Observer longitude in degrees.
 * @param latitude Observer latitude in degrees.
 * @param elevation Observer elevation in meters.
 * @return Barycentric Julian Date.
 */
[[nodiscard]] double calculateBJD(double jd, double ra, double dec,
                                   double longitude, double latitude,
                                   double elevation);

}  // namespace lithium::tools::calculation

#endif  // LITHIUM_TOOLS_CALCULATION_JULIAN_HPP
