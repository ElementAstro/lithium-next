#include "indi_focuser_core.hpp"

namespace lithium::device::indi::focuser {

INDIFocuserCore::INDIFocuserCore(std::string name)
    : name_(std::move(name)) {
    // Initialize logger
    logger_ = spdlog::get("focuser");
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }

    logger_->info("Creating INDI focuser core: {}", name_);
}

}  // namespace lithium::device::indi::focuser
