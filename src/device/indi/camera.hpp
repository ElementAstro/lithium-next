#ifndef LITHIUM_CLIENT_INDI_CAMERA_HPP
#define LITHIUM_CLIENT_INDI_CAMERA_HPP

// Forward declaration to new component-based implementation
#include "camera/indi_camera.hpp"

// Alias the new component-based implementation to maintain backward compatibility
using INDICamera = lithium::device::indi::camera::INDICamera;

#endif  // LITHIUM_CLIENT_INDI_CAMERA_HPP
