#ifndef LITHIUM_TARGET_ENGINE_HPP
#define LITHIUM_TARGET_ENGINE_HPP

#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "atom/macro.hpp"
#include "atom/type/json_fwd.hpp"

#include "reader.hpp"

namespace lithium::target {

/**
 * @brief Represents a celestial astronomical object with detailed properties
 *
 * This class stores information about celestial objects like stars, galaxies,
 * nebulae, etc., including their catalog information, positional data,
 * physical characteristics, and descriptive details.
 */
class CelestialObject {
public:
    /**
     * @brief Constructs a celestial object with all properties
     *
     * @param id Unique identifier
     * @param identifier Primary catalog identifier
     * @param mIdentifier Messier catalog identifier
     * @param extensionName Extended name
     * @param component Component information
     * @param className Classification name
     * @param amateurRank Observer difficulty ranking
     * @param chineseName Chinese name of the object
     * @param type Object type (galaxy, nebula, star cluster, etc.)
     * @param duplicateType Type including duplicates
     * @param morphology Morphological classification
     * @param constellationCn Constellation name in Chinese
     * @param constellationEn Constellation name in English
     * @param raJ2000 Right ascension (J2000) in string format
     * @param raDJ2000 Right ascension (J2000) in decimal degrees
     * @param decJ2000 Declination (J2000) in string format
     * @param decDJ2000 Declination (J2000) in decimal degrees
     * @param visualMagnitudeV Visual magnitude (V band)
     * @param photographicMagnitudeB Photographic magnitude (B band)
     * @param bMinusV B-V color index
     * @param surfaceBrightness Surface brightness in mag/arcmin²
     * @param majorAxis Major axis size in arcmin
     * @param minorAxis Minor axis size in arcmin
     * @param positionAngle Position angle in degrees
     * @param detailedDescription Detailed object description
     * @param briefDescription Brief object description
     */
    CelestialObject(std::string id, std::string identifier,
                    std::string mIdentifier, std::string extensionName,
                    std::string component, std::string className,
                    std::string amateurRank, std::string chineseName,
                    std::string type, std::string duplicateType,
                    std::string morphology, std::string constellationCn,
                    std::string constellationEn, std::string raJ2000,
                    double raDJ2000, std::string decJ2000, double decDJ2000,
                    double visualMagnitudeV, double photographicMagnitudeB,
                    double bMinusV, double surfaceBrightness, double majorAxis,
                    double minorAxis, int positionAngle,
                    std::string detailedDescription,
                    std::string briefDescription)
        : ID(std::move(id)),
          Identifier(std::move(identifier)),
          MIdentifier(std::move(mIdentifier)),
          ExtensionName(std::move(extensionName)),
          Component(std::move(component)),
          ClassName(std::move(className)),
          AmateurRank(std::move(amateurRank)),
          ChineseName(std::move(chineseName)),
          Type(std::move(type)),
          DuplicateType(std::move(duplicateType)),
          Morphology(std::move(morphology)),
          ConstellationCn(std::move(constellationCn)),
          ConstellationEn(std::move(constellationEn)),
          RAJ2000(std::move(raJ2000)),
          RADJ2000(raDJ2000),
          DecJ2000(std::move(decJ2000)),
          DecDJ2000(decDJ2000),
          VisualMagnitudeV(visualMagnitudeV),
          PhotographicMagnitudeB(photographicMagnitudeB),
          BMinusV(bMinusV),
          SurfaceBrightness(surfaceBrightness),
          MajorAxis(majorAxis),
          MinorAxis(minorAxis),
          PositionAngle(positionAngle),
          DetailedDescription(std::move(detailedDescription)),
          BriefDescription(std::move(briefDescription)) {}

    /**
     * @brief Default constructor
     */
    CelestialObject() = default;

    /**
     * @brief Deserialize a celestial object from JSON data
     * 
     * @param j JSON object containing celestial object data
     * @return CelestialObject instance populated with JSON data
     */
    static auto from_json(const nlohmann::json& j) -> CelestialObject;

    /**
     * @brief Serialize the celestial object to JSON
     * 
     * @return JSON object representation of the celestial object
     */
    [[nodiscard]] auto to_json() const -> nlohmann::json;

    /**
     * @brief Get the name (identifier) of the celestial object
     * 
     * @return The object's primary identifier
     */
    [[nodiscard]] const std::string& getName() const { return Identifier; }

    // Object properties
    std::string ID;                      ///< Unique identifier
    std::string Identifier;              ///< Primary catalog identifier
    std::string MIdentifier;             ///< Messier catalog identifier
    std::string ExtensionName;           ///< Extended name
    std::string Component;               ///< Component information
    std::string ClassName;               ///< Classification name
    std::string AmateurRank;             ///< Observer difficulty ranking
    std::string ChineseName;             ///< Chinese name of the object
    std::string Type;                    ///< Object type
    std::string DuplicateType;           ///< Type including duplicates
    std::string Morphology;              ///< Morphological classification
    std::string ConstellationCn;         ///< Constellation name in Chinese
    std::string ConstellationEn;         ///< Constellation name in English
    std::string RAJ2000;                 ///< Right ascension (J2000) in string format
    double RADJ2000;                     ///< Right ascension (J2000) in decimal degrees
    std::string DecJ2000;                ///< Declination (J2000) in string format
    double DecDJ2000;                    ///< Declination (J2000) in decimal degrees
    double VisualMagnitudeV;             ///< Visual magnitude (V band)
    double PhotographicMagnitudeB;       ///< Photographic magnitude (B band)
    double BMinusV;                      ///< B-V color index
    double SurfaceBrightness;            ///< Surface brightness in mag/arcmin²
    double MajorAxis;                    ///< Major axis size in arcmin
    double MinorAxis;                    ///< Minor axis size in arcmin
    int PositionAngle;                   ///< Position angle in degrees
    std::string DetailedDescription;     ///< Detailed object description
    std::string BriefDescription;        ///< Brief object description
};

/**
 * @brief Represents a star object with reference to CelestialObject data
 * 
 * This class provides additional metadata like alternative names (aliases)
 * and usage statistics (click count) on top of the celestial object data.
 */
class StarObject {
private:
    std::string name_;                  ///< Primary name of the star
    std::vector<std::string> aliases_;  ///< Alternative names
    int clickCount_;                    ///< Usage count for popularity ranking
    CelestialObject celestialObject_;   ///< Associated celestial data

public:
    /**
     * @brief Constructs a star object with name and aliases
     * 
     * @param name Primary name of the star
     * @param aliases Alternative names for the star
     * @param clickCount Usage count, defaults to 0
     */
    StarObject(std::string name, std::vector<std::string> aliases,
               int clickCount = 0);

    /**
     * @brief Default constructor
     */
    StarObject() : name_(""), aliases_({}), clickCount_(0) {}

    /**
     * @brief Get the primary name of the star
     * 
     * @return Star's primary name
     */
    [[nodiscard]] const std::string& getName() const;
    
    /**
     * @brief Get all alternative names (aliases) of the star
     * 
     * @return Vector of alias strings
     */
    [[nodiscard]] const std::vector<std::string>& getAliases() const;
    
    /**
     * @brief Get the popularity count of the star
     * 
     * @return Click count integer
     */
    [[nodiscard]] int getClickCount() const;

    /**
     * @brief Set the primary name of the star
     * 
     * @param name New primary name
     */
    void setName(const std::string& name);
    
    /**
     * @brief Set all alternative names (aliases) of the star
     * 
     * @param aliases New vector of aliases
     */
    void setAliases(const std::vector<std::string>& aliases);
    
    /**
     * @brief Set the popularity count of the star
     * 
     * @param clickCount New click count value
     */
    void setClickCount(int clickCount);

    /**
     * @brief Associate celestial object data with this star
     * 
     * @param celestialObject CelestialObject containing detailed astronomical data
     */
    void setCelestialObject(const CelestialObject& celestialObject);
    
    /**
     * @brief Get the associated celestial object data
     * 
     * @return CelestialObject with detailed astronomical data
     */
    [[nodiscard]] CelestialObject getCelestialObject() const;
    
    /**
     * @brief Serialize the star object to JSON
     * 
     * @return JSON object representation of the star
     */
    [[nodiscard]] nlohmann::json to_json() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Trie (prefix tree) for efficient string storage and retrieval
 *
 * Used for efficient storage and retrieval of strings, especially
 * useful for auto-completion tasks.
 */
class Trie {
    /**
     * @brief Node in the Trie data structure
     */
    struct TrieNode {
        std::unordered_map<char, TrieNode*> children;  ///< Child nodes
        bool isEndOfWord = false;                      ///< Marks end of a word
    } ATOM_ALIGNAS(64);

public:
    /**
     * @brief Constructs an empty Trie
     */
    Trie();

    /**
     * @brief Destructor, releases memory
     */
    ~Trie();

    // Disabled copy constructor and assignment operator
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    // Default move constructor and assignment operator
    Trie(Trie&&) noexcept = default;
    Trie& operator=(Trie&&) noexcept = default;

    /**
     * @brief Inserts a word into the Trie
     *
     * @param word Word to insert
     */
    void insert(const std::string& word);

    /**
     * @brief Provides auto-completion suggestions for a given prefix
     *
     * @param prefix Prefix to search for
     * @return Vector of auto-completion suggestions
     */
    [[nodiscard]] auto autoComplete(const std::string& prefix) const
        -> std::vector<std::string>;

private:
    /**
     * @brief Depth-first search to collect all words with a given prefix
     *
     * @param node Current TrieNode being visited
     * @param prefix Current formed prefix
     * @param suggestions Vector to collect suggestions
     */
    void dfs(TrieNode* node, const std::string& prefix,
             std::vector<std::string>& suggestions) const;

    /**
     * @brief Recursively frees memory of Trie nodes
     *
     * @param node Current TrieNode to free
     */
    void clear(TrieNode* node);

    TrieNode* root_;  ///< Root node of the Trie
};

/**
 * @brief Search engine for celestial objects
 * 
 * Provides functionality to search, filter, and recommend celestial objects
 * based on various criteria and user preferences.
 */
class SearchEngine {
public:
    /**
     * @brief Constructor
     */
    SearchEngine();
    
    /**
     * @brief Destructor
     */
    ~SearchEngine();

    /**
     * @brief Add a star object to the search index
     * 
     * @param starObject StarObject to be indexed
     */
    void addStarObject(const StarObject& starObject);

    /**
     * @brief Search for a star object by exact name or alias
     * 
     * @param query Search query string
     * @return Vector of matching star objects
     */
    [[nodiscard]] auto searchStarObject(const std::string& query) const
        -> std::vector<StarObject>;

    /**
     * @brief Perform fuzzy search for star objects
     * 
     * @param query Search query string
     * @param tolerance Maximum edit distance for matches
     * @return Vector of matching star objects
     */
    [[nodiscard]] auto fuzzySearchStarObject(const std::string& query,
                                             int tolerance) const
        -> std::vector<StarObject>;

    /**
     * @brief Provide auto-completion suggestions for star names
     * 
     * @param prefix Prefix to auto-complete
     * @return Vector of name suggestions
     */
    [[nodiscard]] auto autoCompleteStarObject(const std::string& prefix) const
        -> std::vector<std::string>;

    /**
     * @brief Rank search results by popularity (click count)
     * 
     * @param results Vector of search results to rank
     * @return Vector of ranked search results
     */
    [[nodiscard]] static auto getRankedResults(std::vector<StarObject>& results)
        -> std::vector<StarObject>;

    /**
     * @brief Load star object names and aliases from JSON file
     * 
     * @param filename Path to the JSON file
     * @return True if loading was successful
     */
    bool loadFromNameJson(const std::string& filename);
    
    /**
     * @brief Load celestial object data from JSON file
     * 
     * @param filename Path to the JSON file
     * @return True if loading was successful
     */
    bool loadFromCelestialJson(const std::string& filename);

    /**
     * @brief Search for objects with specific filtering criteria
     * 
     * @param type Object type filter
     * @param morphology Morphological classification filter
     * @param minMagnitude Minimum visual magnitude
     * @param maxMagnitude Maximum visual magnitude
     * @return Vector of star objects matching the criteria
     */
    [[nodiscard]] auto filterSearch(
        const std::string& type = "", const std::string& morphology = "",
        double minMagnitude = -std::numeric_limits<double>::infinity(),
        double maxMagnitude = std::numeric_limits<double>::infinity()) const
        -> std::vector<StarObject>;

    /**
     * @brief Initialize the recommendation engine with a model
     * 
     * @param modelFilename Path to the model file
     * @return True if initialization was successful
     */
    bool initializeRecommendationEngine(const std::string& modelFilename);
    
    /**
     * @brief Add a user rating for a celestial object
     * 
     * @param user User identifier
     * @param item Item (star) identifier
     * @param rating User rating value
     */
    void addUserRating(const std::string& user, const std::string& item,
                       double rating);
    
    /**
     * @brief Get recommended items for a user
     * 
     * @param user User identifier
     * @param topN Number of recommendations to return
     * @return Vector of recommended items with scores
     */
    std::vector<std::pair<std::string, double>> recommendItems(
        const std::string& user, int topN = 5) const;
    
    /**
     * @brief Save the recommendation model to file
     * 
     * @param filename Path to save the model
     * @return True if saving was successful
     */
    bool saveRecommendationModel(const std::string& filename) const;
    
    /**
     * @brief Load a recommendation model from file
     * 
     * @param filename Path to the model file
     * @return True if loading was successful
     */
    bool loadRecommendationModel(const std::string& filename);
    
    /**
     * @brief Train the recommendation engine on current data
     */
    void trainRecommendationEngine();

    /**
     * @brief Load data from CSV file
     * 
     * @param filename Path to the CSV file
     * @param requiredFields List of required field names
     * @param dialect CSV dialect specifications
     * @return True if loading was successful
     */
    bool loadFromCSV(const std::string& filename,
                     const std::vector<std::string>& requiredFields,
                     Dialect dialect = Dialect());

    /**
     * @brief Get hybrid recommendations combining content-based and collaborative filtering
     * 
     * @param user User identifier
     * @param topN Number of recommendations to return
     * @param contentWeight Weight for content-based recommendations
     * @param collaborativeWeight Weight for collaborative filtering recommendations
     * @return Vector of recommended items with scores
     */
    auto getHybridRecommendations(const std::string& user, int topN = 5,
                                  double contentWeight = 0.3,
                                  double collaborativeWeight = 0.7)
        -> std::vector<std::pair<std::string, double>>;

    /**
     * @brief Export data to CSV file
     * 
     * @param filename Path to the output CSV file
     * @param fields List of fields to export
     * @param dialect CSV dialect specifications
     * @return True if export was successful
     */
    bool exportToCSV(const std::string& filename,
                     const std::vector<std::string>& fields,
                     Dialect dialect = Dialect()) const;

    /**
     * @brief Process ratings from a CSV file in batch
     * 
     * @param csvFilename Path to the CSV file with ratings
     */
    void batchProcessRatings(const std::string& csvFilename);
    
    /**
     * @brief Update star objects from a CSV file in batch
     * 
     * @param csvFilename Path to the CSV file with star object data
     */
    void batchUpdateStarObjects(const std::string& csvFilename);

    /**
     * @brief Clear the search results cache
     */
    void clearCache();
    
    /**
     * @brief Set the cache size
     * 
     * @param size New cache size
     */
    void setCacheSize(size_t size);
    
    /**
     * @brief Get cache statistics
     * 
     * @return String with cache statistics
     */
    [[nodiscard]] auto getCacheStats() const -> std::string;

    /**
     * @brief Optimize the recommendation engine
     */
    void optimizeRecommendationEngine();

    /**
     * @brief Get recommendation engine statistics
     * 
     * @return String with recommendation engine stats
     */
    [[nodiscard]] auto getRecommendationEngineStats() const -> std::string;

    /**
     * @brief Add implicit feedback (view, click) for a user-item pair
     * 
     * @param user User identifier
     * @param item Item identifier
     */
    void addImplicitFeedback(const std::string& user, const std::string& item);

    /**
     * @brief Export recommendation data to CSV
     * 
     * @param filename Path to output CSV file
     * @return True if export was successful
     */
    bool exportRecommendationDataToCSV(const std::string& filename) const;

    /**
     * @brief Import recommendation data from CSV
     * 
     * @param filename Path to input CSV file
     * @return True if import was successful
     */
    bool importRecommendationDataFromCSV(const std::string& filename);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;  ///< Implementation details (PIMPL idiom)
};

}  // namespace lithium::target

#endif  // LITHIUM_TARGET_ENGINE_HPP