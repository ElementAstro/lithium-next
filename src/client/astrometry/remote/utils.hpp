#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "client.hpp"

namespace astrometry::utils {

/**
 * @brief Solve an image from a URL and wait for the result
 *
 * @param client The Astrometry client
 * @param url Image URL
 * @param params Optional submission parameters
 * @param timeout_seconds Timeout for job completion
 * @return JobInfo containing the results
 */
JobInfo solve_url(AstrometryClient& client, const std::string& url,
                  std::optional<SubmissionParams> params = std::nullopt,
                  int timeout_seconds = 600);

/**
 * @brief Solve an image from a file and wait for the result
 *
 * @param client The Astrometry client
 * @param file_path Path to the image file
 * @param params Optional submission parameters
 * @param timeout_seconds Timeout for job completion
 * @return JobInfo containing the results
 */
JobInfo solve_file(AstrometryClient& client,
                   const std::filesystem::path& file_path,
                   std::optional<SubmissionParams> params = std::nullopt,
                   int timeout_seconds = 600);

/**
 * @brief Format a calibration result as a human-readable string
 *
 * @param calibration The calibration result
 * @return Formatted string with calibration information
 */
std::string format_calibration(const CalibrationResult& calibration);

/**
 * @brief Convert degrees to sexagesimal format (HH:MM:SS.S or DD:MM:SS.S)
 *
 * @param degrees Degrees
 * @param is_ra Whether the value is Right Ascension (true) or Declination
 * (false)
 * @return Formatted sexagesimal string
 */
std::string degrees_to_sexagesimal(double degrees, bool is_ra = true);

/**
 * @brief Generate a WCS header from a calibration result
 *
 * @param calibration The calibration result
 * @param image_width Image width in pixels
 * @param image_height Image height in pixels
 * @return std::string WCS header as a string
 */
std::string generate_wcs_header(const CalibrationResult& calibration,
                                int image_width, int image_height);

}  // namespace astrometry::utils