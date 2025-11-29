/**
 * @file wcs.cpp
 * @brief WCS utilities implementation.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "wcs.hpp"

#include <cmath>
#include <stdexcept>

namespace lithium::tools::solver {

WCSParams extractWCSParams(const std::string& wcsInfo) {
    WCSParams wcs;

    auto findAndExtract = [&wcsInfo](const std::string& key,
                                     size_t offset) -> double {
        size_t pos = wcsInfo.find(key);
        if (pos == std::string::npos) {
            throw std::runtime_error("WCS key not found: " + key);
        }
        size_t endPos = wcsInfo.find('\n', pos);
        if (endPos == std::string::npos) {
            endPos = wcsInfo.length();
        }
        return std::stod(wcsInfo.substr(pos + offset, endPos - pos - offset));
    };

    try {
        wcs.crpix0 = findAndExtract("crpix0", 7);
        wcs.crpix1 = findAndExtract("crpix1", 7);
        wcs.crval0 = findAndExtract("crval0", 7);
        wcs.crval1 = findAndExtract("crval1", 7);
        wcs.cd11 = findAndExtract("cd11", 5);
        wcs.cd12 = findAndExtract("cd12", 5);
        wcs.cd21 = findAndExtract("cd21", 5);
        wcs.cd22 = findAndExtract("cd22", 5);
    } catch (const std::exception& e) {
        // Return default WCS on parse error
        return WCSParams{};
    }

    return wcs;
}

SphericalCoordinates pixelToRaDec(double x, double y, const WCSParams& wcs) {
    double dx = x - wcs.crpix0;
    double dy = y - wcs.crpix1;

    double ra = wcs.crval0 + wcs.cd11 * dx + wcs.cd12 * dy;
    double dec = wcs.crval1 + wcs.cd21 * dx + wcs.cd22 * dy;

    return {ra, dec};
}

bool raDecToPixel(double ra, double dec, const WCSParams& wcs, double& x,
                  double& y) {
    // Calculate determinant of CD matrix
    double det = wcs.cd11 * wcs.cd22 - wcs.cd12 * wcs.cd21;
    if (std::abs(det) < 1e-10) {
        return false;  // Singular matrix
    }

    // Calculate inverse CD matrix
    double invCd11 = wcs.cd22 / det;
    double invCd12 = -wcs.cd12 / det;
    double invCd21 = -wcs.cd21 / det;
    double invCd22 = wcs.cd11 / det;

    // Calculate pixel coordinates
    double dra = ra - wcs.crval0;
    double ddec = dec - wcs.crval1;

    x = wcs.crpix0 + invCd11 * dra + invCd12 * ddec;
    y = wcs.crpix1 + invCd21 * dra + invCd22 * ddec;

    return true;
}

std::vector<SphericalCoordinates> getFOVCorners(const WCSParams& wcs,
                                                int imageWidth,
                                                int imageHeight) {
    std::vector<SphericalCoordinates> corners(4);
    corners[0] = pixelToRaDec(0, 0, wcs);                     // Bottom-left
    corners[1] = pixelToRaDec(imageWidth, 0, wcs);            // Bottom-right
    corners[2] = pixelToRaDec(imageWidth, imageHeight, wcs);  // Top-right
    corners[3] = pixelToRaDec(0, imageHeight, wcs);           // Top-left
    return corners;
}

void calculateFOVDimensions(const WCSParams& wcs, int imageWidth,
                            int imageHeight, double& fovWidth,
                            double& fovHeight) {
    auto corners = getFOVCorners(wcs, imageWidth, imageHeight);

    // Calculate width (average of top and bottom edges)
    double bottomWidth =
        std::abs(corners[1].rightAscension - corners[0].rightAscension);
    double topWidth =
        std::abs(corners[2].rightAscension - corners[3].rightAscension);
    fovWidth = (bottomWidth + topWidth) / 2.0;

    // Calculate height (average of left and right edges)
    double leftHeight =
        std::abs(corners[3].declination - corners[0].declination);
    double rightHeight =
        std::abs(corners[2].declination - corners[1].declination);
    fovHeight = (leftHeight + rightHeight) / 2.0;
}

}  // namespace lithium::tools::solver
