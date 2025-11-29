#include "driverlist.hpp"

#include <tinyxml2.h>
#include <filesystem>

#include "atom/async/pool.hpp"
#include "atom/log/spdlog_logger.hpp"

using namespace tinyxml2;
using namespace std::filesystem;
using namespace atom::async;

auto loadXMLFile(const std::string& filename, XMLDocument& doc) -> bool {
    LOG_INFO("Loading XML file: {}", filename);
    if (doc.LoadFile(filename.c_str()) != XML_SUCCESS) {
        LOG_ERROR("Unable to load XML file: {}", filename);
        return false;
    }
    LOG_INFO("Successfully loaded XML file: {}", filename);
    return true;
}

auto parseDriversList(const std::string& filename) -> std::vector<DevGroup> {
    LOG_INFO("Parsing drivers list from file: {}", filename);
    std::vector<DevGroup> devGroups;
    XMLDocument doc;

    if (!loadXMLFile(filename, doc)) {
        return devGroups;
    }

    XMLElement* root = doc.RootElement();
    for (XMLElement* devGroupElem = root->FirstChildElement("devGroup");
         devGroupElem != nullptr;
         devGroupElem = devGroupElem->NextSiblingElement("devGroup")) {
        DevGroup devGroup;
        devGroup.group = devGroupElem->Attribute("group");
        LOG_INFO("Found devGroup: {}", devGroup.group);
        devGroups.push_back(devGroup);
    }

    LOG_INFO("Completed parsing drivers list from file: {}", filename);
    return devGroups;
}

auto parseDevicesFromFile(const std::string& filepath,
                          std::vector<Device>& devicesFrom)
    -> std::vector<DevGroup> {
    LOG_INFO("Processing XML file: {}", filepath);
    std::vector<DevGroup> devGroups;
    XMLDocument doc;

    if (!loadXMLFile(filepath, doc)) {
        LOG_ERROR("Unable to load XML file: {}", filepath);
        return devGroups;
    }

    XMLElement* root = doc.RootElement();
    if (!root) {
        LOG_ERROR("No root element in XML file: {}", filepath);
        return devGroups;
    }

    for (XMLElement* devGroupElem = root->FirstChildElement("devGroup");
         devGroupElem != nullptr;
         devGroupElem = devGroupElem->NextSiblingElement("devGroup")) {
        DevGroup devGroup;
        const char* groupAttr = devGroupElem->Attribute("group");
        if (groupAttr) {
            devGroup.group = groupAttr;
            LOG_INFO("Found devGroup: {}", devGroup.group);
        } else {
            LOG_WARN("devGroup element missing 'group' attribute");
            continue;
        }

        for (XMLElement* deviceElem = devGroupElem->FirstChildElement("device");
             deviceElem != nullptr;
             deviceElem = deviceElem->NextSiblingElement("device")) {
            Device device;
            const char* labelAttr = deviceElem->Attribute("label");
            if (labelAttr) {
                device.label = labelAttr;
                LOG_INFO("Found device: {}", device.label);
            } else {
                LOG_WARN("device element missing 'label' attribute");
                continue;
            }

            if (deviceElem->FindAttribute("manufacturer") != nullptr) {
                device.manufacturer = deviceElem->Attribute("manufacturer");
                LOG_INFO("Device manufacturer: {}", device.manufacturer);
            }

            for (XMLElement* driverElem = deviceElem->FirstChildElement();
                 driverElem != nullptr;
                 driverElem = driverElem->NextSiblingElement()) {
                if (std::string(driverElem->Name()) == "driver") {
                    const char* driverText = driverElem->GetText();
                    if (driverText) {
                        device.driverName = driverText;
                        LOG_INFO("Device driver: {}", device.driverName);
                    } else {
                        LOG_WARN("driver element is empty");
                    }
                } else if (std::string(driverElem->Name()) == "version") {
                    const char* versionText = driverElem->GetText();
                    if (versionText) {
                        device.version = versionText;
                        LOG_INFO("Device version: {}", device.version);
                    } else {
                        LOG_WARN("version element is empty");
                    }
                }
            }
            devGroup.devices.push_back(device);
            devicesFrom.push_back(device);
        }
        devGroups.push_back(devGroup);
    }

    return devGroups;
}

auto parseDevicesFromPath(const std::string& path,
                          std::vector<Device>& devicesFrom)
    -> std::vector<DevGroup> {
    LOG_INFO("Parsing devices from path: {}", path);
    std::vector<DevGroup> devGroups;
    ThreadPool pool;

    std::vector<std::future<std::vector<DevGroup>>> futures;

    for (const auto& entry : directory_iterator(path)) {
        if (entry.path().extension() == ".xml" &&
            entry.path().filename().string().substr(
                entry.path().filename().string().size() - 6) != "sk.xml") {
            futures.push_back(pool.enqueue([entry, &devicesFrom]() {
                return parseDevicesFromFile(entry.path().string(), devicesFrom);
            }));
        }
    }

    for (auto& future : futures) {
        auto result = future.get();
        devGroups.insert(devGroups.end(), result.begin(), result.end());
    }

    LOG_INFO("Completed parsing devices from path: {}", path);
    return devGroups;
}

auto mergeDeviceGroups(const DriversList& driversListFrom,
                       const std::vector<DevGroup>& devGroupsFromPath)
    -> DriversList {
    LOG_INFO("Merging device groups");
    DriversList mergedList = driversListFrom;

    for (const auto& devGroupXml : devGroupsFromPath) {
        auto it = std::find_if(
            mergedList.devGroups.begin(), mergedList.devGroups.end(),
            [&devGroupXml](const DevGroup& devGroupFrom) {
                return devGroupXml.group == devGroupFrom.group;
            });
        if (it != mergedList.devGroups.end()) {
            LOG_INFO("Merging devices into group: {}", devGroupXml.group);
            it->devices.insert(it->devices.end(), devGroupXml.devices.begin(),
                               devGroupXml.devices.end());
        } else {
            mergedList.devGroups.push_back(devGroupXml);
        }
    }

    LOG_INFO("Completed merging device groups");
    return mergedList;
}

auto readDriversListFromFiles(std::string_view filename, std::string_view path)
    -> std::tuple<DriversList, std::vector<DevGroup>, std::vector<Device>> {
    LOG_INFO("Reading drivers list from files: {} and path: {}", filename,
             path);
    DriversList driversListFrom;
    std::vector<DevGroup> devGroupsFrom;
    std::vector<Device> devicesFrom;

    if (!exists(path)) {
        LOG_ERROR("Folder not found: {}", path);
        return {driversListFrom, devGroupsFrom, devicesFrom};
    }

    driversListFrom.devGroups = parseDriversList(filename.data());
    devGroupsFrom = parseDevicesFromPath(path.data(), devicesFrom);
    driversListFrom = mergeDeviceGroups(driversListFrom, devGroupsFrom);

    LOG_INFO("Completed reading drivers list from files");
    return {driversListFrom, devGroupsFrom, devicesFrom};
}
