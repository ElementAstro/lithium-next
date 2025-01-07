#include "atom/type/json.hpp"
#include "config/configor.hpp"

#include <iostream>

int main() {
  // Create a shared pointer instance
  auto configManager = lithium::ConfigManager::createShared();

  // Set a configuration value
  nlohmann::json newValue = "new_value";
  if (configManager->set("some/key/path", newValue)) {
    std::cout << "Configuration value set successfully" << std::endl;
  } else {
    std::cout << "Failed to set configuration value" << std::endl;
  }

  // Get a configuration value
  auto value = configManager->get("some/key/path");
  if (value.has_value()) {
    std::cout << "Configuration value: " << value.value().dump() << std::endl;
  } else {
    std::cout << "Configuration value does not exist" << std::endl;
  }

  // Check if a configuration value exists
  if (configManager->has("some/key/path")) {
    std::cout << "Configuration value exists" << std::endl;
  } else {
    std::cout << "Configuration value does not exist" << std::endl;
  }

  // Save configuration to a file
  if (configManager->save("some.json")) {
    std::cout << "Configuration saved successfully" << std::endl;
  } else {
    std::cout << "Failed to save configuration" << std::endl;
  }

  // Load configuration from a file
  if (configManager->loadFromFile("some.json")) {
    std::cout << "Configuration file loaded successfully" << std::endl;
  } else {
    std::cout << "Failed to load configuration file" << std::endl;
  }

  // Remove a configuration value
  if (configManager->remove("some/key/path")) {
    std::cout << "Configuration value removed successfully" << std::endl;
  } else {
    std::cout << "Failed to remove configuration value" << std::endl;
  }

  // Check if a configuration value exists
  if (configManager->has("some/key/path")) {
    std::cout << "Configuration value exists" << std::endl;
  } else {
    std::cout << "Configuration value does not exist" << std::endl;
  }

  // Merge configuration
  nlohmann::json newConfig = {{"new/key/path", "new_value"}};
  configManager->merge(newConfig);

  // Get the merged configuration value
  auto mergedValue = configManager->get("new/key/path");
  if (mergedValue.has_value()) {
    std::cout << "Merged configuration value: " << mergedValue.value().dump()
              << std::endl;
  } else {
    std::cout << "Merged configuration value does not exist" << std::endl;
  }

  // Clear all configurations
  configManager->clear();
  std::cout << "All configurations cleared" << std::endl;

  // Check if a configuration value exists
  if (configManager->has("new/key/path")) {
    std::cout << "Configuration value exists" << std::endl;
  } else {
    std::cout << "Configuration value does not exist" << std::endl;
  }

  return 0;
}