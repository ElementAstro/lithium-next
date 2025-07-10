/*
 * ascom_com_helper.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM COM Helper Utilities

*************************************************/

#pragma once

#ifdef _WIN32

#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#include <oleauto.h>
#include <wbemidl.h>
#include <atlbase.h>
#include <atlcom.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

#include <spdlog/spdlog.h>

// COM object wrapper with automatic cleanup
class COMObjectWrapper {
public:
    explicit COMObjectWrapper(IDispatch* dispatch = nullptr) 
        : dispatch_(dispatch) {
        if (dispatch_) {
            dispatch_->AddRef();
        }
    }
    
    ~COMObjectWrapper() {
        if (dispatch_) {
            dispatch_->Release();
            dispatch_ = nullptr;
        }
    }
    
    // Move constructor
    COMObjectWrapper(COMObjectWrapper&& other) noexcept 
        : dispatch_(other.dispatch_) {
        other.dispatch_ = nullptr;
    }
    
    // Move assignment
    COMObjectWrapper& operator=(COMObjectWrapper&& other) noexcept {
        if (this != &other) {
            if (dispatch_) {
                dispatch_->Release();
            }
            dispatch_ = other.dispatch_;
            other.dispatch_ = nullptr;
        }
        return *this;
    }
    
    // Disable copy
    COMObjectWrapper(const COMObjectWrapper&) = delete;
    COMObjectWrapper& operator=(const COMObjectWrapper&) = delete;
    
    IDispatch* get() const { return dispatch_; }
    IDispatch* release() {
        IDispatch* temp = dispatch_;
        dispatch_ = nullptr;
        return temp;
    }
    
    bool isValid() const { return dispatch_ != nullptr; }
    
    void reset(IDispatch* dispatch = nullptr) {
        if (dispatch_) {
            dispatch_->Release();
        }
        dispatch_ = dispatch;
        if (dispatch_) {
            dispatch_->AddRef();
        }
    }

private:
    IDispatch* dispatch_;
};

// Variant wrapper with automatic cleanup
class VariantWrapper {
public:
    VariantWrapper() {
        VariantInit(&variant_);
    }
    
    explicit VariantWrapper(const VARIANT& var) {
        VariantInit(&variant_);
        VariantCopy(&variant_, &var);
    }
    
    ~VariantWrapper() {
        VariantClear(&variant_);
    }
    
    // Move constructor
    VariantWrapper(VariantWrapper&& other) noexcept {
        variant_ = other.variant_;
        VariantInit(&other.variant_);
    }
    
    // Move assignment
    VariantWrapper& operator=(VariantWrapper&& other) noexcept {
        if (this != &other) {
            VariantClear(&variant_);
            variant_ = other.variant_;
            VariantInit(&other.variant_);
        }
        return *this;
    }
    
    // Disable copy
    VariantWrapper(const VariantWrapper&) = delete;
    VariantWrapper& operator=(const VariantWrapper&) = delete;
    
    VARIANT& get() { return variant_; }
    const VARIANT& get() const { return variant_; }
    
    VARIANT* operator&() { return &variant_; }
    const VARIANT* operator&() const { return &variant_; }
    
    // Conversion helpers
    std::optional<std::string> toString() const {
        if (variant_.vt == VT_BSTR && variant_.bstrVal) {
            return std::string(_bstr_t(variant_.bstrVal));
        }
        
        // Try to convert other types to string
        VariantWrapper temp;
        if (SUCCEEDED(VariantChangeType(&temp.variant_, &variant_, 0, VT_BSTR))) {
            if (temp.variant_.bstrVal) {
                return std::string(_bstr_t(temp.variant_.bstrVal));
            }
        }
        
        return std::nullopt;
    }
    
    std::optional<int> toInt() const {
        if (variant_.vt == VT_I4) {
            return variant_.intVal;
        }
        
        VariantWrapper temp;
        if (SUCCEEDED(VariantChangeType(&temp.variant_, &variant_, 0, VT_I4))) {
            return temp.variant_.intVal;
        }
        
        return std::nullopt;
    }
    
    std::optional<double> toDouble() const {
        if (variant_.vt == VT_R8) {
            return variant_.dblVal;
        }
        
        VariantWrapper temp;
        if (SUCCEEDED(VariantChangeType(&temp.variant_, &variant_, 0, VT_R8))) {
            return temp.variant_.dblVal;
        }
        
        return std::nullopt;
    }
    
    std::optional<bool> toBool() const {
        if (variant_.vt == VT_BOOL) {
            return variant_.boolVal == VARIANT_TRUE;
        }
        
        VariantWrapper temp;
        if (SUCCEEDED(VariantChangeType(&temp.variant_, &variant_, 0, VT_BOOL))) {
            return temp.variant_.boolVal == VARIANT_TRUE;
        }
        
        return std::nullopt;
    }
    
    // Factory methods
    static VariantWrapper fromString(const std::string& str) {
        VariantWrapper wrapper;
        wrapper.variant_.vt = VT_BSTR;
        wrapper.variant_.bstrVal = SysAllocString(CComBSTR(str.c_str()));
        return wrapper;
    }
    
    static VariantWrapper fromInt(int value) {
        VariantWrapper wrapper;
        wrapper.variant_.vt = VT_I4;
        wrapper.variant_.intVal = value;
        return wrapper;
    }
    
    static VariantWrapper fromDouble(double value) {
        VariantWrapper wrapper;
        wrapper.variant_.vt = VT_R8;
        wrapper.variant_.dblVal = value;
        return wrapper;
    }
    
    static VariantWrapper fromBool(bool value) {
        VariantWrapper wrapper;
        wrapper.variant_.vt = VT_BOOL;
        wrapper.variant_.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
        return wrapper;
    }

private:
    VARIANT variant_;
};

// Advanced COM helper class
class ASCOMCOMHelper {
public:
    ASCOMCOMHelper();
    ~ASCOMCOMHelper();
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Object creation and management
    std::optional<COMObjectWrapper> createObject(const std::string& progId);
    std::optional<COMObjectWrapper> createObjectFromCLSID(const CLSID& clsid);
    
    // Property operations with caching
    std::optional<VariantWrapper> getProperty(IDispatch* object, const std::string& property);
    bool setProperty(IDispatch* object, const std::string& property, const VariantWrapper& value);
    
    // Method invocation with parameter support
    std::optional<VariantWrapper> invokeMethod(IDispatch* object, const std::string& method);
    std::optional<VariantWrapper> invokeMethod(IDispatch* object, const std::string& method, 
                                               const std::vector<VariantWrapper>& params);
    
    // Advanced method invocation with named parameters
    std::optional<VariantWrapper> invokeMethodWithNamedParams(IDispatch* object, const std::string& method,
                                                               const std::unordered_map<std::string, VariantWrapper>& namedParams);
    
    // Batch operations
    bool setMultipleProperties(IDispatch* object, const std::unordered_map<std::string, VariantWrapper>& properties);
    std::unordered_map<std::string, VariantWrapper> getMultipleProperties(IDispatch* object, 
                                                                           const std::vector<std::string>& properties);
    
    // Array handling
    std::optional<std::vector<VariantWrapper>> safeArrayToVector(SAFEARRAY* pArray);
    std::optional<SAFEARRAY*> vectorToSafeArray(const std::vector<VariantWrapper>& vector, VARTYPE vt);
    
    // Connection testing
    bool testConnection(IDispatch* object);
    bool isObjectValid(IDispatch* object);
    
    // Error handling and diagnostics
    std::string getLastError() const { return last_error_; }
    HRESULT getLastHResult() const { return last_hresult_; }
    void clearError();
    
    // Event handling support
    bool connectToEvents(IDispatch* object, const std::string& interfaceId);
    void disconnectFromEvents(IDispatch* object);
    
    // Registry operations for ASCOM discovery
    std::vector<std::string> enumerateASCOMDrivers(const std::string& deviceType);
    std::optional<std::string> getDriverInfo(const std::string& progId);
    
    // Performance optimization
    void enablePropertyCaching(bool enable) { property_caching_enabled_ = enable; }
    void clearPropertyCache() { property_cache_.clear(); }
    
    // Threaded operations
    template<typename Func>
    auto executeInSTAThread(Func&& func) -> decltype(func());
    
    // Utility functions
    static std::string formatCOMError(HRESULT hr);
    static std::string guidToString(const GUID& guid);
    static std::optional<GUID> stringToGuid(const std::string& str);

private:
    bool initialized_;
    std::string last_error_;
    HRESULT last_hresult_;
    
    // Property caching
    bool property_caching_enabled_;
    std::unordered_map<std::string, VariantWrapper> property_cache_;
    std::mutex cache_mutex_;
    
    // Method lookup cache
    std::unordered_map<std::string, DISPID> method_cache_;
    std::mutex method_cache_mutex_;
    
    // Helper methods
    std::optional<DISPID> getDispatchId(IDispatch* object, const std::string& name);
    void setError(const std::string& error, HRESULT hr = S_OK);
    std::string buildCacheKey(IDispatch* object, const std::string& property);
    
    // Internal method invocation
    std::optional<VariantWrapper> invokeMethodInternal(IDispatch* object, DISPID dispId, 
                                                       WORD flags, const std::vector<VariantWrapper>& params);
};

// RAII COM initialization helper
class COMInitializer {
public:
    explicit COMInitializer(DWORD coinitFlags = COINIT_APARTMENTTHREADED);
    ~COMInitializer();
    
    bool isInitialized() const { return initialized_; }
    HRESULT getInitResult() const { return init_result_; }

private:
    bool initialized_;
    HRESULT init_result_;
};

// Exception class for COM errors
class COMException : public std::exception {
public:
    explicit COMException(const std::string& message, HRESULT hr = S_OK)
        : message_(message), hresult_(hr) {
        full_message_ = message_ + " (HRESULT: " + ASCOMCOMHelper::formatCOMError(hr) + ")";
    }
    
    const char* what() const noexcept override {
        return full_message_.c_str();
    }
    
    HRESULT getHResult() const { return hresult_; }
    const std::string& getMessage() const { return message_; }

private:
    std::string message_;
    std::string full_message_;
    HRESULT hresult_;
};

// Specialized ASCOM device helper
class ASCOMDeviceHelper {
public:
    explicit ASCOMDeviceHelper(std::shared_ptr<ASCOMCOMHelper> comHelper);
    
    // Device connection
    bool connectToDevice(const std::string& progId);
    bool connectToDevice(const CLSID& clsid);
    void disconnectFromDevice();
    
    // Standard ASCOM properties
    std::optional<std::string> getDriverInfo();
    std::optional<std::string> getDriverVersion();
    std::optional<std::string> getName();
    std::optional<std::string> getDescription();
    std::optional<bool> isConnected();
    bool setConnected(bool connected);
    
    // Common ASCOM methods
    std::optional<std::vector<std::string>> getSupportedActions();
    std::optional<std::string> getAction(const std::string& actionName, const std::string& parameters = "");
    bool setAction(const std::string& actionName, const std::string& parameters = "");
    
    // Device-specific property access
    template<typename T>
    std::optional<T> getDeviceProperty(const std::string& property);
    
    template<typename T>
    bool setDeviceProperty(const std::string& property, const T& value);
    
    // Device capabilities discovery
    std::unordered_map<std::string, VariantWrapper> discoverCapabilities();
    
    // Error handling
    std::string getLastDeviceError() const;
    void clearDeviceError();

private:
    std::shared_ptr<ASCOMCOMHelper> com_helper_;
    COMObjectWrapper device_object_;
    std::string device_prog_id_;
    std::string last_device_error_;
    
    bool validateDevice();
};

// Template implementations
template<typename Func>
auto ASCOMCOMHelper::executeInSTAThread(Func&& func) -> decltype(func()) {
    // Implementation for STA thread execution
    // This would create a new STA thread if needed and execute the function
    return func(); // Simplified for now
}

template<typename T>
std::optional<T> ASCOMDeviceHelper::getDeviceProperty(const std::string& property) {
    if (!device_object_.isValid()) {
        return std::nullopt;
    }
    
    auto result = com_helper_->getProperty(device_object_.get(), property);
    if (!result) {
        return std::nullopt;
    }
    
    // Type-specific conversion
    if constexpr (std::is_same_v<T, std::string>) {
        return result->toString();
    } else if constexpr (std::is_same_v<T, int>) {
        return result->toInt();
    } else if constexpr (std::is_same_v<T, double>) {
        return result->toDouble();
    } else if constexpr (std::is_same_v<T, bool>) {
        return result->toBool();
    }
    
    return std::nullopt;
}

template<typename T>
bool ASCOMDeviceHelper::setDeviceProperty(const std::string& property, const T& value) {
    if (!device_object_.isValid()) {
        return false;
    }
    
    VariantWrapper variant;
    
    // Type-specific conversion
    if constexpr (std::is_same_v<T, std::string>) {
        variant = VariantWrapper::fromString(value);
    } else if constexpr (std::is_same_v<T, int>) {
        variant = VariantWrapper::fromInt(value);
    } else if constexpr (std::is_same_v<T, double>) {
        variant = VariantWrapper::fromDouble(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        variant = VariantWrapper::fromBool(value);
    } else {
        return false;
    }
    
    return com_helper_->setProperty(device_object_.get(), property, variant);
}

#endif // _WIN32
