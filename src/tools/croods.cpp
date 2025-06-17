#include "croods.hpp"
#include "convert.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>
#include <spdlog/spdlog.h>

namespace lithium::tools {

namespace {
    // Time constants
    constexpr double SECONDS_IN_MINUTE = 60.0;
    constexpr double SECONDS_IN_HOUR = 3600.0; // 60 * 60
    constexpr double SECONDS_IN_DAY = 86400.0; // 24 * 60 * 60
    constexpr double MINUTES_IN_HOUR = 60.0;
    constexpr double HOURS_IN_DAY = 24.0;
    
    // Angular constants
    constexpr double PI = std::numbers::pi;
    constexpr double DEGREES_IN_CIRCLE = 360.0;
    constexpr double RADIANS_TO_DEGREES = 180.0 / PI;
    constexpr double DEGREES_TO_RADIANS = PI / 180.0;
}

double timeToJD(const std::chrono::system_clock::time_point& time) {
    auto duration = time.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    return JD_EPOCH + (seconds / SECONDS_IN_DAY);
}

double jdToMJD(double jd) { 
    return jd - MJD_OFFSET; 
}

double mjdToJD(double mjd) { 
    return mjd + MJD_OFFSET; 
}

double calculateBJD(double jd, double ra, double dec, double longitude,
                   double latitude, double elevation) {
    // Convert coordinates to radians
    double raRad = ra * DEGREES_TO_RADIANS;
    double decRad = dec * DEGREES_TO_RADIANS;
    double lonRad = longitude * DEGREES_TO_RADIANS;
    double latRad = latitude * DEGREES_TO_RADIANS;

    // Calculate light travel time (simplified)
    double earthRadius = EARTH_RADIUS_EQUATORIAL;
    double altitude = elevation + earthRadius;
    double lightTime = altitude * std::sin(decRad) / SPEED_OF_LIGHT;

    // Convert to BJD
    return jd + lightTime / SECONDS_IN_DAY;
}

std::string formatTime(const std::chrono::system_clock::time_point& time,
                      bool isLocal, const std::string& format) {
    std::time_t tt = std::chrono::system_clock::to_time_t(time);
    std::tm tm = isLocal ? *std::localtime(&tt) : *std::gmtime(&tt);
    std::stringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str() + (isLocal ? " (Local)" : " (UTC)");
}

bool periodBelongs(double value, double minVal, double maxVal, double period,
                  bool minInclusive, bool maxInclusive) {
    spdlog::info("periodBelongs: value={:.6f}, min={:.6f}, max={:.6f}, period={:.6f}, "
                "minInclusive={}, maxInclusive={}", 
                value, minVal, maxVal, period, minInclusive, maxInclusive);

    // Optimize by pre-calculating period indices
    int periodIndex = static_cast<int>((value - maxVal) / period);
    
    // Check ranges with optimized comparisons
    for (int i = -1; i <= 1; ++i) {
        double rangeMin = minVal + (periodIndex + i) * period;
        double rangeMax = maxVal + (periodIndex + i) * period;
        
        bool inRange = (minInclusive ? value >= rangeMin : value > rangeMin) &&
                       (maxInclusive ? value <= rangeMax : value < rangeMax);
        
        if (inRange) {
            spdlog::info("Value belongs to range: [{:.6f}, {:.6f}]", rangeMin, rangeMax);
            return true;
        }
    }

    spdlog::info("Value does not belong to any range");
    return false;
}

std::string getInfoTextA(const std::chrono::system_clock::time_point& localTime,
                        double raDegree, double decDegree, double dRaDegree,
                        double dDecDegree, const std::string& mountStatus,
                        const std::string& guideStatus) {
    // Pre-allocate storage for performance
    std::vector<size_t> start = {0, 16, 23, 50, 65, 75, 90, 103};
    std::vector<std::string> strs(8);

    // Format data
    strs[0] = formatTime(localTime, true);
    strs[1] = "RA/DEC";
    strs[2] = radToHmsStr(degreeToRad(raDegree)) + " " +
              radToDmsStr(degreeToRad(decDegree));
    strs[3] = mountStatus;
    strs[4] = guideStatus;
    strs[5] = "RMS " + std::to_string(dRaDegree) + "/" + std::to_string(dDecDegree);

    // Create formatted result with preallocated space
    std::string result(120, ' ');
    for (size_t i = 0; i < start.size(); ++i) {
        if (i < strs.size() && start[i] + strs[i].length() <= result.length()) {
            result.replace(start[i], strs[i].length(), strs[i]);
        }
    }
    return result;
}

std::string getInfoTextB(const std::chrono::system_clock::time_point& utcTime,
                        double azRad, double altRad, const std::string& camStatus,
                        double camTemp, double camTargetTemp, int camX, int camY,
                        int cfwPos, const std::string& cfwName,
                        const std::string& cfwStatus) {
    // Pre-allocate storage for performance
    std::vector<size_t> start = {0, 16, 24, 50, 65, 75, 90, 103};
    std::vector<std::string> strs(8);

    // Format data
    strs[0] = formatTime(utcTime, false);
    strs[1] = "AZ/ALT";
    strs[2] = radToDmsStr(azRad) + " " + radToDmsStr(altRad);
    strs[3] = camStatus;
    strs[4] = std::to_string(camTemp) + "/" + std::to_string(camTargetTemp);
    strs[5] = std::to_string(camX) + "*" + std::to_string(camY);
    strs[6] = "CFW " + cfwStatus;
    strs[7] = "#" + std::to_string(cfwPos) + " " + cfwName;

    // Create formatted result with preallocated space
    std::string result(120, ' ');
    for (size_t i = 0; i < start.size(); ++i) {
        if (i < strs.size() && start[i] + strs[i].length() <= result.length()) {
            result.replace(start[i], strs[i].length(), strs[i]);
        }
    }
    return result;
}

std::string getInfoTextC(int cpuTemp, int cpuLoad, double diskFree,
                        double longitudeRad, double latitudeRad, double raJ2000,
                        double decJ2000, double az, double alt,
                        const std::string& objName) {
    // Pre-allocate storage for performance
    std::vector<size_t> start = {0, 16, 23, 50, 65, 120, 121, 122};
    std::vector<std::string> strs(8);

    // Format data
    strs[0] = "CPU " + std::to_string(cpuTemp) + "C " + std::to_string(cpuLoad) + "%";
    strs[1] = "Site";
    strs[2] = radToDmsStr(longitudeRad) + " " + radToDmsStr(latitudeRad);
    strs[3] = "Free " + std::to_string(diskFree) + "G";
    strs[4] = "Info: " + objName + " " + radToHmsStr(raJ2000) + " " +
              radToDmsStr(decJ2000) + " " + radToDmsStr(PI - az) + " " +
              radToDmsStr(alt);

    // Create formatted result with preallocated space
    std::string result(150, ' ');
    for (size_t i = 0; i < start.size(); ++i) {
        if (i < strs.size() && start[i] + strs[i].length() <= result.length()) {
            result.replace(start[i], strs[i].length(), strs[i]);
        }
    }
    return result;
}

}  // namespace lithium::tools