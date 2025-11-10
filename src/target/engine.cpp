#include "engine.hpp"
#include "preference.hpp"

#include <algorithm>
#include <fstream>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "atom/search/lru.hpp"
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

using json = nlohmann::json;

namespace lithium::target {

// --------------------- CelestialObject Implementation ---------------------

auto CelestialObject::from_json(const json& j) -> CelestialObject {
    spdlog::info("Deserializing CelestialObject from JSON");
    try {
        CelestialObject obj(
            j.at("ID").get<std::string>(), j.at("标识").get<std::string>(),
            j.at("M标识").get<std::string>(), j.at("拓展名").get<std::string>(),
            j.at("组件").get<std::string>(), j.at("Class").get<std::string>(),
            j.at("业余排名").get<std::string>(),
            j.at("中文名").get<std::string>(), j.at("类型").get<std::string>(),
            j.at("含重复类型").get<std::string>(),
            j.at("形态").get<std::string>(),
            j.at("星座(Zh)").get<std::string>(),
            j.at("星座(En)").get<std::string>(),
            j.at("赤经(J2000)").get<std::string>(),
            j.at("赤经D(J2000)").get<double>(),
            j.at("赤纬(J2000)").get<std::string>(),
            j.at("赤纬D(J2000)").get<double>(),
            j.at("可见光星等V").get<double>(),
            j.at("摄影(蓝光)星等B").get<double>(), j.at("B-V").get<double>(),
            j.at("表面亮度(mag/arcmin2)").get<double>(),
            j.at("长轴(分)").get<double>(), j.at("短轴(分)").get<double>(),
            j.at("方位角").get<int>(), j.at("详细描述").get<std::string>(),
            j.at("简略描述").get<std::string>());
        spdlog::info("Successfully deserialized CelestialObject with ID: {}",
                     obj.ID);
        return obj;
    } catch (const json::exception& e) {
        spdlog::error(
            "JSON deserialization error in CelestialObject::from_json: {}",
            e.what());
        throw;
    }
}

auto CelestialObject::to_json() const -> json {
    spdlog::info("Serializing CelestialObject with ID: {}", ID);
    try {
        return json{{"ID", ID},
                    {"标识", Identifier},
                    {"M标识", MIdentifier},
                    {"拓展名", ExtensionName},
                    {"组件", Component},
                    {"Class", ClassName},
                    {"业余排名", AmateurRank},
                    {"中文名", ChineseName},
                    {"类型", Type},
                    {"含重复类型", DuplicateType},
                    {"形态", Morphology},
                    {"星座(Zh)", ConstellationCn},
                    {"星座(En)", ConstellationEn},
                    {"赤经(J2000)", RAJ2000},
                    {"赤经D(J2000)", RADJ2000},
                    {"赤纬(J2000)", DecJ2000},
                    {"赤纬D(J2000)", DecDJ2000},
                    {"可见光星等V", VisualMagnitudeV},
                    {"摄影(蓝光)星等B", PhotographicMagnitudeB},
                    {"B-V", BMinusV},
                    {"表面亮度(mag/arcmin2)", SurfaceBrightness},
                    {"长轴(分)", MajorAxis},
                    {"短轴(分)", MinorAxis},
                    {"方位角", PositionAngle},
                    {"详细描述", DetailedDescription},
                    {"简略描述", BriefDescription}};
    } catch (const json::exception& e) {
        spdlog::error(
            "JSON serialization error in CelestialObject::to_json: {}",
            e.what());
        throw;
    }
}

// --------------------- StarObject Implementation ---------------------

StarObject::StarObject(std::string name, std::vector<std::string> aliases,
                       int clickCount)
    : name_(std::move(name)),
      aliases_(std::move(aliases)),
      clickCount_(clickCount) {
    spdlog::info("Constructed StarObject with name: {}", name_);
}

const std::string& StarObject::getName() const {
    spdlog::debug("Accessing StarObject::getName for {}", name_);
    return name_;
}

const std::vector<std::string>& StarObject::getAliases() const {
    spdlog::debug("Accessing StarObject::getAliases for {}", name_);
    return aliases_;
}

int StarObject::getClickCount() const {
    spdlog::debug("Accessing StarObject::getClickCount for {}", name_);
    return clickCount_;
}

void StarObject::setName(const std::string& name) {
    spdlog::info("Setting name from {} to {}", name_, name);
    name_ = name;
}

void StarObject::setAliases(const std::vector<std::string>& aliases) {
    spdlog::info("Setting aliases for {}", name_);
    aliases_ = aliases;
}

void StarObject::setClickCount(int clickCount) {
    spdlog::info("Setting clickCount for {} to {}", name_, clickCount);
    clickCount_ = clickCount;
}

void StarObject::setCelestialObject(const CelestialObject& celestialObject) {
    spdlog::info("Associating CelestialObject with ID: {} to StarObject: {}",
                 celestialObject.ID, name_);
    celestialObject_ = celestialObject;
}

CelestialObject StarObject::getCelestialObject() const {
    spdlog::debug("Accessing CelestialObject for StarObject: {}", name_);
    return celestialObject_;
}

auto StarObject::to_json() const -> json {
    spdlog::info("Serializing StarObject: {}", name_);
    try {
        return json{{"name", name_},
                    {"aliases", aliases_},
                    {"clickCount", clickCount_},
                    {"celestialObject", celestialObject_.to_json()}};
    } catch (const json::exception& e) {
        spdlog::error(
            "JSON serialization error in StarObject::to_json for {}: {}", name_,
            e.what());
        throw;
    }
}

// --------------------- Trie Implementation ---------------------

Trie::Trie() {
    root_ = new TrieNode();
    spdlog::info("Initialized Trie");
}

Trie::~Trie() {
    clear(root_);
    spdlog::info("Destroyed Trie");
}

void Trie::insert(const std::string& word) {
    spdlog::debug("Inserting word into Trie: {}", word);
    TrieNode* current = root_;
    for (const char& ch : word) {
        if (current->children.find(ch) == current->children.end()) {
            spdlog::trace("Creating new TrieNode for character: {}", ch);
            current->children[ch] = new TrieNode();
        }
        current = current->children[ch];
    }
    current->isEndOfWord = true;
    spdlog::debug("Finished inserting word into Trie: {}", word);
}

auto Trie::autoComplete(const std::string& prefix) const
    -> std::vector<std::string> {
    spdlog::debug("Auto-completing prefix: {}", prefix);
    std::vector<std::string> suggestions;
    TrieNode* current = root_;
    for (const char& ch : prefix) {
        if (current->children.find(ch) == current->children.end()) {
            spdlog::debug("Prefix '{}' not found in Trie", prefix);
            return suggestions;  // Prefix not found
        }
        current = current->children[ch];
    }
    spdlog::debug("Prefix '{}' found. Performing DFS for suggestions", prefix);
    dfs(current, prefix, suggestions);
    spdlog::debug("Auto-complete found {} suggestions for prefix: {}",
                  suggestions.size(), prefix);
    return suggestions;
}

void Trie::dfs(TrieNode* node, const std::string& prefix,
               std::vector<std::string>& suggestions) const {
    if (node->isEndOfWord) {
        suggestions.emplace_back(prefix);
        spdlog::trace("Found word in Trie during DFS: {}", prefix);
    }
    for (const auto& [ch, child] : node->children) {
        dfs(child, prefix + ch, suggestions);
    }
}

void Trie::clear(TrieNode* node) {
    if (node == nullptr) {
        return;
    }
    for (auto& [ch, child] : node->children) {
        clear(child);
    }
    spdlog::trace("Deleting TrieNode");
    delete node;
}

// --------------------- SearchEngine::Impl Implementation ---------------------

class SearchEngine::Impl {
public:
    static constexpr size_t DEFAULT_CACHE_CAPACITY = 100;

    Impl() : queryCache_(DEFAULT_CACHE_CAPACITY) {
        spdlog::info("SearchEngine initialized with cache capacity {}",
                     DEFAULT_CACHE_CAPACITY);
    }

    ~Impl() { spdlog::info("SearchEngine destroyed"); }

    bool initializeRecommendationEngine(const std::string& modelFilename) {
        try {
            recommendationEngine_.loadModel(modelFilename);
            spdlog::info("Recommendation Engine loaded model from '{}'",
                         modelFilename);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to load Recommendation Engine model: {}",
                          e.what());
            return false;
        }
    }

    void addStarObject(const StarObject& starObject) {
        std::unique_lock lock(indexMutex_);
        spdlog::info("Adding StarObject: {}", starObject.getName());
        try {
            auto result =
                starObjectIndex_.emplace(starObject.getName(), starObject);
            if (!result.second) {
                spdlog::warn(
                    "StarObject with name '{}' already exists. Overwriting",
                    starObject.getName());
                starObjectIndex_[starObject.getName()] = starObject;
            }
            trie_.insert(starObject.getName());
            for (const auto& alias : starObject.getAliases()) {
                trie_.insert(alias);
                aliasIndex_.emplace(alias, starObject.getName());
                spdlog::debug("Added alias '{}' for StarObject '{}'", alias,
                              starObject.getName());
            }
            spdlog::info("Successfully added StarObject: {}",
                         starObject.getName());

            // Add to recommendation engine
            recommendationEngine_.addItem(starObject.getName(),
                                          starObject.getAliases());
        } catch (const std::exception& e) {
            spdlog::error("Exception in addStarObject for {}: {}",
                          starObject.getName(), e.what());
        }
    }

    void addUserRating(const std::string& user, const std::string& item,
                       double rating) {
        try {
            recommendationEngine_.addRating(user, item, rating);
            spdlog::info("Added rating: User '{}', Item '{}', Rating {}", user,
                         item, rating);
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception in addUserRating for user '{}', item '{}': {}", user,
                item, e.what());
        }
    }

    std::vector<StarObject> searchStarObject(const std::string& query) const {
        spdlog::info("Searching for StarObject with query: {}", query);
        std::shared_lock lock(indexMutex_);
        try {
            if (auto cached = queryCache_.get(query)) {
                spdlog::info("Cache hit for query: {}", query);
                return *cached;
            }

            std::vector<StarObject> results;
            auto it = starObjectIndex_.find(query);
            if (it != starObjectIndex_.end()) {
                results.emplace_back(it->second);
                spdlog::debug("Found StarObject by name: {}", query);
            }

            // Search by alias
            auto aliasRange = aliasIndex_.equal_range(query);
            for (auto itr = aliasRange.first; itr != aliasRange.second; ++itr) {
                auto starIt = starObjectIndex_.find(itr->second);
                if (starIt != starObjectIndex_.end()) {
                    results.emplace_back(starIt->second);
                    spdlog::debug("Found StarObject '{}' by alias '{}'",
                                  starIt->second.getName(), query);
                }
            }

            if (!results.empty()) {
                queryCache_.put(query, results);
                spdlog::info("Search completed for query: {} with {} results",
                             query, results.size());
            } else {
                spdlog::info("No results found for query: {}", query);
            }

            return results;
        } catch (const std::exception& e) {
            spdlog::error("Exception in searchStarObject for query '{}': {}",
                          query, e.what());
            return {};
        }
    }

    std::vector<StarObject> fuzzySearchStarObject(const std::string& query,
                                                  int tolerance) const {
        spdlog::info(
            "Performing fuzzy search for query: '{}' with tolerance: {}", query,
            tolerance);
        std::shared_lock lock(indexMutex_);
        std::vector<StarObject> results;
        try {
            for (const auto& [name, starObject] : starObjectIndex_) {
                if (levenshteinDistance(query, name) <= tolerance) {
                    results.emplace_back(starObject);
                    spdlog::debug("Fuzzy match found: {}", name);
                } else {
                    for (const auto& alias : starObject.getAliases()) {
                        if (levenshteinDistance(query, alias) <= tolerance) {
                            results.emplace_back(starObject);
                            spdlog::debug("Fuzzy match found by alias: {}",
                                          alias);
                            break;
                        }
                    }
                }
            }
            spdlog::info(
                "Fuzzy search completed for query: '{}' with {} results", query,
                results.size());
            return results;
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception in fuzzySearchStarObject for query '{}': {}", query,
                e.what());
            return {};
        }
    }

    std::vector<std::string> autoCompleteStarObject(
        const std::string& prefix) const {
        spdlog::info("Auto-completing StarObject with prefix: {}", prefix);
        try {
            auto suggestions = trie_.autoComplete(prefix);
            spdlog::info(
                "Auto-complete retrieved {} suggestions for prefix: {}",
                suggestions.size(), prefix);
            return suggestions;
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception in autoCompleteStarObject for prefix '{}': {}",
                prefix, e.what());
            return {};
        }
    }

    static std::vector<StarObject> getRankedResultsStatic(
        std::vector<StarObject>& results) {
        spdlog::info("Ranking search results by click count");
        std::sort(results.begin(), results.end(),
                  [](const StarObject& a, const StarObject& b) {
                      return a.getClickCount() > b.getClickCount();
                  });
        spdlog::info("Ranking completed. Top result click count: {}",
                     results.empty() ? 0 : results[0].getClickCount());
        return results;
    }

    std::vector<StarObject> filterSearch(const std::string& type,
                                         const std::string& morphology,
                                         double minMagnitude,
                                         double maxMagnitude) const {
        spdlog::info(
            "Performing filtered search with type: '{}', morphology: '{}', "
            "magnitude range: {}-{}",
            type, morphology, minMagnitude, maxMagnitude);
        std::shared_lock lock(indexMutex_);
        std::vector<StarObject> results;
        try {
            for (const auto& [name, starObject] : starObjectIndex_) {
                const auto& celestial = starObject.getCelestialObject();
                bool matches = true;

                if (!type.empty() && celestial.Type != type) {
                    matches = false;
                }

                if (!morphology.empty() && celestial.Morphology != morphology) {
                    matches = false;
                }

                if (celestial.VisualMagnitudeV < minMagnitude ||
                    celestial.VisualMagnitudeV > maxMagnitude) {
                    matches = false;
                }

                if (matches) {
                    results.emplace_back(starObject);
                    spdlog::debug("StarObject '{}' matches filter criteria",
                                  name);
                }
            }
            spdlog::info("Filtered search completed with {} results",
                         results.size());
            return results;
        } catch (const std::exception& e) {
            spdlog::error("Exception in filterSearch: {}", e.what());
            return {};
        }
    }

    bool loadFromNameJson(const std::string& filename) {
        spdlog::info("Loading StarObjects from file: {}", filename);
        std::ifstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file: {}", filename);
            return false;
        }

        json jsonData;
        try {
            file >> jsonData;
            spdlog::info("Successfully read JSON data from {}", filename);
        } catch (const json::parse_error& e) {
            spdlog::error("JSON parsing error while reading {}: {}", filename,
                          e.what());
            return false;
        }

        size_t initialSize = starObjectIndex_.size();

        for (const auto& item : jsonData) {
            if (!item.is_array() || item.size() < 1) {
                spdlog::warn("Invalid entry in {}: {}", filename, item.dump());
                continue;  // Skip invalid entries
            }
            std::string name = item[0].get<std::string>();
            std::vector<std::string> aliases;
            if (item.size() >= 2 && !item[1].is_null()) {
                // Assume aliases are comma-separated
                std::string aliasesStr = item[1].get<std::string>();
                std::stringstream ss(aliasesStr);
                std::string alias;
                while (std::getline(ss, alias, ',')) {
                    // Trim whitespace
                    size_t start = alias.find_first_not_of(" \t");
                    size_t end = alias.find_last_not_of(" \t");
                    if (start != std::string::npos &&
                        end != std::string::npos) {
                        aliases.emplace_back(
                            alias.substr(start, end - start + 1));
                        spdlog::debug("Parsed alias '{}' for StarObject '{}'",
                                      alias.substr(start, end - start + 1),
                                      name);
                    }
                }
            }
            StarObject star(name, aliases, 0);  // Default clickCount is 0
            addStarObject(star);
        }

        size_t loadedCount = starObjectIndex_.size() - initialSize;
        spdlog::info("Loaded {} StarObjects from {}", loadedCount, filename);
        return true;
    }

    bool loadFromCelestialJson(const std::string& filename) {
        spdlog::info("Loading CelestialObjects from file: {}", filename);
        std::ifstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file: {}", filename);
            return false;
        }

        json jsonData;
        try {
            file >> jsonData;
            spdlog::info("Successfully read JSON data from {}", filename);
        } catch (const json::parse_error& e) {
            spdlog::error("JSON parsing error while reading {}: {}", filename,
                          e.what());
            return false;
        }

        int matched = 0;
        int unmatched = 0;

        for (const auto& item : jsonData) {
            try {
                CelestialObject celestialObject =
                    CelestialObject::from_json(item);
                std::string name =
                    celestialObject.getName();  // Identifier is the name

                // Find corresponding StarObject
                auto it = starObjectIndex_.find(name);
                if (it != starObjectIndex_.end()) {
                    it->second.setCelestialObject(celestialObject);
                    matched++;
                    spdlog::debug(
                        "Associated CelestialObject with StarObject '{}'",
                        name);

                    // Update recommendation engine item features
                    recommendationEngine_.addItemFeature(
                        name, "Type", 1.0);  // Example feature
                } else {
                    unmatched++;
                    spdlog::warn(
                        "No matching StarObject found for CelestialObject '{}'",
                        name);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error associating CelestialObject: {}",
                              e.what());
            }
        }

        spdlog::info(
            "Loaded CelestialObjects from {}: Matched {}, Unmatched {}",
            filename, matched, unmatched);
        return true;
    }

    std::vector<std::pair<std::string, double>> recommendItems(
        const std::string& user, int topN = 5) {
        spdlog::info("Requesting top {} recommendations for user '{}'", topN,
                     user);
        try {
            return recommendationEngine_.recommendItems(user, topN);
        } catch (const std::exception& e) {
            spdlog::error("Exception in recommendItems for user '{}': {}", user,
                          e.what());
            return {};
        }
    }

    bool saveRecommendationModel(const std::string& filename) {
        spdlog::info("Saving Recommendation Engine model to '{}'", filename);
        try {
            recommendationEngine_.saveModel(filename);
            spdlog::info(
                "Successfully saved Recommendation Engine model to '{}'",
                filename);
            return true;
        } catch (const std::exception& e) {
            spdlog::error(
                "Failed to save Recommendation Engine model to '{}': {}",
                filename, e.what());
            return false;
        }
    }

    bool loadRecommendationModel(const std::string& filename) {
        spdlog::info("Loading Recommendation Engine model from '{}'", filename);
        try {
            recommendationEngine_.loadModel(filename);
            spdlog::info(
                "Successfully loaded Recommendation Engine model from '{}'",
                filename);
            return true;
        } catch (const std::exception& e) {
            spdlog::error(
                "Failed to load Recommendation Engine model from '{}': {}",
                filename, e.what());
            return false;
        }
    }

    void trainRecommendationEngine() {
        spdlog::info("Starting training of Recommendation Engine");
        try {
            recommendationEngine_.train();
            spdlog::info("Recommendation Engine training completed");
        } catch (const std::exception& e) {
            spdlog::error("Exception during Recommendation Engine training: {}",
                          e.what());
        }
    }

    bool loadFromCSV(const std::string& filename,
                     const std::vector<std::string>& requiredFields,
                     Dialect dialect) {
        try {
            std::ifstream input(filename);
            DictReader reader(input, requiredFields, dialect);

            std::unordered_map<std::string, std::string> row;
            size_t count = 0;

            while (reader.next(row)) {
                processStarObjectFromCSV(row);
                count++;

                if (count % 1000 == 0) {
                    spdlog::info("Processed {} records from CSV", count);
                }
            }

            spdlog::info("Successfully loaded {} records from {}", count,
                         filename);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Error loading from CSV {}: {}", filename, e.what());
            return false;
        }
    }

    auto getHybridRecommendations(const std::string& user, int topN,
                                  double contentWeight,
                                  double collaborativeWeight)
        -> std::vector<std::pair<std::string, double>> {
        try {
            // Get collaborative filtering recommendations
            auto cfRecs = recommendationEngine_.recommendItems(user, topN * 2);

            // Get content-based recommendations
            auto contentRecs = getContentBasedRecommendations(user, topN * 2);

            // Merge results
            std::unordered_map<std::string, double> hybridScores;

            for (const auto& [item, score] : cfRecs) {
                hybridScores[item] = score * collaborativeWeight;
            }

            for (const auto& [item, score] : contentRecs) {
                hybridScores[item] += score * contentWeight;
            }

            // Convert to vector and sort
            std::vector<std::pair<std::string, double>> results(
                hybridScores.begin(), hybridScores.end());

            std::sort(results.begin(), results.end(),
                      [](const auto& a, const auto& b) {
                          return a.second > b.second;
                      });

            // Truncate to top N results
            if (results.size() > static_cast<size_t>(topN)) {
                results.resize(topN);
            }

            return results;
        } catch (const std::exception& e) {
            spdlog::error("Error in hybrid recommendations: {}", e.what());
            return {};
        }
    }

    auto getUserHistory(const std::string& user)
        -> std::unordered_map<std::string, double> {
        spdlog::info("Getting history for user: {}", user);
        try {
            std::unordered_map<std::string, double> history;

            // Get user ratings from recommendation engine
            for (const auto& [name, star] : starObjectIndex_) {
                auto rating = recommendationEngine_.predictRating(user, name);
                if (rating > 0.0) {  // Only record valid ratings
                    history[name] = rating;
                }
            }

            spdlog::info("Retrieved {} historical items for user {}",
                         history.size(), user);
            return history;
        } catch (const std::exception& e) {
            spdlog::error("Error getting user history for {}: {}", user,
                          e.what());
            return {};
        }
    }

    void populateStarObjectRow(
        const StarObject& star,
        std::unordered_map<std::string, std::string>& row) {
        spdlog::debug("Populating row data for StarObject: {}", star.getName());
        try {
            // Basic information
            row["name"] = star.getName();

            // Join aliases with semicolons
            if (!star.getAliases().empty()) {
                std::stringstream ss;
                for (size_t i = 0; i < star.getAliases().size(); ++i) {
                    if (i > 0)
                        ss << ";";
                    ss << star.getAliases()[i];
                }
                row["aliases"] = ss.str();
            } else {
                row["aliases"] = "";
            }

            row["click_count"] = std::to_string(star.getClickCount());

            // Celestial object information
            const auto& celestial = star.getCelestialObject();
            row["id"] = celestial.ID;
            row["type"] = celestial.Type;
            row["morphology"] = celestial.Morphology;
            row["ra_j2000"] = celestial.RAJ2000;
            row["ra_d_j2000"] = std::to_string(celestial.RADJ2000);
            row["dec_j2000"] = celestial.DecJ2000;
            row["dec_d_j2000"] = std::to_string(celestial.DecDJ2000);
            row["visual_magnitude"] =
                std::to_string(celestial.VisualMagnitudeV);

            spdlog::debug("Successfully populated row data for StarObject: {}",
                          star.getName());
        } catch (const std::exception& e) {
            spdlog::error("Error populating row data for {}: {}",
                          star.getName(), e.what());
            throw;
        }
    }

    bool exportToCSV(const std::string& filename,
                     const std::vector<std::string>& fields, Dialect dialect) {
        try {
            std::ofstream output(filename);
            DictWriter writer(output, fields, dialect);

            for (const auto& [name, star] : starObjectIndex_) {
                std::unordered_map<std::string, std::string> row;
                populateStarObjectRow(star, row);
                writer.writeRow(row);
            }

            spdlog::info("Successfully exported data to {}", filename);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Error exporting to CSV {}: {}", filename, e.what());
            return false;
        }
    }

    void processStarObjectFromCSV(
        const std::unordered_map<std::string, std::string>& row) {
        std::string name = row.at("name");
        std::vector<std::string> aliases;

        if (row.count("aliases")) {
            std::stringstream ss(row.at("aliases"));
            std::string alias;
            while (std::getline(ss, alias, ';')) {
                aliases.push_back(trim(alias));
            }
        }

        StarObject star(name, aliases);

        // Set other properties
        if (row.count("click_count")) {
            star.setClickCount(std::stoi(row.at("click_count")));
        }

        addStarObject(star);
    }

    auto getSimilarItems(const std::string& itemId)
        -> std::vector<std::pair<std::string, double>> {
        spdlog::info("Getting similar items for: {}", itemId);
        try {
            // Store similarities and corresponding items
            std::vector<std::pair<std::string, double>> similarities;

            // Get source item
            auto sourceIt = starObjectIndex_.find(itemId);
            if (sourceIt == starObjectIndex_.end()) {
                spdlog::warn("Item {} not found in index", itemId);
                return similarities;
            }

            const auto& sourceCelestial = sourceIt->second.getCelestialObject();

            // Calculate similarity with other items
            for (const auto& [targetId, targetStar] : starObjectIndex_) {
                if (targetId == itemId)
                    continue;

                double similarity = 0.0;
                const auto& targetCelestial = targetStar.getCelestialObject();

                // Type match weight (0.4)
                if (sourceCelestial.Type == targetCelestial.Type) {
                    similarity += 0.4;
                }

                // Position similarity weight (0.3)
                double raDiff = std::abs(sourceCelestial.RADJ2000 -
                                         targetCelestial.RADJ2000);
                double decDiff = std::abs(sourceCelestial.DecDJ2000 -
                                          targetCelestial.DecDJ2000);
                double posSimilarity =
                    1.0 - std::min(1.0, std::sqrt(raDiff * raDiff +
                                                  decDiff * decDiff) /
                                            10.0);
                similarity += 0.3 * posSimilarity;

                // Brightness similarity weight (0.3)
                double magDiff = std::abs(sourceCelestial.VisualMagnitudeV -
                                          targetCelestial.VisualMagnitudeV);
                double magSimilarity = 1.0 - std::min(1.0, magDiff / 5.0);
                similarity += 0.3 * magSimilarity;

                if (similarity > 0.1) {  // Only keep results above threshold
                    similarities.emplace_back(targetId, similarity);
                }
            }

            // Sort by similarity in descending order
            std::sort(similarities.begin(), similarities.end(),
                      [](const auto& a, const auto& b) {
                          return a.second > b.second;
                      });

            // Limit return count
            if (similarities.size() > 20) {
                similarities.resize(20);
            }

            spdlog::info("Found {} similar items for {}", similarities.size(),
                         itemId);
            return similarities;

        } catch (const std::exception& e) {
            spdlog::error("Error getting similar items for {}: {}", itemId,
                          e.what());
            return {};
        }
    }

    auto getContentBasedRecommendations(const std::string& user, int count)
        -> std::vector<std::pair<std::string, double>> {
        std::vector<std::pair<std::string, double>> results;

        // Get user history
        auto userHistory = getUserHistory(user);

        // Calculate recommendations based on user history
        std::unordered_map<std::string, double> scores;
        for (const auto& historyItem : userHistory) {
            auto similar = getSimilarItems(historyItem.first);
            for (const auto& [item, similarity] : similar) {
                scores[item] += similarity * historyItem.second;
            }
        }

        // Convert to vector and sort
        results.assign(scores.begin(), scores.end());
        std::sort(
            results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        if (results.size() > static_cast<size_t>(count)) {
            results.resize(count);
        }

        return results;
    }

    // Helper functions
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return "";

        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    static int levenshteinDistance(const std::string& str1,
                                   const std::string& str2) {
        spdlog::debug("Calculating Levenshtein distance between '{}' and '{}'",
                      str1, str2);
        const size_t len1 = str1.size();
        const size_t len2 = str2.size();

        // Initialize distance matrix
        std::vector<std::vector<int>> distanceMatrix(
            len1 + 1, std::vector<int>(len2 + 1));

        for (size_t i = 0; i <= len1; ++i) {
            distanceMatrix[i][0] = static_cast<int>(i);
        }
        for (size_t j = 0; j <= len2; ++j) {
            distanceMatrix[0][j] = static_cast<int>(j);
        }

        // Calculate edit distance
        for (size_t i = 1; i <= len1; ++i) {
            for (size_t j = 1; j <= len2; ++j) {
                if (str1[i - 1] == str2[j - 1]) {
                    distanceMatrix[i][j] = distanceMatrix[i - 1][j - 1];
                } else {
                    distanceMatrix[i][j] = std::min({
                        distanceMatrix[i - 1][j] + 1,     // Delete
                        distanceMatrix[i][j - 1] + 1,     // Insert
                        distanceMatrix[i - 1][j - 1] + 1  // Replace
                    });
                }
            }
        }

        spdlog::trace("Levenshtein distance between '{}' and '{}' is {}", str1,
                      str2, distanceMatrix[len1][len2]);
        return distanceMatrix[len1][len2];
    }

    // Data members
    std::unordered_map<std::string, StarObject>
        starObjectIndex_;  ///< Key: Star name
    std::unordered_multimap<std::string, std::string>
        aliasIndex_;  ///< Key: Alias, Value: Star name
    Trie trie_;       ///< Prefix tree for auto-completion
    mutable atom::search::ThreadSafeLRUCache<std::string,
                                             std::vector<StarObject>>
        queryCache_;  ///< Thread-safe LRU cache
    mutable std::shared_mutex
        indexMutex_;  ///< Mutex for thread-safe access to indices
    AdvancedRecommendationEngine
        recommendationEngine_;  ///< Recommendation engine
};

// --------------------- SearchEngine Implementation ---------------------

SearchEngine::SearchEngine() : pImpl_(std::make_unique<Impl>()) {
    spdlog::info("SearchEngine instance created");
}

SearchEngine::~SearchEngine() {
    spdlog::info("SearchEngine instance destroyed");
}

bool SearchEngine::initializeRecommendationEngine(
    const std::string& modelFilename) {
    spdlog::info("Initializing Recommendation Engine with model file '{}'",
                 modelFilename);
    return pImpl_->initializeRecommendationEngine(modelFilename);
}

void SearchEngine::addStarObject(const StarObject& starObject) {
    spdlog::info("Request to add StarObject: {}", starObject.getName());
    pImpl_->addStarObject(starObject);
}

void SearchEngine::addUserRating(const std::string& user,
                                 const std::string& item, double rating) {
    spdlog::info("Request to add rating: User '{}', Item '{}', Rating {}", user,
                 item, rating);
    pImpl_->addUserRating(user, item, rating);
}

auto SearchEngine::searchStarObject(const std::string& query) const
    -> std::vector<StarObject> {
    spdlog::info("Request to search StarObject with query: {}", query);
    return pImpl_->searchStarObject(query);
}

auto SearchEngine::fuzzySearchStarObject(const std::string& query,
                                         int tolerance) const
    -> std::vector<StarObject> {
    spdlog::info(
        "Request to perform fuzzy search on StarObject with query: '{}' and "
        "tolerance: {}",
        query, tolerance);
    return pImpl_->fuzzySearchStarObject(query, tolerance);
}

auto SearchEngine::autoCompleteStarObject(const std::string& prefix) const
    -> std::vector<std::string> {
    spdlog::info("Request to auto-complete StarObject with prefix: {}", prefix);
    return pImpl_->autoCompleteStarObject(prefix);
}

std::vector<StarObject> SearchEngine::getRankedResults(
    std::vector<StarObject>& results) {
    spdlog::info("Request to rank search results");
    return Impl::getRankedResultsStatic(results);
}

bool SearchEngine::loadFromNameJson(const std::string& filename) {
    spdlog::info("Request to load StarObjects from name JSON file: {}",
                 filename);
    return pImpl_->loadFromNameJson(filename);
}

bool SearchEngine::loadFromCelestialJson(const std::string& filename) {
    spdlog::info("Request to load CelestialObjects from JSON file: {}",
                 filename);
    return pImpl_->loadFromCelestialJson(filename);
}

auto SearchEngine::filterSearch(const std::string& type,
                                const std::string& morphology,
                                double minMagnitude, double maxMagnitude) const
    -> std::vector<StarObject> {
    spdlog::info(
        "Request to perform filtered search with type: '{}', morphology: '{}', "
        "magnitude range: {}-{}",
        type, morphology, minMagnitude, maxMagnitude);
    return pImpl_->filterSearch(type, morphology, minMagnitude, maxMagnitude);
}

std::vector<std::pair<std::string, double>> SearchEngine::recommendItems(
    const std::string& user, int topN) const {
    spdlog::info("Request to recommend top {} items for user '{}'", topN, user);
    return pImpl_->recommendItems(user, topN);
}

bool SearchEngine::saveRecommendationModel(const std::string& filename) const {
    spdlog::info("Request to save Recommendation Engine model to '{}'",
                 filename);
    return pImpl_->saveRecommendationModel(filename);
}

bool SearchEngine::loadRecommendationModel(const std::string& filename) {
    spdlog::info("Request to load Recommendation Engine model from '{}'",
                 filename);
    return pImpl_->loadRecommendationModel(filename);
}

void SearchEngine::trainRecommendationEngine() {
    spdlog::info("Request to train Recommendation Engine");
    pImpl_->trainRecommendationEngine();
}

bool SearchEngine::loadFromCSV(const std::string& filename,
                               const std::vector<std::string>& requiredFields,
                               Dialect dialect) {
    return pImpl_->loadFromCSV(filename, requiredFields, dialect);
}

auto SearchEngine::getHybridRecommendations(const std::string& user, int topN,
                                            double contentWeight,
                                            double collaborativeWeight)
    -> std::vector<std::pair<std::string, double>> {
    return pImpl_->getHybridRecommendations(user, topN, contentWeight,
                                            collaborativeWeight);
}

bool SearchEngine::exportToCSV(const std::string& filename,
                               const std::vector<std::string>& fields,
                               Dialect dialect) const {
    return pImpl_->exportToCSV(filename, fields, dialect);
}

void SearchEngine::batchProcessRatings(const std::string& csvFilename) {
    spdlog::info("Starting batch processing of ratings from {}", csvFilename);
    try {
        std::ifstream file(csvFilename);
        if (!file.is_open()) {
            spdlog::error("Failed to open ratings file: {}", csvFilename);
            return;
        }

        std::string line;
        // Skip header line
        std::getline(file, line);

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string user, item, ratingStr;

            // Assume CSV format: user,item,rating
            std::getline(ss, user, ',');
            std::getline(ss, item, ',');
            std::getline(ss, ratingStr, ',');

            try {
                double rating = std::stod(ratingStr);
                addUserRating(user, item, rating);
                spdlog::debug("Processed rating: {} -> {} = {}", user, item,
                              rating);
            } catch (const std::exception& e) {
                spdlog::error("Error processing rating: {}", e.what());
                continue;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in batch processing ratings: {}", e.what());
    }
}

void SearchEngine::batchUpdateStarObjects(const std::string& csvFilename) {
    spdlog::info("Starting batch update of StarObjects from {}", csvFilename);
    try {
        std::ifstream file(csvFilename);
        if (!file.is_open()) {
            spdlog::error("Failed to open update file: {}", csvFilename);
            return;
        }

        std::string line;
        // Skip header line
        std::getline(file, line);

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::unordered_map<std::string, std::string> fields;
            std::string field;
            size_t fieldIndex = 0;

            while (std::getline(ss, field, ',')) {
                // Basic fields: name,aliases,type,magnitude
                switch (fieldIndex) {
                    case 0:
                        fields["name"] = field;
                        break;
                    case 1:
                        fields["aliases"] = field;
                        break;
                    case 2:
                        fields["type"] = field;
                        break;
                    case 3:
                        fields["magnitude"] = field;
                        break;
                    default:
                        break;
                }
                fieldIndex++;
            }

            try {
                // Parse aliases
                std::vector<std::string> aliases;
                std::stringstream aliasStream(fields["aliases"]);
                std::string alias;
                while (std::getline(aliasStream, alias, ';')) {
                    aliases.push_back(alias);
                }

                // Create or update StarObject
                StarObject star(fields["name"], aliases);

                // Set other properties
                if (!fields["type"].empty()) {
                    CelestialObject celestial = star.getCelestialObject();
                    celestial.Type = fields["type"];
                    if (!fields["magnitude"].empty()) {
                        celestial.VisualMagnitudeV =
                            std::stod(fields["magnitude"]);
                    }
                    star.setCelestialObject(celestial);
                }

                addStarObject(star);
                spdlog::info("Updated StarObject: {}", fields["name"]);
            } catch (const std::exception& e) {
                spdlog::error("Error updating StarObject: {}", e.what());
                continue;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in batch updating StarObjects: {}", e.what());
    }
}

void SearchEngine::clearCache() {
    spdlog::info("Clearing search engine cache");
    pImpl_->queryCache_.clear();
}

void SearchEngine::setCacheSize(size_t size) {
    spdlog::info("Setting cache size to {}", size);
    pImpl_->queryCache_.resize(size);
}

auto SearchEngine::getCacheStats() const -> std::string {
    const auto& cache = pImpl_->queryCache_;
    std::stringstream ss;
    ss << "Cache Statistics:\n"
       << "Size: " << cache.size() << "\n";

    spdlog::info("Retrieved cache statistics");
    return ss.str();
}

}  // namespace lithium::target
