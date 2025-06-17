/*
 * ascom_com_helper.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM COM Helper Implementation

*************************************************/

#include "ascom_com_helper.hpp"

#ifdef _WIN32

#include <sstream>
#include <algorithm>
#include <thread>

// ASCOMCOMHelper implementation
ASCOMCOMHelper::ASCOMCOMHelper() 
    : initialized_(false), last_hresult_(S_OK), property_caching_enabled_(true) {
}

ASCOMCOMHelper::~ASCOMCOMHelper() {
    cleanup();
}

bool ASCOMCOMHelper::initialize() {
    if (initialized_) {
        return true;
    }
    
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        setError("Failed to initialize COM", hr);
        return false;
    }
    
    // Initialize security
    hr = CoInitializeSecurity(
        nullptr,                     // Security descriptor
        -1,                          // COM authentication
        nullptr,                     // Authentication services
        nullptr,                     // Reserved
        RPC_C_AUTHN_LEVEL_NONE,     // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        nullptr,                     // Authentication info
        EOAC_NONE,                  // Additional capabilities
        nullptr                      // Reserved
    );
    
    // Security initialization can fail if already initialized, which is OK
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        LOG_F(WARNING, "COM security initialization failed: {}", formatCOMError(hr));
    }
    
    initialized_ = true;
    clearError();
    return true;
}

void ASCOMCOMHelper::cleanup() {
    if (initialized_) {
        clearPropertyCache();
        method_cache_.clear();
        CoUninitialize();
        initialized_ = false;
    }
}

std::optional<COMObjectWrapper> ASCOMCOMHelper::createObject(const std::string& progId) {
    if (!initialized_) {
        setError("COM not initialized");
        return std::nullopt;
    }
    
    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        setError("Failed to get CLSID from ProgID: " + progId, hr);
        return std::nullopt;
    }
    
    return createObjectFromCLSID(clsid);
}

std::optional<COMObjectWrapper> ASCOMCOMHelper::createObjectFromCLSID(const CLSID& clsid) {
    if (!initialized_) {
        setError("COM not initialized");
        return std::nullopt;
    }
    
    IDispatch* dispatch = nullptr;
    HRESULT hr = CoCreateInstance(
        clsid,
        nullptr,
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch,
        reinterpret_cast<void**>(&dispatch)
    );
    
    if (FAILED(hr)) {
        setError("Failed to create COM instance", hr);
        return std::nullopt;
    }
    
    clearError();
    return COMObjectWrapper(dispatch);
}

std::optional<VariantWrapper> ASCOMCOMHelper::getProperty(IDispatch* object, const std::string& property) {
    if (!object) {
        setError("Invalid object pointer");
        return std::nullopt;
    }
    
    // Check cache first
    if (property_caching_enabled_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto cacheKey = buildCacheKey(object, property);
        auto it = property_cache_.find(cacheKey);
        if (it != property_cache_.end()) {
            return VariantWrapper(it->second.get());
        }
    }
    
    auto dispId = getDispatchId(object, property);
    if (!dispId) {
        return std::nullopt;
    }
    
    DISPPARAMS dispParams = { nullptr, nullptr, 0, 0 };
    VariantWrapper result;
    
    HRESULT hr = object->Invoke(
        *dispId,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispParams,
        &result.get(),
        nullptr,
        nullptr
    );
    
    if (FAILED(hr)) {
        setError("Failed to get property: " + property, hr);
        return std::nullopt;
    }
    
    // Cache the result
    if (property_caching_enabled_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto cacheKey = buildCacheKey(object, property);
        property_cache_[cacheKey] = VariantWrapper(result.get());
    }
    
    clearError();
    return result;
}

bool ASCOMCOMHelper::setProperty(IDispatch* object, const std::string& property, const VariantWrapper& value) {
    if (!object) {
        setError("Invalid object pointer");
        return false;
    }
    
    auto dispId = getDispatchId(object, property);
    if (!dispId) {
        return false;
    }
    
    VARIANT var = value.get();
    VARIANT* params[] = { &var };
    DISPID dispIdPut = DISPID_PROPERTYPUT;
    
    DISPPARAMS dispParams = {
        params,
        &dispIdPut,
        1,
        1
    };
    
    HRESULT hr = object->Invoke(
        *dispId,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &dispParams,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (FAILED(hr)) {
        setError("Failed to set property: " + property, hr);
        return false;
    }
    
    // Invalidate cache
    if (property_caching_enabled_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto cacheKey = buildCacheKey(object, property);
        property_cache_.erase(cacheKey);
    }
    
    clearError();
    return true;
}

std::optional<VariantWrapper> ASCOMCOMHelper::invokeMethod(IDispatch* object, const std::string& method) {
    std::vector<VariantWrapper> emptyParams;
    return invokeMethod(object, method, emptyParams);
}

std::optional<VariantWrapper> ASCOMCOMHelper::invokeMethod(IDispatch* object, const std::string& method, 
                                                           const std::vector<VariantWrapper>& params) {
    if (!object) {
        setError("Invalid object pointer");
        return std::nullopt;
    }
    
    auto dispId = getDispatchId(object, method);
    if (!dispId) {
        return std::nullopt;
    }
    
    return invokeMethodInternal(object, *dispId, DISPATCH_METHOD, params);
}

std::optional<VariantWrapper> ASCOMCOMHelper::invokeMethodWithNamedParams(IDispatch* object, const std::string& method,
                                                                          const std::unordered_map<std::string, VariantWrapper>& namedParams) {
    if (!object || namedParams.empty()) {
        setError("Invalid parameters for named method invocation");
        return std::nullopt;
    }
    
    // Get method DISPID
    auto methodDispId = getDispatchId(object, method);
    if (!methodDispId) {
        return std::nullopt;
    }
    
    // Get DISPIDs for parameter names
    std::vector<DISPID> paramDispIds;
    std::vector<VariantWrapper> paramValues;
    std::vector<LPOLESTR> paramNames;
    
    for (const auto& [name, value] : namedParams) {
        CComBSTR bstrName(name.c_str());
        paramNames.push_back(bstrName);
        paramValues.push_back(VariantWrapper(value.get()));
    }
    
    paramDispIds.resize(paramNames.size());
    HRESULT hr = object->GetIDsOfNames(
        IID_NULL,
        paramNames.data(),
        static_cast<UINT>(paramNames.size()),
        LOCALE_USER_DEFAULT,
        paramDispIds.data()
    );
    
    if (FAILED(hr)) {
        setError("Failed to get parameter DISPIDs for method: " + method, hr);
        return std::nullopt;
    }
    
    // Prepare DISPPARAMS with named parameters
    std::vector<VARIANT> variants;
    for (const auto& wrapper : paramValues) {
        variants.push_back(wrapper.get());
    }
    
    DISPPARAMS dispParams = {
        variants.data(),
        paramDispIds.data(),
        static_cast<UINT>(variants.size()),
        static_cast<UINT>(paramDispIds.size())
    };
    
    VariantWrapper result;
    hr = object->Invoke(
        *methodDispId,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &dispParams,
        &result.get(),
        nullptr,
        nullptr
    );
    
    if (FAILED(hr)) {
        setError("Failed to invoke method with named parameters: " + method, hr);
        return std::nullopt;
    }
    
    clearError();
    return result;
}

bool ASCOMCOMHelper::setMultipleProperties(IDispatch* object, const std::unordered_map<std::string, VariantWrapper>& properties) {
    if (!object || properties.empty()) {
        return false;
    }
    
    bool allSuccess = true;
    for (const auto& [property, value] : properties) {
        if (!setProperty(object, property, value)) {
            allSuccess = false;
            LOG_F(ERROR, "Failed to set property: {}", property);
        }
    }
    
    return allSuccess;
}

std::unordered_map<std::string, VariantWrapper> ASCOMCOMHelper::getMultipleProperties(IDispatch* object, 
                                                                                       const std::vector<std::string>& properties) {
    std::unordered_map<std::string, VariantWrapper> results;
    
    if (!object || properties.empty()) {
        return results;
    }
    
    for (const auto& property : properties) {
        auto value = getProperty(object, property);
        if (value) {
            results[property] = std::move(*value);
        }
    }
    
    return results;
}

std::optional<std::vector<VariantWrapper>> ASCOMCOMHelper::safeArrayToVector(SAFEARRAY* pArray) {
    if (!pArray) {
        return std::nullopt;
    }
    
    VARTYPE vt;
    HRESULT hr = SafeArrayGetVartype(pArray, &vt);
    if (FAILED(hr)) {
        setError("Failed to get SafeArray type", hr);
        return std::nullopt;
    }
    
    long lBound, uBound;
    hr = SafeArrayGetLBound(pArray, 1, &lBound);
    if (FAILED(hr)) {
        setError("Failed to get SafeArray lower bound", hr);
        return std::nullopt;
    }
    
    hr = SafeArrayGetUBound(pArray, 1, &uBound);
    if (FAILED(hr)) {
        setError("Failed to get SafeArray upper bound", hr);
        return std::nullopt;
    }
    
    std::vector<VariantWrapper> result;
    result.reserve(uBound - lBound + 1);
    
    void* pData;
    hr = SafeArrayAccessData(pArray, &pData);
    if (FAILED(hr)) {
        setError("Failed to access SafeArray data", hr);
        return std::nullopt;
    }
    
    for (long i = lBound; i <= uBound; ++i) {
        VariantWrapper wrapper;
        
        switch (vt) {
            case VT_BSTR: {
                BSTR* bstrArray = static_cast<BSTR*>(pData);
                wrapper = VariantWrapper::fromString(_bstr_t(bstrArray[i - lBound]));
                break;
            }
            case VT_I4: {
                int* intArray = static_cast<int*>(pData);
                wrapper = VariantWrapper::fromInt(intArray[i - lBound]);
                break;
            }
            case VT_R8: {
                double* doubleArray = static_cast<double*>(pData);
                wrapper = VariantWrapper::fromDouble(doubleArray[i - lBound]);
                break;
            }
            case VT_BOOL: {
                VARIANT_BOOL* boolArray = static_cast<VARIANT_BOOL*>(pData);
                wrapper = VariantWrapper::fromBool(boolArray[i - lBound] == VARIANT_TRUE);
                break;
            }
            default:
                // Handle other types as needed
                break;
        }
        
        result.push_back(std::move(wrapper));
    }
    
    SafeArrayUnaccessData(pArray);
    clearError();
    return result;
}

bool ASCOMCOMHelper::testConnection(IDispatch* object) {
    if (!object) {
        return false;
    }
    
    // Try to get a basic property like "Name" or "Connected"
    auto result = getProperty(object, "Name");
    if (!result) {
        result = getProperty(object, "Connected");
    }
    
    return result.has_value();
}

bool ASCOMCOMHelper::isObjectValid(IDispatch* object) {
    if (!object) {
        return false;
    }
    
    // Try to get type information
    ITypeInfo* typeInfo = nullptr;
    HRESULT hr = object->GetTypeInfo(0, LOCALE_USER_DEFAULT, &typeInfo);
    
    if (typeInfo) {
        typeInfo->Release();
    }
    
    return SUCCEEDED(hr);
}

std::vector<std::string> ASCOMCOMHelper::enumerateASCOMDrivers(const std::string& deviceType) {
    std::vector<std::string> drivers;
    
    std::string keyPath = "SOFTWARE\\ASCOM\\" + deviceType + " Drivers";
    
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS) {
        return drivers;
    }
    
    DWORD index = 0;
    char subKeyName[MAX_PATH];
    DWORD subKeyNameSize = MAX_PATH;
    
    while (RegEnumKeyExA(hKey, index, subKeyName, &subKeyNameSize, 
                         nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        
        drivers.push_back(std::string(subKeyName));
        
        ++index;
        subKeyNameSize = MAX_PATH;
    }
    
    RegCloseKey(hKey);
    return drivers;
}

std::optional<std::string> ASCOMCOMHelper::getDriverInfo(const std::string& progId) {
    auto object = createObject(progId);
    if (!object) {
        return std::nullopt;
    }
    
    auto result = getProperty(object->get(), "DriverInfo");
    if (result) {
        return result->toString();
    }
    
    return std::nullopt;
}

void ASCOMCOMHelper::clearError() {
    last_error_.clear();
    last_hresult_ = S_OK;
}

std::string ASCOMCOMHelper::formatCOMError(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << hr;
    
    // Add description if available
    _com_error error(hr);
    if (error.ErrorMessage()) {
        oss << " (" << error.ErrorMessage() << ")";
    }
    
    return oss.str();
}

std::string ASCOMCOMHelper::guidToString(const GUID& guid) {
    char guidString[39];
    sprintf_s(guidString, sizeof(guidString),
        "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    
    return std::string(guidString);
}

std::optional<GUID> ASCOMCOMHelper::stringToGuid(const std::string& str) {
    GUID guid;
    HRESULT hr = CLSIDFromString(CComBSTR(str.c_str()), &guid);
    
    if (SUCCEEDED(hr)) {
        return guid;
    }
    
    return std::nullopt;
}

// Private helper methods
std::optional<DISPID> ASCOMCOMHelper::getDispatchId(IDispatch* object, const std::string& name) {
    if (!object) {
        return std::nullopt;
    }
    
    // Check cache first
    std::string cacheKey = std::to_string(reinterpret_cast<uintptr_t>(object)) + ":" + name;
    {
        std::lock_guard<std::mutex> lock(method_cache_mutex_);
        auto it = method_cache_.find(cacheKey);
        if (it != method_cache_.end()) {
            return it->second;
        }
    }
    
    DISPID dispId;
    CComBSTR bstrName(name.c_str());
    HRESULT hr = object->GetIDsOfNames(IID_NULL, &bstrName, 1, LOCALE_USER_DEFAULT, &dispId);
    
    if (FAILED(hr)) {
        setError("Failed to get DISPID for: " + name, hr);
        return std::nullopt;
    }
    
    // Cache the result
    {
        std::lock_guard<std::mutex> lock(method_cache_mutex_);
        method_cache_[cacheKey] = dispId;
    }
    
    return dispId;
}

void ASCOMCOMHelper::setError(const std::string& error, HRESULT hr) {
    last_error_ = error;
    last_hresult_ = hr;
    
    std::string fullError = error;
    if (hr != S_OK) {
        fullError += " (" + formatCOMError(hr) + ")";
    }
    
    LOG_F(ERROR, "ASCOM COM Error: {}", fullError);
}

std::string ASCOMCOMHelper::buildCacheKey(IDispatch* object, const std::string& property) {
    return std::to_string(reinterpret_cast<uintptr_t>(object)) + ":" + property;
}

std::optional<VariantWrapper> ASCOMCOMHelper::invokeMethodInternal(IDispatch* object, DISPID dispId, 
                                                                   WORD flags, const std::vector<VariantWrapper>& params) {
    std::vector<VARIANT> variants;
    variants.reserve(params.size());
    
    // Convert parameters (note: COM expects parameters in reverse order)
    for (auto it = params.rbegin(); it != params.rend(); ++it) {
        variants.push_back(it->get());
    }
    
    DISPPARAMS dispParams = {
        variants.empty() ? nullptr : variants.data(),
        nullptr,
        static_cast<UINT>(variants.size()),
        0
    };
    
    VariantWrapper result;
    HRESULT hr = object->Invoke(
        dispId,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        flags,
        &dispParams,
        &result.get(),
        nullptr,
        nullptr
    );
    
    if (FAILED(hr)) {
        setError("Method invocation failed", hr);
        return std::nullopt;
    }
    
    clearError();
    return result;
}

// COMInitializer implementation
COMInitializer::COMInitializer(DWORD coinitFlags) : initialized_(false) {
    init_result_ = CoInitializeEx(nullptr, coinitFlags);
    
    if (SUCCEEDED(init_result_) || init_result_ == RPC_E_CHANGED_MODE) {
        initialized_ = true;
    }
}

COMInitializer::~COMInitializer() {
    if (initialized_) {
        CoUninitialize();
    }
}

// ASCOMDeviceHelper implementation
ASCOMDeviceHelper::ASCOMDeviceHelper(std::shared_ptr<ASCOMCOMHelper> comHelper) 
    : com_helper_(comHelper) {
}

bool ASCOMDeviceHelper::connectToDevice(const std::string& progId) {
    device_prog_id_ = progId;
    
    auto object = com_helper_->createObject(progId);
    if (!object) {
        last_device_error_ = com_helper_->getLastError();
        return false;
    }
    
    device_object_ = std::move(*object);
    
    if (!validateDevice()) {
        device_object_.reset();
        return false;
    }
    
    // Set Connected = true
    if (!setConnected(true)) {
        device_object_.reset();
        return false;
    }
    
    clearDeviceError();
    return true;
}

bool ASCOMDeviceHelper::connectToDevice(const CLSID& clsid) {
    auto object = com_helper_->createObjectFromCLSID(clsid);
    if (!object) {
        last_device_error_ = com_helper_->getLastError();
        return false;
    }
    
    device_object_ = std::move(*object);
    
    if (!validateDevice()) {
        device_object_.reset();
        return false;
    }
    
    if (!setConnected(true)) {
        device_object_.reset();
        return false;
    }
    
    clearDeviceError();
    return true;
}

void ASCOMDeviceHelper::disconnectFromDevice() {
    if (device_object_.isValid()) {
        setConnected(false);
        device_object_.reset();
    }
    clearDeviceError();
}

std::optional<std::string> ASCOMDeviceHelper::getDriverInfo() {
    return getDeviceProperty<std::string>("DriverInfo");
}

std::optional<std::string> ASCOMDeviceHelper::getDriverVersion() {
    return getDeviceProperty<std::string>("DriverVersion");
}

std::optional<std::string> ASCOMDeviceHelper::getName() {
    return getDeviceProperty<std::string>("Name");
}

std::optional<std::string> ASCOMDeviceHelper::getDescription() {
    return getDeviceProperty<std::string>("Description");
}

std::optional<bool> ASCOMDeviceHelper::isConnected() {
    return getDeviceProperty<bool>("Connected");
}

bool ASCOMDeviceHelper::setConnected(bool connected) {
    return setDeviceProperty("Connected", connected);
}

std::optional<std::vector<std::string>> ASCOMDeviceHelper::getSupportedActions() {
    if (!device_object_.isValid()) {
        return std::nullopt;
    }
    
    auto result = com_helper_->getProperty(device_object_.get(), "SupportedActions");
    if (!result) {
        return std::nullopt;
    }
    
    // Handle SafeArray of strings
    if (result->get().vt == (VT_ARRAY | VT_BSTR)) {
        auto vectorResult = com_helper_->safeArrayToVector(result->get().parray);
        if (vectorResult) {
            std::vector<std::string> actions;
            for (const auto& wrapper : *vectorResult) {
                auto str = wrapper.toString();
                if (str) {
                    actions.push_back(*str);
                }
            }
            return actions;
        }
    }
    
    return std::nullopt;
}

std::unordered_map<std::string, VariantWrapper> ASCOMDeviceHelper::discoverCapabilities() {
    std::unordered_map<std::string, VariantWrapper> capabilities;
    
    if (!device_object_.isValid()) {
        return capabilities;
    }
    
    // Common ASCOM properties to discover
    std::vector<std::string> commonProperties = {
        "Name", "Description", "DriverInfo", "DriverVersion", "InterfaceVersion",
        "SupportedActions", "Connected"
    };
    
    return com_helper_->getMultipleProperties(device_object_.get(), commonProperties);
}

bool ASCOMDeviceHelper::validateDevice() {
    if (!device_object_.isValid()) {
        last_device_error_ = "Invalid device object";
        return false;
    }
    
    // Check if object supports basic ASCOM interface
    auto name = getDeviceProperty<std::string>("Name");
    if (!name) {
        last_device_error_ = "Device does not support ASCOM Name property";
        return false;
    }
    
    return true;
}

std::string ASCOMDeviceHelper::getLastDeviceError() const {
    return last_device_error_;
}

void ASCOMDeviceHelper::clearDeviceError() {
    last_device_error_.clear();
}

#endif // _WIN32
