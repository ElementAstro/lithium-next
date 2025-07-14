/*
 * device_cache_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Device Cache System for optimized data and state management

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace lithium {

// Cache entry types
enum class CacheEntryType {
    DEVICE_STATE,
    DEVICE_CONFIG,
    DEVICE_CAPABILITIES,
    DEVICE_PROPERTIES,
    OPERATION_RESULT,
    TELEMETRY_DATA,
    CUSTOM
};

// Cache eviction policies
enum class EvictionPolicy {
    LRU,    // Least Recently Used
    LFU,    // Least Frequently Used
    TTL,    // Time To Live
    FIFO,   // First In, First Out
    RANDOM,
    ADAPTIVE
};

// Cache storage backends
enum class StorageBackend {
    MEMORY,
    DISK,
    HYBRID,
    DISTRIBUTED
};

// Cache entry
template<typename T>
struct CacheEntry {
    std::string key;
    T value;
    CacheEntryType type;
    
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point last_modified;
    std::chrono::system_clock::time_point expires_at;
    
    size_t access_count{0};
    size_t size_bytes{0};
    int priority{0};
    
    bool is_persistent{false};
    bool is_dirty{false};
    bool is_locked{false};
    
    std::string device_name;
    std::string category;
    std::unordered_map<std::string, std::string> metadata;
};

// Cache configuration
struct CacheConfig {
    size_t max_memory_size{100 * 1024 * 1024}; // 100MB
    size_t max_entries{10000};
    size_t max_entry_size{10 * 1024 * 1024}; // 10MB
    
    EvictionPolicy eviction_policy{EvictionPolicy::LRU};
    StorageBackend storage_backend{StorageBackend::MEMORY};
    
    std::chrono::seconds default_ttl{3600}; // 1 hour
    std::chrono::seconds cleanup_interval{300}; // 5 minutes
    std::chrono::seconds sync_interval{60}; // 1 minute
    
    bool enable_compression{true};
    bool enable_encryption{false};
    bool enable_persistence{true};
    bool enable_statistics{true};
    
    std::string cache_directory{"./cache"};
    std::string encryption_key;
    
    double memory_threshold{0.9};
    double disk_threshold{0.9};
    
    // Performance tuning
    size_t initial_hash_table_size{1024};
    double hash_load_factor{0.75};
    size_t async_write_queue_size{1000};
    size_t read_ahead_size{10};
};

// Cache statistics
struct CacheStatistics {
    size_t total_requests{0};
    size_t cache_hits{0};
    size_t cache_misses{0};
    size_t evictions{0};
    size_t expirations{0};
    
    size_t current_entries{0};
    size_t current_memory_usage{0};
    size_t current_disk_usage{0};
    
    double hit_rate{0.0};
    double miss_rate{0.0};
    double eviction_rate{0.0};
    
    std::chrono::milliseconds average_access_time{0};
    std::chrono::milliseconds average_write_time{0};
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_reset;
    
    std::unordered_map<CacheEntryType, size_t> entries_by_type;
    std::unordered_map<std::string, size_t> entries_by_device;
};

// Cache events
enum class CacheEventType {
    ENTRY_ADDED,
    ENTRY_UPDATED,
    ENTRY_REMOVED,
    ENTRY_EXPIRED,
    ENTRY_EVICTED,
    CACHE_FULL,
    CACHE_CLEARED
};

struct CacheEvent {
    CacheEventType type;
    std::string key;
    std::string device_name;
    CacheEntryType entry_type;
    size_t entry_size;
    std::chrono::system_clock::time_point timestamp;
    std::string reason;
};

template<typename T>
class DeviceCacheSystem {
public:
    DeviceCacheSystem();
    explicit DeviceCacheSystem(const CacheConfig& config);
    ~DeviceCacheSystem();
    
    // Configuration
    void setConfiguration(const CacheConfig& config);
    CacheConfig getConfiguration() const;
    
    // Cache lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Basic cache operations
    bool put(const std::string& key, const T& value, 
             CacheEntryType type = CacheEntryType::CUSTOM,
             std::chrono::seconds ttl = std::chrono::seconds{0});
    
    bool get(const std::string& key, T& value);
    std::shared_ptr<CacheEntry<T>> getEntry(const std::string& key);
    
    bool contains(const std::string& key) const;
    bool remove(const std::string& key);
    void clear();
    
    // Advanced operations
    bool putIfAbsent(const std::string& key, const T& value, CacheEntryType type = CacheEntryType::CUSTOM);
    bool replace(const std::string& key, const T& value);
    bool compareAndSwap(const std::string& key, const T& expected, const T& new_value);
    
    // Batch operations
    std::vector<std::pair<std::string, T>> getMultiple(const std::vector<std::string>& keys);
    void putMultiple(const std::vector<std::tuple<std::string, T, CacheEntryType>>& entries);
    void removeMultiple(const std::vector<std::string>& keys);
    
    // Device-specific operations
    bool putDeviceState(const std::string& device_name, const T& state);
    bool getDeviceState(const std::string& device_name, T& state);
    void clearDeviceCache(const std::string& device_name);
    
    bool putDeviceConfig(const std::string& device_name, const T& config);
    bool getDeviceConfig(const std::string& device_name, T& config);
    
    bool putDeviceCapabilities(const std::string& device_name, const T& capabilities);
    bool getDeviceCapabilities(const std::string& device_name, T& capabilities);
    
    // Query operations
    std::vector<std::string> getKeys() const;
    std::vector<std::string> getKeysForDevice(const std::string& device_name) const;
    std::vector<std::string> getKeysByType(CacheEntryType type) const;
    std::vector<std::string> getKeysByPattern(const std::string& pattern) const;
    
    size_t size() const;
    size_t sizeForDevice(const std::string& device_name) const;
    size_t memoryUsage() const;
    size_t diskUsage() const;
    
    // Cache management
    void setTTL(const std::string& key, std::chrono::seconds ttl);
    std::chrono::seconds getTTL(const std::string& key) const;
    void refresh(const std::string& key);
    
    void lock(const std::string& key);
    void unlock(const std::string& key);
    bool isLocked(const std::string& key) const;
    
    // Eviction and cleanup
    void evictLRU();
    void evictLFU();
    void evictExpired();
    void evictBySize(size_t target_size);
    
    void runCleanup();
    void scheduleCleanup();
    
    // Persistence
    bool saveToFile(const std::string& file_path);
    bool loadFromFile(const std::string& file_path);
    void enableAutoPersistence(bool enable);
    bool isAutoPersistenceEnabled() const;
    
    // Compression and encryption
    void enableCompression(bool enable);
    bool isCompressionEnabled() const;
    
    void enableEncryption(bool enable, const std::string& key = "");
    bool isEncryptionEnabled() const;
    
    // Statistics and monitoring
    CacheStatistics getStatistics() const;
    void resetStatistics();
    
    std::vector<CacheEntry<T>> getTopAccessedEntries(size_t count = 10) const;
    std::vector<CacheEntry<T>> getLargestEntries(size_t count = 10) const;
    std::vector<CacheEntry<T>> getOldestEntries(size_t count = 10) const;
    
    // Event handling
    using CacheEventCallback = std::function<void(const CacheEvent&)>;
    void setCacheEventCallback(CacheEventCallback callback);
    
    // Performance optimization
    void enablePreloading(bool enable);
    bool isPreloadingEnabled() const;
    void preloadDevice(const std::string& device_name);
    
    void enableReadAhead(bool enable);
    bool isReadAheadEnabled() const;
    
    void enableWriteBehind(bool enable);
    bool isWriteBehindEnabled() const;
    
    // Cache warming
    void warmupCache(const std::vector<std::string>& keys);
    void scheduleWarmup(const std::vector<std::string>& keys, 
                       std::chrono::system_clock::time_point when);
    
    // Cache invalidation
    void invalidate(const std::string& key);
    void invalidateDevice(const std::string& device_name);
    void invalidateType(CacheEntryType type);
    void invalidatePattern(const std::string& pattern);
    
    // Cache coherence (for distributed caches)
    void enableCoherence(bool enable);
    bool isCoherenceEnabled() const;
    void notifyUpdate(const std::string& key);
    
    // Advanced features
    
    // Cache partitioning
    void createPartition(const std::string& partition_name, const CacheConfig& config);
    void removePartition(const std::string& partition_name);
    std::vector<std::string> getPartitions() const;
    
    // Cache mirroring
    void enableMirroring(bool enable);
    bool isMirroringEnabled() const;
    void addMirror(const std::string& mirror_name);
    void removeMirror(const std::string& mirror_name);
    
    // Cache replication
    void enableReplication(bool enable);
    bool isReplicationEnabled() const;
    void setReplicationFactor(size_t factor);
    
    // Debugging and diagnostics
    std::string getCacheStatus() const;
    std::string getEntryInfo(const std::string& key) const;
    void dumpCacheState(const std::string& output_path) const;
    
    // Maintenance
    void runMaintenance();
    void compactCache();
    void validateCacheIntegrity();
    void repairCache();
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Internal methods
    void backgroundMaintenance();
    void processWriteQueue();
    void updateStatistics();
    
    // Eviction algorithms
    std::string selectLRUCandidate() const;
    std::string selectLFUCandidate() const;
    std::string selectRandomCandidate() const;
    
    // Compression and encryption
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data) const;
    
    // Serialization
    std::vector<uint8_t> serialize(const T& value) const;
    T deserialize(const std::vector<uint8_t>& data) const;
    
    // Hash functions
    size_t hashKey(const std::string& key) const;
    std::string generateCacheKey(const std::string& device_name, 
                                const std::string& property_name,
                                CacheEntryType type) const;
};

// Utility functions
namespace cache_utils {
    std::string formatCacheStatistics(const CacheStatistics& stats);
    std::string formatCacheEvent(const CacheEvent& event);
    
    double calculateHitRate(const CacheStatistics& stats);
    double calculateEvictionRate(const CacheStatistics& stats);
    
    // Cache size estimation
    size_t estimateEntrySize(const std::string& key, const void* value, size_t value_size);
    size_t estimateMemoryOverhead(size_t entry_count);
    
    // Cache key utilities
    std::string createDeviceStateKey(const std::string& device_name);
    std::string createDeviceConfigKey(const std::string& device_name);
    std::string createDeviceCapabilityKey(const std::string& device_name);
    std::string createOperationResultKey(const std::string& device_name, const std::string& operation);
    
    // Pattern matching
    bool matchesPattern(const std::string& key, const std::string& pattern);
    std::vector<std::string> expandPattern(const std::string& pattern);
    
    // Cache optimization
    size_t calculateOptimalCacheSize(size_t data_size, double hit_rate_target);
    std::chrono::seconds calculateOptimalTTL(double access_frequency, double data_volatility);
}

} // namespace lithium
