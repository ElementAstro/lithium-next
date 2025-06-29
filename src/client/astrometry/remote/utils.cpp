#include "utils.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace astrometry::utils {

// Solve an image from a URL
JobInfo solve_url(AstrometryClient& client, const std::string& url,
                  std::optional<SubmissionParams> params, int timeout_seconds) {
    // Prepare submission params
    SubmissionParams submission_params;
    if (params.has_value()) {
        submission_params = params.value();
    }
    submission_params.url = url;

    // Ensure the client is logged in
    if (!client.is_logged_in()) {
        client.login();
    }

    // Submit URL
    int submission_id = client.submit_url(submission_params);

    // Wait for completion
    int job_id = client.wait_for_job_completion(submission_id, timeout_seconds);

    // Get job info
    return client.get_job_info(job_id);
}

// Solve an image from a file
JobInfo solve_file(AstrometryClient& client,
                   const std::filesystem::path& file_path,
                   std::optional<SubmissionParams> params,
                   int timeout_seconds) {
    // Prepare submission params
    SubmissionParams submission_params;
    if (params.has_value()) {
        submission_params = params.value();
    }
    submission_params.file_path = file_path;

    // Ensure the client is logged in
    if (!client.is_logged_in()) {
        client.login();
    }

    // Submit file
    int submission_id = client.submit_file(submission_params);

    // Wait for completion
    int job_id = client.wait_for_job_completion(submission_id, timeout_seconds);

    // Get job info
    return client.get_job_info(job_id);
}

// Format calibration as a string
std::string format_calibration(const CalibrationResult& calibration) {
    std::ostringstream oss;

    oss << "Calibration:\n";
    oss << "  Center: RA=" << degrees_to_sexagesimal(calibration.ra, true)
        << " (" << std::fixed << std::setprecision(6) << calibration.ra
        << " deg), "
        << "Dec=" << degrees_to_sexagesimal(calibration.dec, false) << " ("
        << std::fixed << std::setprecision(6) << calibration.dec << " deg)\n";
    oss << "  Field size: " << std::fixed << std::setprecision(3)
        << (calibration.radius * 2) << " deg\n";
    oss << "  Pixel scale: " << std::fixed << std::setprecision(3)
        << calibration.pixscale << " arcsec/pixel\n";
    oss << "  Orientation: " << std::fixed << std::setprecision(3)
        << calibration.orientation << " deg\n";
    oss << "  Parity: " << (calibration.parity > 0 ? "Normal" : "Reversed")
        << "\n";

    return oss.str();
}

// Convert degrees to sexagesimal format
std::string degrees_to_sexagesimal(double degrees, bool is_ra) {
    // Normalize the angle
    if (is_ra) {
        degrees = std::fmod(degrees, 360.0);
        if (degrees < 0)
            degrees += 360.0;

        // Convert to hours (RA is traditionally given in hours, not degrees)
        degrees /= 15.0;
    }

    // Extract the sign for declination
    char sign = ' ';
    if (!is_ra && degrees < 0) {
        sign = '-';
        degrees = std::abs(degrees);
    } else if (!is_ra) {
        sign = '+';
    }

    // Calculate components
    int hours_or_degrees = static_cast<int>(degrees);
    double minutes_decimal = (degrees - hours_or_degrees) * 60.0;
    int minutes = static_cast<int>(minutes_decimal);
    double seconds = (minutes_decimal - minutes) * 60.0;

    // Format the output
    std::ostringstream oss;
    if (!is_ra) {
        oss << sign;
    }

    oss << std::setfill('0') << std::setw(2) << hours_or_degrees << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(5) << std::fixed
        << std::setprecision(2) << seconds;

    return oss.str();
}

// Generate WCS header implementation
std::string generate_wcs_header(const CalibrationResult& calibration,
                                int image_width, int image_height) {
    std::ostringstream oss;

    // Reference pixel (center of the image)
    double crpix1 = image_width / 2.0;
    double crpix2 = image_height / 2.0;

    // Reference coordinates
    double crval1 = calibration.ra;
    double crval2 = calibration.dec;

    // Pixel scale in degrees/pixel
    double cdelt = calibration.pixscale / 3600.0;

    // Rotation matrix elements
    double cos_theta = std::cos(calibration.orientation * M_PI / 180.0);
    double sin_theta = std::sin(calibration.orientation * M_PI / 180.0);

    // CD matrix elements (include parity)
    double cd11 = -cdelt * cos_theta * calibration.parity;
    double cd12 = cdelt * sin_theta;
    double cd21 = -cdelt * sin_theta * calibration.parity;
    double cd22 = -cdelt * cos_theta;

    // Generate the header
    oss << "WCSAXES =                    2 / Number of coordinate axes\n";
    oss << "CTYPE1  = 'RA---TAN'           / TAN (gnomonic) projection\n";
    oss << "CTYPE2  = 'DEC--TAN'           / TAN (gnomonic) projection\n";
    oss << std::fixed << std::setprecision(10);
    oss << "CRVAL1  = " << std::setw(20) << crval1
        << " / RA at reference point (deg)\n";
    oss << "CRVAL2  = " << std::setw(20) << crval2
        << " / Dec at reference point (deg)\n";
    oss << std::fixed << std::setprecision(6);
    oss << "CRPIX1  = " << std::setw(20) << crpix1 << " / X reference pixel\n";
    oss << "CRPIX2  = " << std::setw(20) << crpix2 << " / Y reference pixel\n";
    oss << "CD1_1   = " << std::setw(20) << cd11
        << " / Transformation matrix\n";
    oss << "CD1_2   = " << std::setw(20) << cd12
        << " / Transformation matrix\n";
    oss << "CD2_1   = " << std::setw(20) << cd21
        << " / Transformation matrix\n";
    oss << "CD2_2   = " << std::setw(20) << cd22
        << " / Transformation matrix\n";
    oss << "RADESYS = 'ICRS    '           / International Celestial Reference "
           "System\n";
    oss << "EQUINOX =               2000.0 / Equinox of coordinates\n";

    return oss.str();
}

}  // namespace astrometry::utils
