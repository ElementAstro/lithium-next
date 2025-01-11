#ifndef LITHIUM_TOOLS_FORMAT_HPP
#define LITHIUM_TOOLS_FORMAT_HPP

#include <format>

namespace lithium::tools {
template <std::floating_point T>
constexpr auto formatRa(T ra) -> std::string {
    static_assert(std::is_floating_point_v<T>,
                  "T must be a floating point type");

    int hours = static_cast<int>(ra);
    int minutes = static_cast<int>((ra - hours) * 60);
    double seconds = ((ra - hours) * 60 - minutes) * 60;

    return std::format("{:02d}h {:02d}m {:.2f}s", hours, minutes, seconds);
}

template <std::floating_point T>
constexpr auto formatDec(T dec) -> std::string {
    static_assert(std::is_floating_point_v<T>,
                  "T must be a floating point type");

    char sign = (dec >= 0) ? '+' : '-';
    dec = std::abs(dec);
    int degrees = static_cast<int>(dec);
    int minutes = static_cast<int>((dec - degrees) * 60);
    double seconds = ((dec - degrees) * 60 - minutes) * 60;

    return std::format("{}{:02d}° {:02d}' {:.2f}\"", sign, degrees, minutes,
                       seconds);
}
}  // namespace lithium::tools
#endif  // LITHIUM_TOOLS_FORMAT_HPP