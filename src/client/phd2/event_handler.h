// phd2_event_handler.h
#pragma once

#include "types.h"

namespace phd2 {

/**
 * @brief Interface for handling events from PHD2
 *
 * Clients should implement this interface to receive and process
 * events from PHD2.
 */
class EventHandler {
public:
    virtual ~EventHandler() = default;

    /**
     * @brief Called when an event is received from PHD2
     *
     * @param event The event received
     */
    virtual void onEvent(const Event& event) = 0;

    /**
     * @brief Called when a connection error occurs
     *
     * @param error The error message
     */
    virtual void onConnectionError(const std::string& error) = 0;
};

}  // namespace phd2