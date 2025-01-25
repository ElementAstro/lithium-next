#include <gtest/gtest.h>
#include <filesystem>
#include <random>
#include <thread>
#include "target/preference.hpp"

namespace lithium::target::test {

class AdvancedRecommendationEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<AdvancedRecommendationEngine>();

        // Add some test data
        testUsers = {"user1", "user2", "user3"};
        testItems = {"item1", "item2", "item3"};
        testFeatures = {"feature1", "feature2", "feature3"};

        // Add test ratings
        for (const auto& user : testUsers) {
            for (const auto& item : testItems) {
                engine->addRating(user, item, generateRandomRating());
            }
        }

        // Add test items with features
        for (const auto& item : testItems) {
            engine->addItem(item, testFeatures);
        }
    }

    void TearDown() override {
        engine->clear();
        engine.reset();
        if (std::filesystem::exists("test_model.bin")) {
            std::filesystem::remove("test_model.bin");
        }
    }

    double generateRandomRating() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(1.0, 5.0);
        return dis(gen);
    }

    std::unique_ptr<AdvancedRecommendationEngine> engine;
    std::vector<std::string> testUsers;
    std::vector<std::string> testItems;
    std::vector<std::string> testFeatures;
};

// Basic functionality tests
TEST_F(AdvancedRecommendationEngineTest, AddRating) {
    ASSERT_NO_THROW(engine->addRating("newuser", "newitem", 4.5));
    EXPECT_THROW(engine->addRating("newuser", "newitem", 6.0), DataException);
    EXPECT_THROW(engine->addRating("newuser", "newitem", -1.0), DataException);
}

TEST_F(AdvancedRecommendationEngineTest, AddItem) {
    std::vector<std::string> features = {"feature1", "feature2"};
    ASSERT_NO_THROW(engine->addItem("newitem", features));
    ASSERT_NO_THROW(engine->addItemFeature("newitem", "feature3", 0.5));
    EXPECT_THROW(engine->addItemFeature("newitem", "feature3", 1.5),
                 DataException);
}

TEST_F(AdvancedRecommendationEngineTest, BatchOperations) {
    std::vector<std::tuple<std::string, std::string, double>> ratings = {
        {"user4", "item4", 4.0}, {"user5", "item5", 3.5}};
    ASSERT_NO_THROW(engine->addRatings(ratings));
}

// Training and prediction tests
TEST_F(AdvancedRecommendationEngineTest, TrainingAndPrediction) {
    ASSERT_NO_THROW(engine->train());

    auto prediction = engine->predictRating(testUsers[0], testItems[0]);
    EXPECT_GE(prediction, 0.0);
    EXPECT_LE(prediction, 5.0);

    auto recommendations = engine->recommendItems(testUsers[0], 2);
    EXPECT_EQ(recommendations.size(), 2);
    for (const auto& rec : recommendations) {
        EXPECT_GE(rec.second, 0.0);
    }
}

// Model persistence tests
TEST_F(AdvancedRecommendationEngineTest, ModelPersistence) {
    engine->train();
    ASSERT_NO_THROW(engine->saveModel("test_model.bin"));

    auto newEngine = std::make_unique<AdvancedRecommendationEngine>();
    ASSERT_NO_THROW(newEngine->loadModel("test_model.bin"));

    auto pred1 = engine->predictRating(testUsers[0], testItems[0]);
    auto pred2 = newEngine->predictRating(testUsers[0], testItems[0]);
    EXPECT_NEAR(pred1, pred2, 1e-5);
}

// Thread safety tests
TEST_F(AdvancedRecommendationEngineTest, ThreadSafety) {
    constexpr int NUM_THREADS = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            std::string user = "thread_user" + std::to_string(i);
            std::string item = "thread_item" + std::to_string(i);
            ASSERT_NO_THROW(engine->addRating(user, item, 4.0));
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

// Cache tests
TEST_F(AdvancedRecommendationEngineTest, CacheManagement) {
    engine->train();
    auto rec1 = engine->recommendItems(testUsers[0], 5);
    auto rec2 = engine->recommendItems(testUsers[0], 5);

    EXPECT_EQ(rec1.size(), rec2.size());
    for (size_t i = 0; i < rec1.size(); ++i) {
        EXPECT_EQ(rec1[i].first, rec2[i].first);
        EXPECT_DOUBLE_EQ(rec1[i].second, rec2[i].second);
    }
}

// Error handling tests
TEST_F(AdvancedRecommendationEngineTest, ErrorHandling) {
    EXPECT_THROW(engine->addRating("", "item1", 4.0), DataException);
    EXPECT_THROW(engine->addRating("user1", "", 4.0), DataException);
    EXPECT_THROW(engine->loadModel("nonexistent.bin"), ModelException);
}

// Optimization tests
TEST_F(AdvancedRecommendationEngineTest, Optimization) {
    engine->train();
    ASSERT_NO_THROW(engine->optimize());
    auto stats = engine->getStats();
    EXPECT_FALSE(stats.empty());
}

// Feature similarity tests
TEST_F(AdvancedRecommendationEngineTest, FeatureSimilarity) {
    engine->addItemFeature(testItems[0], "common_feature", 1.0);
    engine->addItemFeature(testItems[1], "common_feature", 1.0);

    engine->train();
    auto recommendations = engine->recommendItems(testUsers[0], 5);
    EXPECT_FALSE(recommendations.empty());
}

// Clear and reset tests
TEST_F(AdvancedRecommendationEngineTest, ClearAndReset) {
    ASSERT_NO_THROW(engine->clear());
    auto stats = engine->getStats();
    EXPECT_EQ(stats.find("Users: 0"), 0);
    EXPECT_EQ(stats.find("Items: 0"), 0);
}

}  // namespace lithium::target::test
