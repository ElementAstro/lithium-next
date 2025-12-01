#ifndef LITHIUM_TARGET_INDEX_SPATIAL_INDEX_HPP
#define LITHIUM_TARGET_INDEX_SPATIAL_INDEX_HPP

#include <algorithm>
#include <cmath>
#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace lithium::target::index {

/**
 * @brief Spatial index for celestial coordinates using R-tree algorithm
 *
 * A spatial indexing structure optimized for:
 * - 2D point queries (Right Ascension and Declination coordinates)
 * - Radius-based searches (spherical distance on celestial sphere)
 * - Rectangular bounding box searches
 * - Thread-safe concurrent operations
 * - Support for generic ID types via templates
 *
 * The implementation uses a custom R-tree with:
 * - Configurable node capacity (default: max 50, min 20 entries)
 * - Quadratic split algorithm for node overflow
 * - Haversine formula for spherical distance calculation
 * - Efficient distance-based result ranking
 *
 * Coordinate system:
 * - RA (Right Ascension): 0-360 degrees or 0-24 hours
 * - Dec (Declination): -90 to +90 degrees
 *
 * @thread_safe All public methods are fully thread-safe
 *
 * @template ObjectId Type for object identifiers (typically std::string)
 *
 * @example
 * ```cpp
 * SpatialIndex index;
 * index.insert("Orion M42", 85.375, -2.27);
 * index.insert("Andromeda M31", 10.685, 41.27);
 *
 * // Find objects within 5 degrees of Orion
 * auto results = index.searchRadius(85.375, -2.27, 5.0, 100);
 * for (const auto& [id, distance] : results) {
 *     std::cout << id << ": " << distance << " degrees away\n";
 * }
 * ```
 */
class SpatialIndex {
public:
    // Type definitions
    using ObjectId = std::string;
    using Point = std::pair<double, double>;  ///< (RA, Dec) in degrees
    using SearchResult = std::pair<ObjectId, double>;  ///< (ID, distance)

    /**
     * @brief Create a new spatial index with default configuration
     *
     * Initializes the index with default parameters:
     * - Max entries per node: 50
     * - Min entries per node: 20
     */
    SpatialIndex();

    /**
     * @brief Create a spatial index with custom node capacity
     *
     * @param maxEntries Maximum number of entries per node (typical: 50)
     * @param minEntries Minimum number of entries per node (typical: 20)
     * @throws std::invalid_argument if minEntries >= maxEntries
     */
    explicit SpatialIndex(size_t maxEntries, size_t minEntries = 20);

    // Deleted copy operations
    SpatialIndex(const SpatialIndex&) = delete;
    SpatialIndex& operator=(const SpatialIndex&) = delete;

    // Move operations allowed
    SpatialIndex(SpatialIndex&&) noexcept = default;
    SpatialIndex& operator=(SpatialIndex&&) noexcept = default;

    /**
     * @brief Destructor
     *
     * Properly deallocates all R-tree nodes.
     */
    ~SpatialIndex() = default;

    /**
     * @brief Insert a single object at the given coordinates
     *
     * Adds an object to the spatial index. If the ID already exists,
     * the existing entry is updated with the new coordinates.
     *
     * @param id Unique identifier for the object
     * @param ra Right Ascension in degrees (0-360)
     * @param dec Declination in degrees (-90 to +90)
     * @thread_safe Thread-safe for concurrent insertion
     *
     * @example
     * ```cpp
     * index.insert("Sirius", 101.287, -16.716);
     * ```
     */
    void insert(const ObjectId& id, double ra, double dec);

    /**
     * @brief Insert multiple objects in batch mode
     *
     * Efficiently inserts multiple objects by acquiring the lock once
     * for all operations. Preferred for bulk loading datasets.
     *
     * @param objects Span of (ID, RA, Dec) tuples
     * @thread_safe Thread-safe for concurrent insertion
     *
     * @example
     * ```cpp
     * std::vector<std::tuple<std::string, double, double>> stars = {
     *     {"Sirius", 101.287, -16.716},
     *     {"Vega", 279.235, 38.783},
     *     {"Altair", 297.696, 8.861}
     * };
     * index.insertBatch(stars);
     * ```
     */
    void insertBatch(
        std::span<const std::tuple<ObjectId, double, double>> objects);

    /**
     * @brief Search for objects within a spherical radius
     *
     * Performs a radius search on the celestial sphere using spherical
     * (great circle) distance calculation. Returns all objects within
     * the specified radius, sorted by distance.
     *
     * @param ra Center Right Ascension in degrees
     * @param dec Center Declination in degrees
     * @param radius Search radius in degrees
     * @param limit Maximum number of results to return
     * @return Vector of (ObjectId, distance) pairs sorted by distance
     * @thread_safe Thread-safe for concurrent reads
     *
     * @example
     * ```cpp
     * auto nearby = index.searchRadius(85.375, -2.27, 10.0, 50);
     * for (const auto& [id, dist] : nearby) {
     *     printf("%s: %.2f degrees away\n", id.c_str(), dist);
     * }
     * ```
     */
    [[nodiscard]] auto searchRadius(double ra, double dec, double radius,
                                     size_t limit = 100) const
        -> std::vector<SearchResult>;

    /**
     * @brief Search for objects within a rectangular bounding box
     *
     * Performs an axis-aligned rectangular search on the RA/Dec plane.
     * This is faster than radius search for certain queries but returns
     * results without distance information.
     *
     * @param minRa Minimum Right Ascension (degrees)
     * @param maxRa Maximum Right Ascension (degrees)
     * @param minDec Minimum Declination (degrees)
     * @param maxDec Maximum Declination (degrees)
     * @param limit Maximum number of results to return
     * @return Vector of matching ObjectIds
     * @thread_safe Thread-safe for concurrent reads
     *
     * @note Results are not sorted by distance
     *
     * @example
     * ```cpp
     * auto results = index.searchBox(80.0, 90.0, -5.0, 0.0);
     * ```
     */
    [[nodiscard]] auto searchBox(double minRa, double maxRa, double minDec,
                                  double maxDec, size_t limit = 100) const
        -> std::vector<ObjectId>;

    /**
     * @brief Remove an object from the index
     *
     * Removes the object with the given ID from the spatial index.
     * If the ID doesn't exist, this is a no-op.
     *
     * @param id The object ID to remove
     * @thread_safe Thread-safe for exclusive write
     *
     * @example
     * ```cpp
     * index.remove("Sirius");
     * ```
     */
    void remove(const ObjectId& id);

    /**
     * @brief Clear all objects from the index
     *
     * Removes all entries and resets the index to its initial state.
     *
     * @thread_safe Thread-safe for exclusive write
     */
    void clear();

    /**
     * @brief Get the total number of objects in the index
     *
     * @return Number of indexed objects
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Check if the index contains an object with the given ID
     *
     * @param id Object ID to search for
     * @return True if the object exists in the index
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto contains(const ObjectId& id) const -> bool;

    /**
     * @brief Get the coordinates of an object
     *
     * @param id Object ID to look up
     * @return Point (RA, Dec) if found, (0, 0) if not found
     * @thread_safe Thread-safe for concurrent reads
     */
    [[nodiscard]] auto getCoordinates(const ObjectId& id) const -> Point;

private:
    /**
     * @brief Bounding box in 2D space
     *
     * Represents a rectangular area in the RA/Dec coordinate space.
     */
    struct BoundingBox {
        double minRa;   ///< Minimum RA boundary
        double maxRa;   ///< Maximum RA boundary
        double minDec;  ///< Minimum Dec boundary
        double maxDec;  ///< Maximum Dec boundary

        /**
         * @brief Calculate the area of this bounding box
         * @return Area in square degrees
         */
        [[nodiscard]] auto area() const -> double {
            return (maxRa - minRa) * (maxDec - minDec);
        }

        /**
         * @brief Check if a point is contained in this box
         * @param ra Right Ascension
         * @param dec Declination
         * @return True if point is inside the box
         */
        [[nodiscard]] auto contains(double ra, double dec) const -> bool {
            return ra >= minRa && ra <= maxRa && dec >= minDec &&
                   dec <= maxDec;
        }
    };

    /**
     * @brief Internal node structure for the R-tree
     *
     * Leaf nodes store actual data entries, while branch nodes store
     * child node pointers and their bounding boxes.
     */
    struct RTreeNode {
        bool isLeaf;                  ///< True for leaf nodes
        BoundingBox bbox;             ///< Bounding box of all children
        std::vector<ObjectId> ids;    ///< Object IDs (leaf nodes only)
        std::vector<Point> points;    ///< Object coordinates (leaf nodes only)
        std::vector<std::unique_ptr<RTreeNode>> children;  ///< Child nodes
    };

    /**
     * @brief Calculate spherical distance between two points
     *
     * Uses the Haversine formula for great circle distance on a sphere.
     * Assumes Earth's radius for consistency.
     *
     * @param ra1 First point's RA (degrees)
     * @param dec1 First point's Dec (degrees)
     * @param ra2 Second point's RA (degrees)
     * @param dec2 Second point's Dec (degrees)
     * @return Distance in degrees
     */
    [[nodiscard]] static auto sphericalDistance(double ra1, double dec1,
                                                 double ra2,
                                                 double dec2) noexcept
        -> double;

    /**
     * @brief Convert degrees to radians
     * @param degrees Angle in degrees
     * @return Angle in radians
     */
    [[nodiscard]] static constexpr auto degreesToRadians(double degrees)
        noexcept -> double {
        return degrees * 3.14159265358979323846 / 180.0;
    }

    /**
     * @brief Insert into the R-tree recursively
     * @param node Current node
     * @param id Object ID
     * @param ra Right Ascension
     * @param dec Declination
     */
    void insertNode(RTreeNode* node, const ObjectId& id, double ra,
                    double dec);

    /**
     * @brief Split a node when it exceeds capacity
     * @param node Node to split
     * @param idToAdd New object ID
     * @param raToAdd New RA
     * @param decToAdd New Dec
     */
    void splitNode(RTreeNode* node, const ObjectId& idToAdd, double raToAdd,
                   double decToAdd);

    /**
     * @brief Search radius recursively
     * @param node Current node
     * @param ra Center RA
     * @param dec Center Dec
     * @param radius Search radius
     * @param results Result vector
     * @param limit Result limit
     */
    void searchRadiusNode(const RTreeNode* node, double ra, double dec,
                          double radius, std::vector<SearchResult>& results,
                          size_t limit) const;

    /**
     * @brief Search box recursively
     * @param node Current node
     * @param box Search box
     * @param results Result vector
     * @param limit Result limit
     */
    void searchBoxNode(const RTreeNode* node, const BoundingBox& box,
                       std::vector<ObjectId>& results, size_t limit) const;

    /**
     * @brief Update bounding box of a node
     * @param node Node to update
     */
    void updateBoundingBox(RTreeNode* node);

    /**
     * @brief Find node containing the object ID
     * @param node Current node
     * @param id Object ID to find
     * @return Pointer to containing node, or nullptr
     */
    [[nodiscard]] auto findNodeContaining(RTreeNode* node,
                                           const ObjectId& id)
        -> RTreeNode*;

    // Configuration
    size_t maxEntries_;  ///< Maximum entries per node
    size_t minEntries_;  ///< Minimum entries per node

    // Tree structure
    std::unique_ptr<RTreeNode> root_;  ///< Root node of R-tree
    std::unordered_map<ObjectId, Point> objectMap_;  ///< Quick lookup by ID

    // Thread synchronization
    mutable std::shared_mutex mutex_;
};

}  // namespace lithium::target::index

#endif  // LITHIUM_TARGET_INDEX_SPATIAL_INDEX_HPP
