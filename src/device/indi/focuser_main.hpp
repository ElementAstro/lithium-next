#ifndef LITHIUM_DEVICE_INDI_FOCUSER_MAIN_HPP
#define LITHIUM_DEVICE_INDI_FOCUSER_MAIN_HPP

// Include both implementations for compatibility
#include "focuser/modular_focuser.hpp"

// Legacy support - alias to modular implementation
namespace lithium::device::indi {
    using INDIFocuser = focuser::ModularINDIFocuser;
}

// Export the modular implementation as the default
using INDIFocuser = lithium::device::indi::focuser::ModularINDIFocuser;

#endif // LITHIUM_DEVICE_INDI_FOCUSER_MAIN_HPP
