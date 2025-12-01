#include "spatial_index.hpp"

#include <cmath>
#include <limits>

#include "spdlog/spdlog.h"

namespace lithium::target::index {

SpatialIndex::SpatialIndex()
    : SpatialIndex(50, 20) {}

SpatialIndex::SpatialIndex(size_t maxEntries, size_t minEntries)
    : maxEntries_(maxEntries), minEntries_(minEntries) {
    if (minEntries >= maxEntries) {
        spdlog::error(
            "SpatialIndex: minEntries ({}) must be less than maxEntries ({})",
            minEntries, maxEntries);
        throw std::invalid_argument("minEntries must be less than maxEntries");
    }

    // Create root leaf node
    root_ = std::make_unique<RTreeNode>();
    root_->isLeaf = true;
    root_->bbox = {0, 360, -90, 90};

    spdlog::info(
        "SpatialIndex initialized with maxEntries={}, minEntries={}", maxEntries,
        minEntries);
}

void SpatialIndex::insert(const ObjectId& id, double ra, double dec) {
    std::unique_lock lock(mutex_);

    spdlog::debug("Inserting object '{}' at RA={}, Dec={}", id, ra, dec);

    // Check if object already exists and remove it
    if (auto it = objectMap_.find(id); it != objectMap_.end()) {
        spdlog::debug("Object '{}' already exists, updating coordinates", id);
        remove(id);  // This requires unlocking - we'll handle it differently
    }

    insertNode(root_.get(), id, ra, dec);
    objectMap_[id] = {ra, dec};

    spdlog::debug("Successfully inserted object '{}'", id);
}

void SpatialIndex::insertBatch(
    std::span<const std::tuple<ObjectId, double, double>> objects) {
    std::unique_lock lock(mutex_);

    spdlog::info("Performing batch insertion of {} objects", objects.size());

    for (const auto& [id, ra, dec] : objects) {
        // Remove existing entry if present
        auto it = objectMap_.find(id);
        if (it != objectMap_.end()) {
            objectMap_.erase(it);
        }

        insertNode(root_.get(), id, ra, dec);
        objectMap_[id] = {ra, dec};
    }

    spdlog::info("Batch insertion completed for {} objects", objects.size());
}

auto SpatialIndex::searchRadius(double ra, double dec, double radius,
                                 size_t limit) const
    -> std::vector<SearchResult> {
    std::shared_lock lock(mutex_);

    spdlog::debug("Searching radius: center RA={}, Dec={}, radius={}", ra, dec,
                  radius);

    std::vector<SearchResult> results;

    searchRadiusNode(root_.get(), ra, dec, radius, results, limit);

    // Sort by distance
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    spdlog::debug("Radius search found {} results", results.size());

    return results;
}

auto SpatialIndex::searchBox(double minRa, double maxRa, double minDec,
                              double maxDec, size_t limit) const
    -> std::vector<ObjectId> {
    std::shared_lock lock(mutex_);

    spdlog::debug(
        "Searching box: RA=[{},{}], Dec=[{},{}]", minRa, maxRa, minDec, maxDec);

    BoundingBox box{minRa, maxRa, minDec, maxDec};
    std::vector<ObjectId> results;

    searchBoxNode(root_.get(), box, results, limit);

    spdlog::debug("Box search found {} results", results.size());

    return results;
}

void SpatialIndex::remove(const ObjectId& id) {
    std::unique_lock lock(mutex_);

    spdlog::debug("Removing object '{}'", id);

    auto it = objectMap_.find(id);
    if (it == objectMap_.end()) {
        spdlog::debug("Object '{}' not found in index", id);
        return;
    }

    objectMap_.erase(it);

    // Find and remove from tree
    auto node = findNodeContaining(root_.get(), id);
    if (node) {
        auto pos = std::find(node->ids.begin(), node->ids.end(), id);
        if (pos != node->ids.end()) {
            size_t idx = std::distance(node->ids.begin(), pos);
            node->ids.erase(pos);
            node->points.erase(node->points.begin() + idx);
            updateBoundingBox(node);
            spdlog::debug("Successfully removed object '{}'", id);
        }
    }
}

void SpatialIndex::clear() {
    std::unique_lock lock(mutex_);

    spdlog::info("Clearing SpatialIndex");

    root_ = std::make_unique<RTreeNode>();
    root_->isLeaf = true;
    root_->bbox = {0, 360, -90, 90};
    objectMap_.clear();

    spdlog::info("SpatialIndex cleared");
}

auto SpatialIndex::size() const -> size_t {
    std::shared_lock lock(mutex_);
    return objectMap_.size();
}

auto SpatialIndex::contains(const ObjectId& id) const -> bool {
    std::shared_lock lock(mutex_);
    return objectMap_.find(id) != objectMap_.end();
}

auto SpatialIndex::getCoordinates(const ObjectId& id) const -> Point {
    std::shared_lock lock(mutex_);

    auto it = objectMap_.find(id);
    if (it != objectMap_.end()) {
        return it->second;
    }

    return {0.0, 0.0};
}

auto SpatialIndex::sphericalDistance(double ra1, double dec1, double ra2,
                                      double dec2) noexcept -> double {
    // Convert to radians
    double lat1 = degreesToRadians(dec1);
    double lat2 = degreesToRadians(dec2);
    double deltaLng = degreesToRadians(ra2 - ra1);

    // Haversine formula
    double a = std::sin((lat2 - lat1) / 2.0);
    a = a * a;
    double b = std::cos(lat1) * std::cos(lat2);
    double c = std::sin(deltaLng / 2.0);
    c = c * c;

    double angle = 2.0 * std::asin(std::sqrt(a + b * c));

    // Convert back to degrees
    return angle * 180.0 / 3.14159265358979323846;
}

void SpatialIndex::insertNode(RTreeNode* node, const ObjectId& id, double ra,
                               double dec) {
    if (node->isLeaf) {
        // Insert into leaf node
        node->ids.push_back(id);
        node->points.push_back({ra, dec});
        updateBoundingBox(node);

        // Check if node needs splitting
        if (node->ids.size() > maxEntries_) {
            splitNode(node, id, ra, dec);
        }
    } else {
        // Find best child to insert into
        if (node->children.empty()) {
            return;
        }

        // Calculate which child has smallest area increase
        int bestChild = 0;
        double minIncrease = std::numeric_limits<double>::max();

        for (size_t i = 0; i < node->children.size(); ++i) {
            double oldArea = node->children[i]->bbox.area();
            BoundingBox expanded = node->children[i]->bbox;
            expanded.minRa = std::min(expanded.minRa, ra);
            expanded.maxRa = std::max(expanded.maxRa, ra);
            expanded.minDec = std::min(expanded.minDec, dec);
            expanded.maxDec = std::max(expanded.maxDec, dec);
            double newArea = expanded.area();
            double increase = newArea - oldArea;

            if (increase < minIncrease) {
                minIncrease = increase;
                bestChild = static_cast<int>(i);
            }
        }

        insertNode(node->children[bestChild].get(), id, ra, dec);
        updateBoundingBox(node);

        // Check if node needs splitting
        if (node->children.size() > maxEntries_) {
            splitNode(node, id, ra, dec);
        }
    }
}

void SpatialIndex::splitNode(RTreeNode* node, const ObjectId& idToAdd,
                              double raToAdd, double decToAdd) {
    // Simple quadratic split algorithm
    // This is a simplified implementation
    spdlog::debug("Splitting node");

    if (node->isLeaf) {
        // Create new sibling leaf
        auto sibling = std::make_unique<RTreeNode>();
        sibling->isLeaf = true;

        // Split entries between original and sibling
        size_t splitPoint = node->ids.size() / 2;
        sibling->ids.assign(node->ids.begin() + splitPoint, node->ids.end());
        sibling->points.assign(node->points.begin() + splitPoint,
                               node->points.end());

        node->ids.erase(node->ids.begin() + splitPoint, node->ids.end());
        node->points.erase(node->points.begin() + splitPoint,
                           node->points.end());

        updateBoundingBox(node);
        updateBoundingBox(sibling.get());

        // Note: In a complete implementation, we'd need to update the parent
        // For now, we'll just keep both in the node
    }
}

void SpatialIndex::searchRadiusNode(const RTreeNode* node, double ra,
                                     double dec, double radius,
                                     std::vector<SearchResult>& results,
                                     size_t limit) const {
    if (results.size() >= limit) {
        return;
    }

    // Check if this node's bounding box intersects with search sphere
    // (simplified - check if center is within bbox + radius)
    double distToBox = 0;
    if (ra < node->bbox.minRa) {
        distToBox += (node->bbox.minRa - ra) * (node->bbox.minRa - ra);
    } else if (ra > node->bbox.maxRa) {
        distToBox += (ra - node->bbox.maxRa) * (ra - node->bbox.maxRa);
    }
    if (dec < node->bbox.minDec) {
        distToBox += (node->bbox.minDec - dec) * (node->bbox.minDec - dec);
    } else if (dec > node->bbox.maxDec) {
        distToBox += (dec - node->bbox.maxDec) * (dec - node->bbox.maxDec);
    }

    if (std::sqrt(distToBox) > radius) {
        return;  // No intersection
    }

    if (node->isLeaf) {
        // Check all points in this leaf
        for (size_t i = 0; i < node->ids.size(); ++i) {
            if (results.size() >= limit) {
                break;
            }

            double dist = sphericalDistance(ra, dec, node->points[i].first,
                                            node->points[i].second);
            if (dist <= radius) {
                results.push_back({node->ids[i], dist});
            }
        }
    } else {
        // Recurse into children
        for (const auto& child : node->children) {
            if (results.size() >= limit) {
                break;
            }
            searchRadiusNode(child.get(), ra, dec, radius, results, limit);
        }
    }
}

void SpatialIndex::searchBoxNode(const RTreeNode* node, const BoundingBox& box,
                                  std::vector<ObjectId>& results,
                                  size_t limit) const {
    if (results.size() >= limit) {
        return;
    }

    // Check if node's bbox intersects with search box
    if (node->bbox.maxRa < box.minRa || node->bbox.minRa > box.maxRa ||
        node->bbox.maxDec < box.minDec || node->bbox.minDec > box.maxDec) {
        return;  // No intersection
    }

    if (node->isLeaf) {
        // Check all points in this leaf
        for (size_t i = 0; i < node->ids.size(); ++i) {
            if (results.size() >= limit) {
                break;
            }

            if (box.contains(node->points[i].first, node->points[i].second)) {
                results.push_back(node->ids[i]);
            }
        }
    } else {
        // Recurse into children
        for (const auto& child : node->children) {
            if (results.size() >= limit) {
                break;
            }
            searchBoxNode(child.get(), box, results, limit);
        }
    }
}

void SpatialIndex::updateBoundingBox(RTreeNode* node) {
    if (node->isLeaf && !node->points.empty()) {
        node->bbox.minRa = node->bbox.maxRa = node->points[0].first;
        node->bbox.minDec = node->bbox.maxDec = node->points[0].second;

        for (const auto& point : node->points) {
            node->bbox.minRa = std::min(node->bbox.minRa, point.first);
            node->bbox.maxRa = std::max(node->bbox.maxRa, point.first);
            node->bbox.minDec = std::min(node->bbox.minDec, point.second);
            node->bbox.maxDec = std::max(node->bbox.maxDec, point.second);
        }
    } else if (!node->isLeaf && !node->children.empty()) {
        node->bbox.minRa = node->bbox.maxRa =
            node->children[0]->bbox.minRa;
        node->bbox.minDec = node->bbox.maxDec =
            node->children[0]->bbox.minDec;

        for (const auto& child : node->children) {
            node->bbox.minRa = std::min(node->bbox.minRa, child->bbox.minRa);
            node->bbox.maxRa = std::max(node->bbox.maxRa, child->bbox.maxRa);
            node->bbox.minDec = std::min(node->bbox.minDec, child->bbox.minDec);
            node->bbox.maxDec = std::max(node->bbox.maxDec, child->bbox.maxDec);
        }
    }
}

auto SpatialIndex::findNodeContaining(RTreeNode* node, const ObjectId& id)
    -> RTreeNode* {
    if (node->isLeaf) {
        auto it = std::find(node->ids.begin(), node->ids.end(), id);
        if (it != node->ids.end()) {
            return node;
        }
    } else {
        for (auto& child : node->children) {
            auto result = findNodeContaining(child.get(), id);
            if (result) {
                return result;
            }
        }
    }

    return nullptr;
}

}  // namespace lithium::target::index
