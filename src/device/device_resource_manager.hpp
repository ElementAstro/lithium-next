/*
 * device_resource_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Device Resource Management System for optimized resource allocation

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

#include "template/device.hpp"

namespace lithium {

// Resource types
enum class ResourceType {
    CPU,
    MEMORY,
    BANDWIDTH,
    STORAGE,
    CONCURRENT_OPERATIONS,
    CUSTOM
};

// Resource allocation
struct ResourceAllocation {
    ResourceType type;
    std::string name;
    double amount;
    double max_amount;
    std::string unit;
    std::chrono::system_clock::time_point allocated_at;
    std::chrono::milliseconds lease_duration{0};
    bool is_exclusive{false};
};

// Resource constraint
struct ResourceConstraint {
    ResourceType type;
    double min_amount;
    double max_amount;
    double preferred_amount;
    int priority;
    bool is_critical{false};
};

// Resource pool configuration
struct ResourcePoolConfig {
    ResourceType type;
    std::string name;
    double total_capacity;
    double reserved_capacity{0.0};
    double warning_threshold{0.8};
    double critical_threshold{0.95};
    bool enable_overcommit{false};
    double overcommit_ratio{1.2};
    std::chrono::seconds default_lease_duration{300};
};

// Resource usage statistics
struct ResourceUsageStats {
    ResourceType type;
    std::string name;
    double current_usage;
    double peak_usage;
    double average_usage;
    double allocated_amount;
    double available_amount;
    double utilization_rate;
    size_t allocation_count;
    size_t active_allocations;
    std::chrono::system_clock::time_point last_update;
};

// Resource scheduling policy
enum class SchedulingPolicy {
    FIRST_COME_FIRST_SERVED,
    PRIORITY_BASED,
    ROUND_ROBIN,
    SHORTEST_JOB_FIRST,
    FAIR_SHARE,
    ADAPTIVE
};

// Resource request
struct ResourceRequest {
    std::string device_name;
    std::string request_id;
    std::vector<ResourceConstraint> constraints;
    int priority{0};
    std::chrono::milliseconds max_wait_time{30000};
    std::chrono::milliseconds estimated_duration{0};
    std::function<void(bool, const std::string&)> completion_callback;
    
    // Advanced features
    bool allow_partial_allocation{false};
    bool allow_preemption{false};
    std::vector<std::string> preferred_nodes;
    std::vector<std::string> excluded_nodes;
};

// Resource lease
struct ResourceLease {
    std::string lease_id;
    std::string device_name;
    std::vector<ResourceAllocation> allocations;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    bool is_active{true};
    bool is_renewable{true};
    int renewal_count{0};
    int max_renewals{3};
};

class DeviceResourceManager {
public:
    DeviceResourceManager();
    ~DeviceResourceManager();
    
    // Resource pool management
    void createResourcePool(const ResourcePoolConfig& config);
    void removeResourcePool(const std::string& pool_name);
    void updateResourcePool(const std::string& pool_name, const ResourcePoolConfig& config);
    std::vector<ResourcePoolConfig> getResourcePools() const;
    
    // Resource allocation
    std::string requestResources(const ResourceRequest& request);
    bool allocateResources(const std::string& request_id);
    void releaseResources(const std::string& lease_id);
    void releaseDeviceResources(const std::string& device_name);
    
    // Lease management
    std::string createLease(const std::string& device_name, 
                          const std::vector<ResourceAllocation>& allocations,
                          std::chrono::milliseconds duration);
    bool renewLease(const std::string& lease_id, std::chrono::milliseconds extension);
    void revokeLease(const std::string& lease_id);
    ResourceLease getLease(const std::string& lease_id) const;
    std::vector<ResourceLease> getActiveLeases() const;
    std::vector<ResourceLease> getDeviceLeases(const std::string& device_name) const;
    
    // Resource monitoring
    ResourceUsageStats getResourceUsage(const std::string& pool_name) const;
    std::vector<ResourceUsageStats> getAllResourceUsage() const;
    double getResourceUtilization(const std::string& pool_name) const;
    
    // Scheduling
    void setSchedulingPolicy(SchedulingPolicy policy);
    SchedulingPolicy getSchedulingPolicy() const;
    void setResourcePriority(const std::string& device_name, int priority);
    int getResourcePriority(const std::string& device_name) const;
    
    // Queue management
    size_t getQueueSize() const;
    size_t getQueueSize(const std::string& pool_name) const;
    std::vector<ResourceRequest> getPendingRequests() const;
    void cancelRequest(const std::string& request_id);
    void cancelDeviceRequests(const std::string& device_name);
    
    // Resource optimization
    void enableAutoOptimization(bool enable);
    bool isAutoOptimizationEnabled() const;
    void runOptimization();
    
    struct OptimizationResult {
        std::string pool_name;
        std::string action;
        double old_capacity;
        double new_capacity;
        double expected_improvement;
        std::string rationale;
    };
    
    std::vector<OptimizationResult> getOptimizationSuggestions() const;
    void applyOptimization(const OptimizationResult& result);
    
    // Resource preemption
    void enablePreemption(bool enable);
    bool isPreemptionEnabled() const;
    void preemptResources(const std::string& device_name);
    
    // Resource reservation
    std::string reserveResources(const std::string& device_name,
                               const std::vector<ResourceConstraint>& constraints,
                               std::chrono::system_clock::time_point start_time,
                               std::chrono::milliseconds duration);
    void cancelReservation(const std::string& reservation_id);
    std::vector<std::string> getActiveReservations() const;
    
    // Load balancing
    void enableLoadBalancing(bool enable);
    bool isLoadBalancingEnabled() const;
    std::string selectOptimalNode(const ResourceRequest& request) const;
    void redistributeLoad();
    
    // Fault tolerance
    void enableFaultTolerance(bool enable);
    bool isFaultToleranceEnabled() const;
    void markNodeUnavailable(const std::string& node_name);
    void markNodeAvailable(const std::string& node_name);
    std::vector<std::string> getUnavailableNodes() const;
    
    // Resource accounting
    struct ResourceAccountingInfo {
        std::string device_name;
        double total_cpu_hours;
        double total_memory_gb_hours;
        double total_bandwidth_gb;
        double total_storage_gb;
        std::chrono::seconds total_runtime;
        double cost_estimate;
        std::chrono::system_clock::time_point first_usage;
        std::chrono::system_clock::time_point last_usage;
    };
    
    ResourceAccountingInfo getResourceAccounting(const std::string& device_name) const;
    std::vector<ResourceAccountingInfo> getAllResourceAccounting() const;
    void resetResourceAccounting(const std::string& device_name = "");
    
    // Quotas and limits
    void setResourceQuota(const std::string& device_name, 
                         ResourceType type, 
                         double quota);
    double getResourceQuota(const std::string& device_name, ResourceType type) const;
    void removeResourceQuota(const std::string& device_name, ResourceType type);
    
    // Callbacks and notifications
    using ResourceEventCallback = std::function<void(const std::string&, const std::string&)>;
    void setResourceAllocatedCallback(ResourceEventCallback callback);
    void setResourceReleasedCallback(ResourceEventCallback callback);
    void setResourceExhaustedCallback(ResourceEventCallback callback);
    
    // Statistics and reporting
    struct SystemResourceStats {
        size_t total_pools{0};
        size_t active_leases{0};
        size_t pending_requests{0};
        size_t completed_requests{0};
        double average_wait_time{0.0};
        double average_utilization{0.0};
        double total_throughput{0.0};
        std::chrono::system_clock::time_point last_update;
    };
    
    SystemResourceStats getSystemStats() const;
    
    std::string generateResourceReport(std::chrono::system_clock::time_point start_time,
                                     std::chrono::system_clock::time_point end_time) const;
    
    void exportResourceUsage(const std::string& output_path, 
                           const std::string& format = "csv") const;
    
    // Configuration
    void setConfiguration(const std::string& config_json);
    std::string getConfiguration() const;
    void saveConfiguration(const std::string& file_path);
    void loadConfiguration(const std::string& file_path);
    
    // Maintenance
    void cleanup();
    void compactHistory();
    void validateResourceIntegrity();
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Internal methods
    void processResourceQueue();
    bool canAllocateResources(const ResourceRequest& request) const;
    void updateResourceUsage();
    void checkResourceConstraints();
    void handleResourceEvents();
    
    // Scheduling algorithms
    std::string selectNextRequest_FCFS() const;
    std::string selectNextRequest_Priority() const;
    std::string selectNextRequest_RoundRobin() const;
    std::string selectNextRequest_ShortestJob() const;
    std::string selectNextRequest_FairShare() const;
    std::string selectNextRequest_Adaptive() const;
};

// Utility functions
namespace resource_utils {
    std::string generateResourceId();
    std::string generateLeaseId();
    std::string generateRequestId();
    
    double calculateResourceEfficiency(const ResourceUsageStats& stats);
    double calculateResourceWaste(const ResourceUsageStats& stats);
    
    std::string formatResourceAmount(double amount, const std::string& unit);
    std::string formatResourceUsage(const ResourceUsageStats& stats);
    
    // Resource conversion utilities
    double convertToStandardUnit(double amount, const std::string& from_unit, const std::string& to_unit);
    
    // Resource estimation
    double estimateResourceNeed(const std::string& device_name, 
                              ResourceType type, 
                              std::chrono::milliseconds duration);
}

} // namespace lithium
