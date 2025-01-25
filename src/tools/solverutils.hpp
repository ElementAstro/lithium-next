#ifndef LITHIUM_TOOLS_SOLVERUTILS_HPP
#define LITHIUM_TOOLS_SOLVERUTILS_HPP

#include <string>
#include <vector>
#include "atom/macro.hpp"
#include "tools/convert.hpp"

namespace lithium::tools {
struct WCSParams {
    double crpix0, crpix1;
    double crval0, crval1;
    double cd11, cd12, cd21, cd22;
} ATOM_ALIGNAS(64);

auto extractWCSParams(const std::string& wcsInfo) -> WCSParams;

auto pixelToRaDec(double x, double y,
                  const WCSParams& wcs) -> SphericalCoordinates;

auto getFOVCorners(const WCSParams& wcs, int imageWidth,
                   int imageHeight) -> std::vector<SphericalCoordinates>;
}  // namespace lithium::tools

#endif
