/*
 * configuration.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FilterWheel configuration management implementation

*************************************************/

#include "configuration.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

INDIFilterwheelConfiguration::INDIFilterwheelConfiguration(std::string name) 
    : INDIFilterwheelBase(name) {
    
    // Set up configuration directory
    configBasePath_ = std::filesystem::current_path() / "config" / "filterwheel";
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(configBasePath_);
    } catch (const std::exception& e) {
        logger_->error("Failed to create configuration directory: {}", e.what());
    }
}

auto INDIFilterwheelConfiguration::saveFilterConfiguration(const std::string& name) -> bool {
    try {
        logger_->info("Saving filter configuration: {}", name);
        
        auto config = serializeCurrentConfiguration();
        auto filepath = getConfigurationFile(name);
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            logger_->error("Failed to open configuration file for writing: {}", filepath.string());
            return false;
        }
        
        file << config;
        file.close();
        
        logger_->info("Configuration '{}' saved successfully", name);
        return true;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to save configuration '{}': {}", name, e.what());
        return false;
    }
}

auto INDIFilterwheelConfiguration::loadFilterConfiguration(const std::string& name) -> bool {
    try {
        logger_->info("Loading filter configuration: {}", name);
        
        auto filepath = getConfigurationFile(name);
        if (!std::filesystem::exists(filepath)) {
            logger_->error("Configuration file does not exist: {}", filepath.string());
            return false;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            logger_->error("Failed to open configuration file for reading: {}", filepath.string());
            return false;
        }
        
        std::string configStr((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
        file.close();
        
        bool success = deserializeConfiguration(configStr);
        if (success) {
            logger_->info("Configuration '{}' loaded successfully", name);
        } else {
            logger_->error("Failed to apply configuration '{}'", name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to load configuration '{}': {}", name, e.what());
        return false;
    }
}

auto INDIFilterwheelConfiguration::deleteFilterConfiguration(const std::string& name) -> bool {
    try {
        logger_->info("Deleting filter configuration: {}", name);
        
        auto filepath = getConfigurationFile(name);
        if (!std::filesystem::exists(filepath)) {
            logger_->warn("Configuration file does not exist: {}", filepath.string());
            return true;
        }
        
        std::filesystem::remove(filepath);
        logger_->info("Configuration '{}' deleted successfully", name);
        return true;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to delete configuration '{}': {}", name, e.what());
        return false;
    }
}

auto INDIFilterwheelConfiguration::getAvailableConfigurations() -> std::vector<std::string> {
    std::vector<std::string> configurations;
    
    try {
        if (!std::filesystem::exists(configBasePath_)) {
            logger_->debug("Configuration directory does not exist: {}", configBasePath_.string());
            return configurations;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(configBasePath_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cfg") {
                std::string configName = entry.path().stem().string();
                configurations.push_back(configName);
            }
        }
        
        logger_->debug("Found {} configurations", configurations.size());
        
    } catch (const std::exception& e) {
        logger_->error("Failed to scan configuration directory: {}", e.what());
    }
    
    return configurations;
}

auto INDIFilterwheelConfiguration::exportConfiguration(const std::string& filename) -> bool {
    try {
        logger_->info("Exporting configuration to: {}", filename);
        
        auto config = serializeCurrentConfiguration();
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            logger_->error("Failed to open export file for writing: {}", filename);
            return false;
        }
        
        file << config;
        file.close();
        
        logger_->info("Configuration exported successfully to: {}", filename);
        return true;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to export configuration: {}", e.what());
        return false;
    }
}

auto INDIFilterwheelConfiguration::importConfiguration(const std::string& filename) -> bool {
    try {
        logger_->info("Importing configuration from: {}", filename);
        
        if (!std::filesystem::exists(filename)) {
            logger_->error("Import file does not exist: {}", filename);
            return false;
        }
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            logger_->error("Failed to open import file for reading: {}", filename);
            return false;
        }
        
        std::string configStr((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
        file.close();
        
        bool success = deserializeConfiguration(configStr);
        if (success) {
            logger_->info("Configuration imported successfully from: {}", filename);
        } else {
            logger_->error("Failed to apply imported configuration");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to import configuration: {}", e.what());
        return false;
    }
}

auto INDIFilterwheelConfiguration::getConfigurationDetails(const std::string& name) -> std::optional<std::string> {
    try {
        auto filepath = getConfigurationFile(name);
        if (!std::filesystem::exists(filepath)) {
            logger_->debug("Configuration file does not exist: {}", filepath.string());
            return std::nullopt;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            logger_->error("Failed to open configuration file: {}", filepath.string());
            return std::nullopt;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        return content;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to read configuration details: {}", e.what());
        return std::nullopt;
    }
}

std::filesystem::path INDIFilterwheelConfiguration::getConfigurationPath() const {
    return configBasePath_;
}

std::filesystem::path INDIFilterwheelConfiguration::getConfigurationFile(const std::string& name) const {
    return configBasePath_ / (name + ".cfg");
}

auto INDIFilterwheelConfiguration::serializeCurrentConfiguration() -> std::string {
    std::ostringstream config;
    
    // Basic device info
    config << "# FilterWheel Configuration\n";
    config << "device_name=" << deviceName_ << "\n";
    config << "driver_version=" << driverVersion_ << "\n";
    config << "driver_interface=" << driverInterface_ << "\n";
    config << "\n";
    
    // Filter configuration
    config << "# Filter Configuration\n";
    config << "filter_count=" << slotNames_.size() << "\n";
    config << "max_slot=" << maxSlot_ << "\n";
    config << "min_slot=" << minSlot_ << "\n";
    config << "current_slot=" << currentSlot_.load() << "\n";
    config << "\n";
    
    // Slot names
    config << "# Slot Names\n";
    for (size_t i = 0; i < slotNames_.size(); ++i) {
        config << "slot_" << i << "=" << slotNames_[i] << "\n";
    }
    config << "\n";
    
    // Filter information
    config << "# Filter Information\n";
    for (int i = 0; i < MAX_FILTERS && i < static_cast<int>(slotNames_.size()); ++i) {
        config << "filter_" << i << "_name=" << filters_[i].name << "\n";
        config << "filter_" << i << "_type=" << filters_[i].type << "\n";
        config << "filter_" << i << "_wavelength=" << filters_[i].wavelength << "\n";
        config << "filter_" << i << "_bandwidth=" << filters_[i].bandwidth << "\n";
        config << "filter_" << i << "_description=" << filters_[i].description << "\n";
    }
    config << "\n";
    
    // Statistics
    config << "# Statistics\n";
    config << "total_moves=" << total_moves_ << "\n";
    config << "last_move_time=" << last_move_time_ << "\n";
    config << "\n";
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    config << "# Saved at: " << std::ctime(&time_t);
    
    return config.str();
}

auto INDIFilterwheelConfiguration::deserializeConfiguration(const std::string& configStr) -> bool {
    try {
        std::istringstream stream(configStr);
        std::string line;
        
        // Clear current state
        slotNames_.clear();
        
        while (std::getline(stream, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Parse key=value pairs
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Process different configuration values
            if (key == "max_slot") {
                maxSlot_ = std::stoi(value);
            } else if (key == "min_slot") {
                minSlot_ = std::stoi(value);
            } else if (key == "filter_count") {
                int count = std::stoi(value);
                slotNames_.resize(count);
            } else if (key.find("slot_") == 0) {
                // Extract slot index
                size_t underscorePos = key.find('_');
                if (underscorePos != std::string::npos) {
                    int slot = std::stoi(key.substr(underscorePos + 1));
                    if (slot >= 0 && slot < static_cast<int>(slotNames_.size())) {
                        slotNames_[slot] = value;
                    }
                }
            } else if (key.find("filter_") == 0) {
                // Parse filter information
                size_t firstUnderscore = key.find('_');
                size_t secondUnderscore = key.find('_', firstUnderscore + 1);
                if (firstUnderscore != std::string::npos && secondUnderscore != std::string::npos) {
                    int slot = std::stoi(key.substr(firstUnderscore + 1, secondUnderscore - firstUnderscore - 1));
                    std::string property = key.substr(secondUnderscore + 1);
                    
                    if (slot >= 0 && slot < MAX_FILTERS) {
                        if (property == "name") {
                            filters_[slot].name = value;
                        } else if (property == "type") {
                            filters_[slot].type = value;
                        } else if (property == "wavelength") {
                            filters_[slot].wavelength = std::stod(value);
                        } else if (property == "bandwidth") {
                            filters_[slot].bandwidth = std::stod(value);
                        } else if (property == "description") {
                            filters_[slot].description = value;
                        }
                    }
                }
            }
        }
        
        logger_->info("Configuration loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_->error("Failed to deserialize configuration: {}", e.what());
        return false;
    }
}
