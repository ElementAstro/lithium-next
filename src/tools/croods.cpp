#include "croods.hpp"
#include "constant.hpp"
#include "tools/convert.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "atom/log/loguru.hpp"

namespace lithium::tools {
constexpr double K_SECONDS_IN_MINUTE = 60.0;
constexpr double K_SECONDS_IN_HOUR = 3600.0;

auto periodBelongs(double value, double minVal, double maxVal, double period,
                   bool minInclusive, bool maxInclusive) -> bool {
    LOG_F(INFO,
          "periodBelongs: value={:.6f}, min={:.6f}, max={:.6f}, period={:.6f}, "
          "minInclusive={}, maxInclusive={}",
          value, minVal, maxVal, period, minInclusive, maxInclusive);

    int periodIndex = static_cast<int>((value - maxVal) / period);
    std::array<std::array<double, 2>, 3> ranges = {
        {{minVal + (periodIndex - 1) * period,
          maxVal + (periodIndex - 1) * period},
         {minVal + periodIndex * period, maxVal + periodIndex * period},
         {minVal + (periodIndex + 1) * period,
          maxVal + (periodIndex + 1) * period}}};

    for (const auto& range : ranges) {
        if ((maxInclusive && minInclusive &&
             (value >= range[0] && value <= range[1])) ||
            (maxInclusive && !minInclusive &&
             (value > range[0] && value <= range[1])) ||
            (!maxInclusive && !minInclusive &&
             (value > range[0] && value < range[1])) ||
            (!maxInclusive && minInclusive &&
             (value >= range[0] && value < range[1]))) {
            LOG_F(INFO, "Value belongs to range: [{:.6f}, {:.6f}]", range[0],
                  range[1]);
            return true;
        }
    }

    LOG_F(INFO, "Value does not belong to any range");
    return false;
}

void printDMS(double angle) {
    int degrees = static_cast<int>(angle);
    double fractional = angle - degrees;
    int minutes = static_cast<int>(fractional * K_MINUTES_IN_HOUR);
    double seconds =
        (fractional * K_MINUTES_IN_HOUR - minutes) * K_SECONDS_IN_MINUTE;

    LOG_F(INFO, "{}° {}' {:.2f}\"", degrees, minutes, seconds);
}

inline std::string formatTime(const std::chrono::system_clock::time_point& time,
                              bool isLocal,
                              const std::string& format = "%H:%M:%S") {
    std::time_t tt = std::chrono::system_clock::to_time_t(time);
    std::tm tm = isLocal ? *std::localtime(&tt) : *std::gmtime(&tt);
    std::stringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str() + (isLocal ? "(Local)" : "(UTC)");
}

auto getInfoTextA(const std::chrono::system_clock::time_point& localTime,
                  double raDegree, double decDegree, double dRaDegree,
                  double dDecDegree, const std::string& mountStatus,
                  const std::string& guideStatus) -> std::string {
    std::vector<size_t> start = {0, 16, 23, 50, 65, 75, 90, 103};
    std::vector<std::string> strs(8);

    strs[0] = formatTime(localTime, true);
    strs[1] = "RA/DEC";
    strs[2] = radToHmsStr(degreeToRad(raDegree)) + " " +
              radToDmsStr(degreeToRad(decDegree));
    strs[3] = mountStatus;
    strs[4] = guideStatus;
    strs[5] =
        "RMS " + std::to_string(dRaDegree) + "/" + std::to_string(dDecDegree);

    std::string result(120, ' ');  // Pre-allocate with spaces
    for (size_t i = 0; i < start.size(); ++i) {
        result.replace(start[i], strs[i].length(), strs[i]);
    }
    return result;
}

auto getInfoTextB(const std::chrono::system_clock::time_point& utcTime,
                  double azRad, double altRad, const std::string& camStatus,
                  double camTemp, double camTargetTemp, int camX, int camY,
                  int cfwPos, const std::string& cfwName,
                  const std::string& cfwStatus) -> std::string {
    std::vector<size_t> start = {0, 16, 24, 50, 65, 75, 90, 103};
    std::vector<std::string> strs(8);

    strs[0] = formatTime(utcTime, false);
    strs[1] = "AZ/ALT";
    strs[2] = radToDmsStr(azRad) + " " + radToDmsStr(altRad);
    strs[3] = camStatus;
    strs[4] = std::to_string(camTemp) + "/" + std::to_string(camTargetTemp);
    strs[5] = std::to_string(camX) + "*" + std::to_string(camY);
    strs[6] = "CFW " + cfwStatus;
    strs[7] = "#" + std::to_string(cfwPos) + " " + cfwName;

    std::string result(120, ' ');
    for (size_t i = 0; i < start.size(); ++i) {
        result.replace(start[i], strs[i].length(), strs[i]);
    }
    return result;
}

auto getInfoTextC(int cpuTemp, int cpuLoad, double diskFree,
                  double longitudeRad, double latitudeRad, double raJ2000,
                  double decJ2000, double az, double alt,
                  const std::string& objName) -> std::string {
    std::vector<size_t> start = {0, 16, 23, 50, 65, 120, 121, 122};
    std::vector<std::string> strs(8);

    strs[0] =
        "CPU " + std::to_string(cpuTemp) + "C " + std::to_string(cpuLoad) + "%";
    strs[1] = "Site";
    strs[2] = radToDmsStr(longitudeRad) + " " + radToDmsStr(latitudeRad);
    strs[3] = "Free " + std::to_string(diskFree) + "G";
    strs[4] = "Info: " + objName + radToHmsStr(raJ2000) + " " +
              radToDmsStr(decJ2000) + " " + radToDmsStr(M_PI - az) + " " +
              radToDmsStr(alt);

    std::string result(150, ' ');
    for (size_t i = 0; i < start.size(); ++i) {
        result.replace(start[i], strs[i].length(), strs[i]);
    }
    return result;
}
}  // namespace lithium::tools