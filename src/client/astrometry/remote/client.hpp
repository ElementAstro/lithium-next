#pragma once

#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include "atom/type/json_fwd.hpp"

namespace astrometry {

/**
 * @brief Configuration structure for the Astrometry client.
 *
 * This structure holds all configuration options for the AstrometryClient,
 * including API endpoint, timeout, SSL verification, user agent, caching,
 * retry logic, and logging preferences.
 */
struct ClientConfig {
    std::string api_url =
        "http://nova.astrometry.net/api";  ///< Base URL for the Astrometry.net
                                           ///< API.
    std::chrono::seconds timeout{30};      ///< Timeout for HTTP requests.
    bool verify_ssl = true;  ///< Whether to verify SSL certificates.
    std::string user_agent =
        "AstrometryNet-Cpp-Client/1.0";  ///< User agent string for HTTP
                                         ///< requests.
    std::filesystem::path cache_dir;     ///< Directory for caching results.
    bool enable_cache = false;           ///< Enable or disable caching.
    int retry_count = 3;  ///< Number of times to retry failed requests.
    std::chrono::milliseconds retry_delay{1000};  ///< Delay between retries.

    // Logging configuration
    std::string log_level = "info";  ///< Logging level (e.g., "info", "debug").
    std::string log_file;  ///< Path to log file (if empty, no file logging).
    bool log_to_console = true;  ///< Whether to log to the console.
};

/**
 * @brief License options for image submissions.
 *
 * Specifies the license type for submitted images, controlling commercial use
 * and modifications.
 */
enum class License { Default, Yes, No, ShareAlike };

/**
 * @brief Units for specifying image scale.
 *
 * Used to indicate the units for scale parameters in image submissions.
 */
enum class ScaleUnits { DegWidth, ArcMinWidth, ArcSecPerPix };

/**
 * @brief Type of scale specification for image submissions.
 *
 * Indicates whether the scale is provided as bounds or as an estimate.
 */
enum class ScaleType { Bounds, Estimate };

/**
 * @brief Parameters for submitting an image to Astrometry.net.
 *
 * This structure contains all possible parameters for an image submission,
 * including the image source (URL or file), licensing, visibility, scale,
 * position, and processing options.
 */
struct SubmissionParams {
    // Required parameters (one of these must be set)
    std::optional<std::string> url;  ///< URL of the image to submit.
    std::optional<std::filesystem::path>
        file_path;  ///< Local file path of the image to submit.

    // Optional parameters
    License allow_commercial_use =
        License::Default;  ///< License for commercial use.
    License allow_modifications =
        License::Default;  ///< License for modifications.
    bool publicly_visible =
        false;  ///< Whether the submission is publicly visible.

    // Scale parameters
    ScaleUnits scale_units =
        ScaleUnits::DegWidth;                  ///< Units for scale parameters.
    ScaleType scale_type = ScaleType::Bounds;  ///< Type of scale specification.
    std::optional<double> scale_lower;         ///< Lower bound for scale.
    std::optional<double> scale_upper;         ///< Upper bound for scale.
    std::optional<double> scale_est;           ///< Estimated scale.
    std::optional<double> scale_err;  ///< Error margin for estimated scale.

    // Position parameters
    std::optional<double>
        center_ra;  ///< Right ascension of the image center (degrees).
    std::optional<double>
        center_dec;  ///< Declination of the image center (degrees).
    std::optional<double> radius;  ///< Search radius (degrees).

    // Processing parameters
    std::optional<double>
        downsample_factor;           ///< Downsampling factor for processing.
    std::optional<int> tweak_order;  ///< Polynomial order for WCS tweaking.
    std::optional<bool>
        use_sextractor;  ///< Whether to use SExtractor for source extraction.
    std::optional<bool>
        crpix_center;           ///< Whether to center CRPIX in the output WCS.
    std::optional<int> parity;  ///< Parity for the solution (1 or -1).

    // For x,y lists
    std::optional<int> image_width;          ///< Width of the image (pixels).
    std::optional<int> image_height;         ///< Height of the image (pixels).
    std::optional<double> positional_error;  ///< Positional error (arcseconds).

    /**
     * @brief Validate the submission parameters.
     *
     * Checks that required parameters are set and values are within valid
     * ranges.
     * @return true if parameters are valid, false otherwise.
     */
    bool validate() const;
};

/**
 * @brief Result of a successful astrometric calibration.
 *
 * Contains the celestial coordinates, pixel scale, orientation, and parity
 * of the calibrated image.
 */
struct CalibrationResult {
    double ra;           ///< Right ascension of the image center (degrees).
    double dec;          ///< Declination of the image center (degrees).
    double radius;       ///< Radius of the field (degrees).
    double pixscale;     ///< Pixel scale (arcseconds per pixel).
    double orientation;  ///< Orientation angle (degrees).
    double parity;       ///< Parity of the solution (1 or -1).
};

/**
 * @brief Annotation for an object detected in the image.
 *
 * Represents a labeled object (e.g., star, galaxy) with its pixel coordinates
 * and radius.
 */
struct Annotation {
    std::string type;  ///< Type of annotation (e.g., "star", "galaxy").
    std::vector<std::string> names;  ///< Names or identifiers for the object.
    double pixelx;                   ///< X coordinate in image pixels.
    double pixely;                   ///< Y coordinate in image pixels.
    double radius;                   ///< Radius of the annotation (pixels).
};

/**
 * @brief Comprehensive information about a completed job.
 *
 * Includes status, tags, detected objects, original filename, calibration
 * result, and all annotations.
 */
struct JobInfo {
    std::string status;  ///< Status of the job (e.g., "success", "failure").
    std::vector<std::string>
        machine_tags;               ///< Machine-generated tags for the job.
    std::vector<std::string> tags;  ///< User-defined tags for the job.
    std::vector<std::string>
        objects_in_field;  ///< Names of objects detected in the field.
    std::string
        original_filename;  ///< Original filename of the submitted image.
    std::optional<CalibrationResult>
        calibration;  ///< Calibration result, if available.
    std::vector<Annotation>
        annotations;  ///< List of annotations for detected objects.
};

/**
 * @brief Error codes for Astrometry client operations.
 *
 * Enumerates possible error conditions that can occur during client operations.
 */
enum class ErrorCode {
    Success = 0,           ///< Operation completed successfully.
    NetworkError,          ///< Network communication error.
    AuthenticationFailed,  ///< Authentication or login failed.
    InvalidParameters,     ///< Invalid parameters provided.
    ServerError,           ///< Server-side error.
    ParseError,            ///< Failed to parse server response.
    Timeout,               ///< Operation timed out.
    FileError,             ///< File I/O error.
    UnknownError           ///< Unknown or unspecified error.
};

/**
 * @brief Exception class for Astrometry client errors.
 *
 * Wraps an ErrorCode and a descriptive error message.
 */
class AstrometryError : public std::runtime_error {
public:
    /**
     * @brief Construct a new AstrometryError.
     * @param code The error code.
     * @param message A descriptive error message.
     */
    AstrometryError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    /**
     * @brief Get the error code associated with this exception.
     * @return The error code.
     */
    ErrorCode code() const { return code_; }

private:
    ErrorCode code_;  ///< The error code.
};

/**
 * @brief Main client class for interacting with the Astrometry.net API.
 *
 * Provides methods for authentication, image submission, job status checking,
 * result retrieval, and file downloads. Supports both synchronous and
 * asynchronous operations, as well as logging and configuration.
 */
class AstrometryClient {
public:
    /**
     * @brief Construct a new AstrometryClient.
     *
     * @param api_key The API key for authentication.
     * @param config Optional client configuration.
     */
    explicit AstrometryClient(std::string api_key, ClientConfig config = {});

    /**
     * @brief Destructor. Cleans up resources.
     */
    ~AstrometryClient();

    // Prevent copy and move operations
    AstrometryClient(const AstrometryClient&) = delete;
    AstrometryClient& operator=(const AstrometryClient&) = delete;
    AstrometryClient(AstrometryClient&&) = delete;
    AstrometryClient& operator=(AstrometryClient&&) = delete;

    /**
     * @brief Log in to the Astrometry.net API.
     *
     * Authenticates the client using the provided API key.
     * @return true if login was successful, false otherwise.
     * @throws AstrometryError on network or authentication failure.
     */
    bool login();

    /**
     * @brief Check if the client is currently logged in.
     * @return true if logged in, false otherwise.
     */
    bool is_logged_in() const;

    /**
     * @brief Log out from the current session.
     *
     * Invalidates the current session key.
     * @return true if logout was successful, false otherwise.
     */
    bool logout();

    /**
     * @brief Submit an image by URL for processing.
     *
     * @param params Submission parameters including the image URL.
     * @return Submission ID if successful.
     * @throws AstrometryError on failure.
     */
    int submit_url(const SubmissionParams& params);

    /**
     * @brief Submit a local image file for processing.
     *
     * @param params Submission parameters including the file path.
     * @return Submission ID if successful.
     * @throws AstrometryError on failure.
     */
    int submit_file(const SubmissionParams& params);

    /**
     * @brief Submit an image by URL for processing asynchronously.
     *
     * @param params Submission parameters including the image URL.
     * @return std::future containing the submission ID if successful.
     */
    std::future<int> submit_url_async(const SubmissionParams& params);

    /**
     * @brief Submit a local image file for processing asynchronously.
     *
     * @param params Submission parameters including the file path.
     * @return std::future containing the submission ID if successful.
     */
    std::future<int> submit_file_async(const SubmissionParams& params);

    /**
     * @brief Check the status of a submission.
     *
     * @param submission_id The submission ID to check.
     * @return JSON object containing submission status.
     * @throws AstrometryError on failure.
     */
    nlohmann::json get_submission_status(int submission_id);

    /**
     * @brief Get the status of a job.
     *
     * @param job_id The job ID to check.
     * @return JSON object containing job status.
     * @throws AstrometryError on failure.
     */
    nlohmann::json get_job_status(int job_id);

    /**
     * @brief Get the calibration information for a job.
     *
     * @param job_id The job ID to retrieve calibration for.
     * @return CalibrationResult containing the calibration information.
     * @throws AstrometryError on failure.
     */
    CalibrationResult get_job_calibration(int job_id);

    /**
     * @brief Get the tags for a job.
     *
     * @param job_id The job ID to retrieve tags for.
     * @return Vector of tags.
     * @throws AstrometryError on failure.
     */
    std::vector<std::string> get_job_tags(int job_id);

    /**
     * @brief Get the machine tags for a job.
     *
     * @param job_id The job ID to retrieve machine tags for.
     * @return Vector of machine tags.
     * @throws AstrometryError on failure.
     */
    std::vector<std::string> get_job_machine_tags(int job_id);

    /**
     * @brief Get the objects in field for a job.
     *
     * @param job_id The job ID to retrieve objects for.
     * @return Vector of object names.
     * @throws AstrometryError on failure.
     */
    std::vector<std::string> get_objects_in_field(int job_id);

    /**
     * @brief Get the annotations for a job.
     *
     * @param job_id The job ID to retrieve annotations for.
     * @return Vector of annotations.
     * @throws AstrometryError on failure.
     */
    std::vector<Annotation> get_annotations(int job_id);

    /**
     * @brief Get comprehensive information about a job.
     *
     * @param job_id The job ID to retrieve information for.
     * @return JobInfo containing all available information.
     * @throws AstrometryError on failure.
     */
    JobInfo get_job_info(int job_id);

    /**
     * @brief Download a result file for a job.
     *
     * Downloads a result file (e.g., WCS, FITS, RDLS) for the specified job.
     * @param job_id The job ID to download a file for.
     * @param file_type The type of file to download ("wcs", "fits", "rdls",
     * etc.).
     * @param output_path The path where the file should be saved.
     * @return true if download was successful, false otherwise.
     * @throws AstrometryError on failure.
     */
    bool download_job_file(int job_id, const std::string& file_type,
                           const std::filesystem::path& output_path);

    /**
     * @brief Download a result file for a job asynchronously.
     *
     * @param job_id The job ID to download a file for.
     * @param file_type The type of file to download ("wcs", "fits", "rdls",
     * etc.).
     * @param output_path The path where the file should be saved.
     * @return std::future indicating if download was successful.
     */
    std::future<bool> download_job_file_async(
        int job_id, const std::string& file_type,
        const std::filesystem::path& output_path);

    /**
     * @brief Wait for a job to complete.
     *
     * Polls the server until the job completes or the timeout is reached.
     * @param submission_id The submission ID to wait for.
     * @param timeout_seconds Maximum time to wait in seconds.
     * @param poll_interval_seconds Time between status checks in seconds.
     * @return Job ID if successful, -1 if timeout or error.
     * @throws AstrometryError on failure.
     */
    int wait_for_job_completion(int submission_id, int timeout_seconds = 600,
                                int poll_interval_seconds = 5);

    /**
     * @brief Wait for a job to complete asynchronously.
     *
     * @param submission_id The submission ID to wait for.
     * @param timeout_seconds Maximum time to wait in seconds.
     * @param poll_interval_seconds Time between status checks in seconds.
     * @return std::future containing job ID if successful, -1 if timeout or
     * error.
     */
    std::future<int> wait_for_job_completion_async(
        int submission_id, int timeout_seconds = 600,
        int poll_interval_seconds = 5);

    /**
     * @brief Set the logger for the client.
     *
     * Allows the user to provide a custom spdlog logger.
     * @param logger Shared pointer to a spdlog logger.
     */
    void set_logger(std::shared_ptr<spdlog::logger> logger);

    /**
     * @brief Get the current logger.
     * @return Shared pointer to the current logger.
     */
    std::shared_ptr<spdlog::logger> get_logger() const;

    /**
     * @brief Set the API base URL.
     * @param url The new API base URL.
     */
    void set_api_url(const std::string& url);

    /**
     * @brief Get the API base URL.
     * @return The current API base URL.
     */
    std::string get_api_url() const;

private:
    std::string api_key_;      ///< API key for authentication.
    std::string session_key_;  ///< Session key for the current login.
    ClientConfig config_;      ///< Client configuration.
    std::shared_ptr<spdlog::logger> logger_;  ///< Logger instance.
    std::unique_ptr<CURL, void (*)(CURL*)>
        curl_;               ///< CURL handle for HTTP requests.
    std::mutex curl_mutex_;  ///< Mutex for thread-safe CURL operations.

    /**
     * @brief Initialize CURL and logger.
     *
     * Sets up the CURL handle and configures logging.
     */
    void initialize();

    /**
     * @brief Helper method for making HTTP requests.
     *
     * Sends a request to the specified API endpoint with the given parameters.
     * @param endpoint API endpoint (relative to base URL).
     * @param params JSON parameters for the request.
     * @param use_post Whether to use HTTP POST (true) or GET (false).
     * @return JSON response from the server.
     * @throws AstrometryError on network or server error.
     */
    nlohmann::json make_request(const std::string& endpoint,
                                const nlohmann::json& params,
                                bool use_post = true);

    /**
     * @brief Helper method for uploading files.
     *
     * Uploads a file to the specified API endpoint with the given parameters.
     * @param endpoint API endpoint (relative to base URL).
     * @param params JSON parameters for the request.
     * @param file_path Path to the file to upload.
     * @return JSON response from the server.
     * @throws AstrometryError on network or server error.
     */
    nlohmann::json upload_file(const std::string& endpoint,
                               const nlohmann::json& params,
                               const std::filesystem::path& file_path);

    // String conversion helpers

    /**
     * @brief Convert a License enum to its string representation.
     * @param license The License value.
     * @return String representation.
     */
    static std::string license_to_string(License license);

    /**
     * @brief Convert a ScaleUnits enum to its string representation.
     * @param units The ScaleUnits value.
     * @return String representation.
     */
    static std::string scale_units_to_string(ScaleUnits units);

    /**
     * @brief Convert a ScaleType enum to its string representation.
     * @param type The ScaleType value.
     * @return String representation.
     */
    static std::string scale_type_to_string(ScaleType type);

    // Response handlers

    /**
     * @brief CURL write callback for receiving response data.
     * @param ptr Pointer to received data.
     * @param size Size of each data element.
     * @param nmemb Number of data elements.
     * @param userdata User data pointer.
     * @return Number of bytes handled.
     */
    static size_t write_callback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata);

    /**
     * @brief CURL header callback for receiving header data.
     * @param buffer Pointer to header data.
     * @param size Size of each data element.
     * @param nitems Number of data elements.
     * @param userdata User data pointer.
     * @return Number of bytes handled.
     */
    static size_t header_callback(char* buffer, size_t size, size_t nitems,
                                  void* userdata);

    // Error handling

    /**
     * @brief Handle CURL errors by throwing an exception.
     * @param result CURLcode result.
     * @param operation Description of the operation.
     * @throws AstrometryError with appropriate error code and message.
     */
    void handle_curl_error(CURLcode result, const std::string& operation);

    /**
     * @brief Handle server response errors by throwing an exception.
     * @param response JSON response from the server.
     * @param operation Description of the operation.
     * @throws AstrometryError with appropriate error code and message.
     */
    void handle_response_error(const nlohmann::json& response,
                               const std::string& operation);

    // Validation

    /**
     * @brief Validate that the client is logged in.
     * @throws AstrometryError if not logged in.
     */
    void validate_session() const;
};

}  // namespace astrometry
