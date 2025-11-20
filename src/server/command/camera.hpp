 #ifndef LITHIUM_SERVER_MIDDLEWARE_CAMERA_HPP
 #define LITHIUM_SERVER_MIDDLEWARE_CAMERA_HPP

 #include <string>

 #include "atom/type/json.hpp"

 namespace lithium::middleware {

 /**
  * @brief List all available cameras.
  */
 using json = nlohmann::json;

 auto listCameras() -> json;

 /**
  * @brief Get status of a specific camera.
  */
 auto getCameraStatus(const std::string &deviceId) -> json;

 /**
  * @brief Connect or disconnect a camera.
  */
 auto connectCamera(const std::string &deviceId, bool connected)
     -> json;

 /**
  * @brief Update camera settings such as cooler, gain, offset, binning, ROI.
  */
 auto updateCameraSettings(const std::string &deviceId,
                           const json &settings) -> json;

 /**
  * @brief Start a single exposure.
  */
 auto startExposure(const std::string &deviceId, double duration,
                    const std::string &frameType, const std::string &filename)
     -> json;

 /**
  * @brief Abort current exposure.
  */
 auto abortExposure(const std::string &deviceId) -> json;

 /**
  * @brief Get camera capabilities and limits.
  */
 auto getCameraCapabilities(const std::string &deviceId) -> json;

 /**
  * @brief Get available gain values and current/default/unity gain.
  */
 auto getCameraGains(const std::string &deviceId) -> json;

 /**
  * @brief Get available offset values and current/default offset.
  */
 auto getCameraOffsets(const std::string &deviceId) -> json;

 /**
  * @brief Set cooler power (manual mode placeholder).
  */
 auto setCoolerPower(const std::string &deviceId, double power,
                     const std::string &mode) -> json;

 /**
  * @brief Initiate camera warm-up sequence.
  */
 auto warmUpCamera(const std::string &deviceId) -> json;

 }  // namespace lithium::middleware

 #endif  // LITHIUM_SERVER_MIDDLEWARE_CAMERA_HPP
