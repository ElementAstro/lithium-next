/*
 * event_handler.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: PHD2 event handler interface with modern C++ features

*************************************************/

#pragma once

#include "types.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace phd2 {

/**
 * @brief Interface for handling events from PHD2
 */
class EventHandler {
public:
    virtual ~EventHandler() = default;

    /**
     * @brief Called when an event is received from PHD2
     * @param event The event received
     */
    virtual void onEvent(const Event& event) = 0;

    /**
     * @brief Called when a connection error occurs
     * @param error The error message
     */
    virtual void onConnectionError(std::string_view error) = 0;

    /**
     * @brief Called when connection state changes
     * @param connected True if connected
     */
    virtual void onConnectionStateChanged([[maybe_unused]] bool connected) {}

    /**
     * @brief Called when guiding state changes
     * @param state New guiding state
     */
    virtual void onGuidingStateChanged([[maybe_unused]] AppStateType state) {}
};

/**
 * @brief Callback-based event handler implementation
 */
class CallbackEventHandler : public EventHandler {
public:
    using EventCallback = std::function<void(const Event&)>;
    using ErrorCallback = std::function<void(std::string_view)>;
    using StateCallback = std::function<void(bool)>;
    using GuidingCallback = std::function<void(AppStateType)>;

    void onEvent(const Event& event) override {
        if (eventCallback_) {
            eventCallback_(event);
        }
    }

    void onConnectionError(std::string_view error) override {
        if (errorCallback_) {
            errorCallback_(error);
        }
    }

    void onConnectionStateChanged(bool connected) override {
        if (stateCallback_) {
            stateCallback_(connected);
        }
    }

    void onGuidingStateChanged(AppStateType state) override {
        if (guidingCallback_) {
            guidingCallback_(state);
        }
    }

    void setEventCallback(EventCallback cb) { eventCallback_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }
    void setStateCallback(StateCallback cb) { stateCallback_ = std::move(cb); }
    void setGuidingCallback(GuidingCallback cb) { guidingCallback_ = std::move(cb); }

private:
    EventCallback eventCallback_;
    ErrorCallback errorCallback_;
    StateCallback stateCallback_;
    GuidingCallback guidingCallback_;
};

/**
 * @brief Event handler that dispatches to multiple handlers
 */
class CompositeEventHandler : public EventHandler {
public:
    void addHandler(std::shared_ptr<EventHandler> handler) {
        handlers_.push_back(std::move(handler));
    }

    void removeHandler(const std::shared_ptr<EventHandler>& handler) {
        handlers_.erase(
            std::remove(handlers_.begin(), handlers_.end(), handler),
            handlers_.end());
    }

    void onEvent(const Event& event) override {
        for (const auto& handler : handlers_) {
            handler->onEvent(event);
        }
    }

    void onConnectionError(std::string_view error) override {
        for (const auto& handler : handlers_) {
            handler->onConnectionError(error);
        }
    }

    void onConnectionStateChanged(bool connected) override {
        for (const auto& handler : handlers_) {
            handler->onConnectionStateChanged(connected);
        }
    }

    void onGuidingStateChanged(AppStateType state) override {
        for (const auto& handler : handlers_) {
            handler->onGuidingStateChanged(state);
        }
    }

private:
    std::vector<std::shared_ptr<EventHandler>> handlers_;
};

/**
 * @brief Event filter that only passes specific event types
 */
class FilteredEventHandler : public EventHandler {
public:
    explicit FilteredEventHandler(
        std::shared_ptr<EventHandler> target,
        std::vector<EventType> allowedTypes)
        : target_(std::move(target)),
          allowedTypes_(std::move(allowedTypes)) {}

    void onEvent(const Event& event) override {
        auto type = getEventType(event);
        if (std::find(allowedTypes_.begin(), allowedTypes_.end(), type) !=
            allowedTypes_.end()) {
            target_->onEvent(event);
        }
    }

    void onConnectionError(std::string_view error) override {
        target_->onConnectionError(error);
    }

private:
    std::shared_ptr<EventHandler> target_;
    std::vector<EventType> allowedTypes_;
};

}  // namespace phd2
