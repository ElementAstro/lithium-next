/*
 * lockfree_container.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Lock-free containers with C++23 optimizations
for high-performance astrophotography control software

**************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <chrono>
#include <span>
#include <concepts>

namespace lithium::container {

/**
 * @brief Lock-free hash map with optimized performance for component management
 */
template<typename Key, typename Value>
requires std::is_trivially_copyable_v<Key> && std::is_move_constructible_v<Value>
class LockFreeHashMap {
private:
    static constexpr std::size_t DEFAULT_CAPACITY = 1024;
    static constexpr std::size_t MAX_LOAD_FACTOR_PERCENT = 75;
    
    struct Node {
        std::atomic<Key> key;
        std::atomic<Value*> value;
        std::atomic<Node*> next;
        std::atomic<bool> deleted;
        
        Node() : key{}, value{nullptr}, next{nullptr}, deleted{false} {}
        Node(Key k, Value* v) : key{k}, value{v}, next{nullptr}, deleted{false} {}
    };
    
    std::unique_ptr<std::atomic<Node*>[]> buckets_;
    std::atomic<std::size_t> size_;
    std::atomic<std::size_t> capacity_;
    std::atomic<bool> resizing_;
    
    // Memory management
    std::atomic<Node*> free_list_;
    alignas(64) std::atomic<std::size_t> allocation_counter_;

public:
    explicit LockFreeHashMap(std::size_t initial_capacity = DEFAULT_CAPACITY)
        : buckets_(std::make_unique<std::atomic<Node*>[]>(initial_capacity))
        , size_(0)
        , capacity_(initial_capacity)
        , resizing_(false)
        , free_list_(nullptr)
        , allocation_counter_(0) {
        
        for (std::size_t i = 0; i < capacity_.load(); ++i) {
            buckets_[i].store(nullptr, std::memory_order_relaxed);
        }
    }
    
    ~LockFreeHashMap() {
        clear();
        
        // Clean up free list
        Node* current = free_list_.load();
        while (current) {
            Node* next = current->next.load();
            delete current;
            current = next;
        }
    }
    
    /**
     * @brief Insert or update a key-value pair
     * @param key The key
     * @param value The value (will be moved)
     * @return True if inserted, false if updated
     */
    bool insert_or_update(const Key& key, Value value) {
        auto hash = std::hash<Key>{}(key);
        auto* value_ptr = new Value(std::move(value));
        
        while (true) {
            auto cap = capacity_.load(std::memory_order_acquire);
            auto bucket_idx = hash % cap;
            auto* bucket = &buckets_[bucket_idx];
            
            // Check if resize is needed
            if (size_.load(std::memory_order_relaxed) > (cap * MAX_LOAD_FACTOR_PERCENT) / 100) {
                try_resize();
                continue; // Retry with new capacity
            }
            
            Node* current = bucket->load(std::memory_order_acquire);
            Node* prev = nullptr;
            
            // Search for existing key
            while (current) {
                if (!current->deleted.load(std::memory_order_acquire)) {
                    auto current_key = current->key.load(std::memory_order_acquire);
                    if (current_key == key) {
                        // Update existing
                        auto* old_value = current->value.exchange(value_ptr, std::memory_order_acq_rel);
                        delete old_value;
                        return false; // Updated
                    }
                }
                prev = current;
                current = current->next.load(std::memory_order_acquire);
            }
            
            // Create new node
            auto* new_node = allocate_node(key, value_ptr);
            
            // Insert at head of bucket
            new_node->next.store(bucket->load(std::memory_order_acquire), std::memory_order_relaxed);
            
            if (bucket->compare_exchange_weak(new_node->next.load(), new_node, 
                                            std::memory_order_release, 
                                            std::memory_order_relaxed)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return true; // Inserted
            }
            
            // CAS failed, retry
            deallocate_node(new_node);
        }
    }
    
    /**
     * @brief Find a value by key
     * @param key The key to search for
     * @return Optional containing the value if found
     */
    std::optional<Value> find(const Key& key) const {
        auto hash = std::hash<Key>{}(key);
        auto cap = capacity_.load(std::memory_order_acquire);
        auto bucket_idx = hash % cap;
        
        Node* current = buckets_[bucket_idx].load(std::memory_order_acquire);
        
        while (current) {
            if (!current->deleted.load(std::memory_order_acquire)) {
                auto current_key = current->key.load(std::memory_order_acquire);
                if (current_key == key) {
                    auto* value_ptr = current->value.load(std::memory_order_acquire);
                    if (value_ptr) {
                        return *value_ptr;
                    }
                }
            }
            current = current->next.load(std::memory_order_acquire);
        }
        
        return std::nullopt;
    }
    
    /**
     * @brief Remove a key-value pair
     * @param key The key to remove
     * @return True if removed, false if not found
     */
    bool erase(const Key& key) {
        auto hash = std::hash<Key>{}(key);
        auto cap = capacity_.load(std::memory_order_acquire);
        auto bucket_idx = hash % cap;
        
        Node* current = buckets_[bucket_idx].load(std::memory_order_acquire);
        
        while (current) {
            if (!current->deleted.load(std::memory_order_acquire)) {
                auto current_key = current->key.load(std::memory_order_acquire);
                if (current_key == key) {
                    // Mark as deleted
                    current->deleted.store(true, std::memory_order_release);
                    
                    // Clean up value
                    auto* value_ptr = current->value.exchange(nullptr, std::memory_order_acq_rel);
                    delete value_ptr;
                    
                    size_.fetch_sub(1, std::memory_order_relaxed);
                    return true;
                }
            }
            current = current->next.load(std::memory_order_acquire);
        }
        
        return false;
    }
    
    /**
     * @brief Get current size
     */
    std::size_t size() const noexcept {
        return size_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if empty
     */
    bool empty() const noexcept {
        return size() == 0;
    }
    
    /**
     * @brief Clear all elements
     */
    void clear() {
        auto cap = capacity_.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < cap; ++i) {
            Node* current = buckets_[i].exchange(nullptr, std::memory_order_acq_rel);
            while (current) {
                Node* next = current->next.load();
                auto* value_ptr = current->value.load();
                delete value_ptr;
                deallocate_node(current);
                current = next;
            }
        }
        size_.store(0, std::memory_order_relaxed);
    }
    
private:
    Node* allocate_node(const Key& key, Value* value) {
        allocation_counter_.fetch_add(1, std::memory_order_relaxed);
        
        // Try to reuse from free list first
        Node* free_node = free_list_.load(std::memory_order_acquire);
        while (free_node) {
            Node* next = free_node->next.load(std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(free_node, next, 
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                // Reuse node
                free_node->key.store(key, std::memory_order_relaxed);
                free_node->value.store(value, std::memory_order_relaxed);
                free_node->next.store(nullptr, std::memory_order_relaxed);
                free_node->deleted.store(false, std::memory_order_relaxed);
                return free_node;
            }
            free_node = free_list_.load(std::memory_order_acquire);
        }
        
        // Allocate new node
        return new Node(key, value);
    }
    
    void deallocate_node(Node* node) {
        if (!node) return;
        
        // Add to free list for reuse
        node->next.store(free_list_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        while (!free_list_.compare_exchange_weak(node->next.load(), node,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
            node->next.store(free_list_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    }
    
    void try_resize() {
        // Only one thread should resize at a time
        bool expected = false;
        if (!resizing_.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
            // Another thread is resizing, wait for it to complete
            while (resizing_.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            return;
        }
        
        auto old_cap = capacity_.load(std::memory_order_relaxed);
        auto new_cap = old_cap * 2;
        
        try {
            auto new_buckets = std::make_unique<std::atomic<Node*>[]>(new_cap);
            for (std::size_t i = 0; i < new_cap; ++i) {
                new_buckets[i].store(nullptr, std::memory_order_relaxed);
            }
            
            // Rehash all existing nodes
            for (std::size_t i = 0; i < old_cap; ++i) {
                Node* current = buckets_[i].exchange(nullptr, std::memory_order_acq_rel);
                while (current) {
                    Node* next = current->next.load();
                    
                    if (!current->deleted.load(std::memory_order_acquire)) {
                        auto key = current->key.load(std::memory_order_acquire);
                        auto hash = std::hash<Key>{}(key);
                        auto new_bucket_idx = hash % new_cap;
                        
                        current->next.store(new_buckets[new_bucket_idx].load(std::memory_order_relaxed),
                                          std::memory_order_relaxed);
                        new_buckets[new_bucket_idx].store(current, std::memory_order_relaxed);
                    } else {
                        deallocate_node(current);
                    }
                    
                    current = next;
                }
            }
            
            buckets_ = std::move(new_buckets);
            capacity_.store(new_cap, std::memory_order_release);
            
        } catch (...) {
            // Resize failed, continue with old capacity
        }
        
        resizing_.store(false, std::memory_order_release);
    }
};

/**
 * @brief Lock-free queue optimized for component event processing
 */
template<typename T>
requires std::is_move_constructible_v<T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
        
        Node() : data(nullptr), next(nullptr) {}
    };
    
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    alignas(64) std::atomic<std::size_t> size_;

public:
    LockFreeQueue() {
        Node* dummy = new Node;
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        size_.store(0, std::memory_order_relaxed);
    }
    
    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);
            delete old_head->data.load();
            delete old_head;
        }
    }
    
    void enqueue(T item) {
        Node* new_node = new Node;
        T* data = new T(std::move(item));
        new_node->data.store(data, std::memory_order_relaxed);
        
        while (true) {
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            
            if (last == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(next, new_node,
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed)) {
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(last, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                }
            }
        }
        
        tail_.compare_exchange_weak(tail_.load(), new_node,
                                  std::memory_order_release,
                                  std::memory_order_relaxed);
        size_.fetch_add(1, std::memory_order_relaxed);
    }
    
    std::optional<T> dequeue() {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);
            
            if (first == head_.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr) {
                        return std::nullopt; // Queue is empty
                    }
                    tail_.compare_exchange_weak(last, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed);
                } else {
                    if (next == nullptr) {
                        continue;
                    }
                    
                    T* data = next->data.load(std::memory_order_acquire);
                    if (head_.compare_exchange_weak(first, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                        if (data) {
                            T result = std::move(*data);
                            delete data;
                            delete first;
                            size_.fetch_sub(1, std::memory_order_relaxed);
                            return result;
                        }
                        delete first;
                    }
                }
            }
        }
    }
    
    std::size_t size() const noexcept {
        return size_.load(std::memory_order_relaxed);
    }
    
    bool empty() const noexcept {
        return size() == 0;
    }
};

}  // namespace lithium::container
