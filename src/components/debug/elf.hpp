#ifndef LITHIUM_DEBUG_ELF_HPP
#define LITHIUM_DEBUG_ELF_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <future>
#include <memory>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Platform-specific headers
#ifdef _WIN32
// Windows-specific headers will be included in implementation
#define LITHIUM_OPTIMIZED_ELF_WINDOWS
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <execution>
#define LITHIUM_OPTIMIZED_ELF_UNIX
#ifdef __linux__
#define LITHIUM_OPTIMIZED_ELF_LINUX
#endif
#endif

#include "atom/utils/stopwatcher.hpp"

namespace lithium {

// Common ELF data structures
struct ElfHeader {
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct ProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

struct SectionHeader {
    std::string name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
};

struct Symbol {
    std::string name;
    uint64_t value;
    uint64_t size;
    unsigned char bind;
    unsigned char type;
    uint16_t shndx;
};

struct RelocationEntry {
    uint64_t offset;
    uint64_t info;
    int64_t addend;
};

struct DynamicEntry {
    uint64_t tag;
    union {
        uint64_t val;
        uint64_t ptr;
    } d_un;
};

namespace optimized {
class OptimizedElfParser;
}

class ElfParser {
public:
    explicit ElfParser(std::string_view file);
    ~ElfParser();
    ElfParser(ElfParser&&) noexcept = default;
    ElfParser& operator=(ElfParser&&) noexcept = default;
    ElfParser(const ElfParser&) = delete;
    ElfParser& operator=(const ElfParser&) = delete;

    auto parse() -> bool;
    [[nodiscard]] auto getElfHeader() const -> std::optional<ElfHeader>;
    [[nodiscard]] auto getProgramHeaders() const noexcept
        -> std::span<const ProgramHeader>;
    [[nodiscard]] auto getSectionHeaders() const noexcept
        -> std::span<const SectionHeader>;
    [[nodiscard]] auto getSymbolTable() const noexcept
        -> std::span<const Symbol>;
    [[nodiscard]] auto findSymbolByName(std::string_view name) const
        -> std::optional<Symbol>;
    [[nodiscard]] auto findSymbolByAddress(uint64_t address) const
        -> std::optional<Symbol>;
    [[nodiscard]] auto findSection(std::string_view name) const
        -> std::optional<SectionHeader>;
    [[nodiscard]] auto getSectionData(const SectionHeader& section) const
        -> std::vector<uint8_t>;
    [[nodiscard]] auto getSymbolsInRange(uint64_t start, uint64_t end) const
        -> std::vector<Symbol>;
    [[nodiscard]] auto getExecutableSegments() const
        -> std::vector<ProgramHeader>;
    [[nodiscard]] auto verifyIntegrity() const -> bool;
    void clearCache();
    [[nodiscard]] auto demangleSymbolName(const std::string& name) const
        -> std::string;
    [[nodiscard]] auto getSymbolVersion(const Symbol& symbol) const
        -> std::optional<std::string>;
    [[nodiscard]] auto getWeakSymbols() const -> std::vector<Symbol>;
    [[nodiscard]] auto getSymbolsByType(unsigned char type) const
        -> std::vector<Symbol>;
    [[nodiscard]] auto getExportedSymbols() const -> std::vector<Symbol>;
    [[nodiscard]] auto getImportedSymbols() const -> std::vector<Symbol>;
    [[nodiscard]] auto findSymbolsByPattern(const std::string& pattern) const
        -> std::vector<Symbol>;
    [[nodiscard]] auto getSectionsByType(uint32_t type) const
        -> std::vector<SectionHeader>;
    [[nodiscard]] auto getSegmentPermissions(const ProgramHeader& header) const
        -> std::string;
    [[nodiscard]] auto calculateChecksum() const -> uint64_t;
    [[nodiscard]] auto isStripped() const -> bool;
    [[nodiscard]] auto getDependencies() const -> std::vector<std::string>;
    void enableCache(bool enable);
    void setParallelProcessing(bool enable);
    void setCacheSize(size_t size);
    void preloadSymbols();
    [[nodiscard]] auto getRelocationEntries() const
        -> std::span<const RelocationEntry>;
    [[nodiscard]] auto getDynamicEntries() const
        -> std::span<const DynamicEntry>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

namespace optimized {

class OptimizedElfParser {
public:
    using SymbolCache = std::pmr::unordered_map<std::string, Symbol>;
    using AddressCache = std::pmr::unordered_map<uint64_t, Symbol>;
    using SectionCache =
        std::pmr::unordered_map<uint32_t, std::vector<SectionHeader>>;
    using MemoryPool = std::pmr::monotonic_buffer_resource;

    struct OptimizationConfig {
        bool enableParallelProcessing{true};
        bool enableMemoryMapping{true};
        bool enableSymbolCaching{true};
        bool enablePrefetching{true};
        size_t cacheSize{1024 * 1024};  // 1MB default
        size_t threadPoolSize{4};       // Default to 4 threads
        size_t chunkSize{4096};
    };

    struct PerformanceMetrics {
        std::atomic<uint64_t> parseTime{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> cacheMisses{0};
        std::atomic<uint64_t> memoryAllocations{0};
        std::atomic<size_t> peakMemoryUsage{0};
        std::atomic<uint32_t> threadsUsed{0};

        PerformanceMetrics(const PerformanceMetrics& other)
            : parseTime(other.parseTime.load()),
              cacheHits(other.cacheHits.load()),
              cacheMisses(other.cacheMisses.load()),
              memoryAllocations(other.memoryAllocations.load()),
              peakMemoryUsage(other.peakMemoryUsage.load()),
              threadsUsed(other.threadsUsed.load()) {}

        PerformanceMetrics() = default;

        PerformanceMetrics& operator=(const PerformanceMetrics& other) {
            if (this != &other) {
                parseTime.store(other.parseTime.load());
                cacheHits.store(other.cacheHits.load());
                cacheMisses.store(other.cacheMisses.load());
                memoryAllocations.store(other.memoryAllocations.load());
                peakMemoryUsage.store(other.peakMemoryUsage.load());
                threadsUsed.store(other.threadsUsed.load());
            }
            return *this;
        }
    };

    explicit OptimizedElfParser(std::string_view file,
                                const OptimizationConfig& config);
    explicit OptimizedElfParser(std::string_view file);
    ~OptimizedElfParser();

    OptimizedElfParser(const OptimizedElfParser&) = delete;
    OptimizedElfParser& operator=(const OptimizedElfParser&) = delete;
    OptimizedElfParser(OptimizedElfParser&&) noexcept;
    OptimizedElfParser& operator=(OptimizedElfParser&&) noexcept;

    [[nodiscard]] auto parse() -> bool;
    [[nodiscard]] auto parseAsync() -> std::future<bool>;
    [[nodiscard]] auto getElfHeader() const -> std::optional<ElfHeader>;
    [[nodiscard]] auto getProgramHeaders() const noexcept
        -> std::span<const ProgramHeader>;
    [[nodiscard]] auto getSectionHeaders() const noexcept
        -> std::span<const SectionHeader>;
    [[nodiscard]] auto getSymbolTable() const noexcept
        -> std::span<const Symbol>;
    [[nodiscard]] auto findSymbolByName(std::string_view name) const
        -> std::optional<Symbol>;
    [[nodiscard]] auto findSymbolByAddress(uint64_t address) const
        -> std::optional<Symbol>;

    template <std::predicate<const Symbol&> Predicate>
    [[nodiscard]] auto findSymbolsIf(Predicate&& pred) const
        -> std::vector<Symbol>;

    [[nodiscard]] auto getSymbolsInRange(uint64_t start, uint64_t end) const
        -> std::vector<Symbol>;
    [[nodiscard]] auto getSectionsByType(uint32_t type) const
        -> std::vector<SectionHeader>;
    [[nodiscard]] auto batchFindSymbols(const std::vector<std::string>& names)
        const -> std::vector<std::optional<Symbol>>;
    void prefetchData(const std::vector<uint64_t>& addresses) const;
    [[nodiscard]] auto getMetrics() const -> PerformanceMetrics;
    void resetMetrics();
    void optimizeMemoryLayout();
    void updateConfig(const OptimizationConfig& config);
    [[nodiscard]] auto validateIntegrity() const -> bool;
    [[nodiscard]] auto getMemoryUsage() const -> size_t;
    [[nodiscard]] auto exportSymbols(std::string_view format) const
        -> std::string;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    mutable PerformanceMetrics metrics_;
    OptimizationConfig config_;

    std::unique_ptr<MemoryPool> memoryPool_;
    std::unique_ptr<atom::utils::StopWatcher> performanceTimer_;

    std::unique_ptr<std::pmr::monotonic_buffer_resource> bufferResource_;
    mutable std::unique_ptr<SymbolCache> symbolCache_;
    mutable std::unique_ptr<AddressCache> addressCache_;
    mutable std::unique_ptr<SectionCache> sectionCache_;

    void initializeOptimizations();
    void setupMemoryPools();
    void warmupCaches();

    template <typename Container, typename Func>
    auto parallelTransform(const Container& input, Func&& func) const;

    template <typename Range>
    auto optimizedSearch(const Range& range, auto&& predicate) const;
};

template <std::predicate<const Symbol&> Predicate>
auto OptimizedElfParser::findSymbolsIf(Predicate&& pred) const
    -> std::vector<Symbol> {
    const auto symbols = getSymbolTable();
    std::vector<Symbol> result;

    if (config_.enableParallelProcessing && symbols.size() > 1000) {
        std::ranges::copy_if(symbols, std::back_inserter(result),
                             std::forward<Predicate>(pred));
    } else {
        std::ranges::copy_if(symbols, std::back_inserter(result),
                             std::forward<Predicate>(pred));
    }

    return result;
}

template <typename Container, typename Func>
auto OptimizedElfParser::parallelTransform(const Container& input,
                                           Func&& func) const {
    if constexpr (requires { std::execution::par_unseq; }) {
        if (config_.enableParallelProcessing &&
            input.size() > config_.chunkSize) {
            std::vector<
                std::invoke_result_t<Func, typename Container::value_type>>
                result;
            result.resize(input.size());

            std::transform(std::execution::par_unseq, input.begin(),
                           input.end(), result.begin(),
                           std::forward<Func>(func));
            return result;
        }
    }

    std::vector<std::invoke_result_t<Func, typename Container::value_type>>
        result;
    std::ranges::transform(input, std::back_inserter(result),
                           std::forward<Func>(func));
    return result;
}

template <typename Range>
auto OptimizedElfParser::optimizedSearch(const Range& range,
                                         auto&& predicate) const {
    if constexpr (std::ranges::random_access_range<Range>) {
        if (std::ranges::is_sorted(range)) {
            return std::ranges::lower_bound(range, predicate);
        }
    }

    if (config_.enableParallelProcessing &&
        std::ranges::size(range) > config_.chunkSize) {
        return std::find_if(std::execution::par_unseq,
                            std::ranges::begin(range), std::ranges::end(range),
                            predicate);
    }

    return std::ranges::find_if(range, predicate);
}

template <typename Derived>
class ElfParserRAII {
public:
    ElfParserRAII() = default;
    virtual ~ElfParserRAII() = default;

    auto parse() -> bool { return static_cast<Derived*>(this)->parseImpl(); }

    auto getMetrics() const {
        return static_cast<const Derived*>(this)->getMetricsImpl();
    }
};

class ConstexprSymbolFinder {
public:
    template <size_t N>
    constexpr static auto findSymbolIndex(const std::array<Symbol, N>& symbols,
                                          std::string_view name)
        -> std::optional<size_t> {
        for (size_t i = 0; i < N; ++i) {
            if (symbols[i].name == name) {
                return i;
            }
        }
        return std::nullopt;
    }

    template <typename T>
    constexpr static auto isValidElfType(T type) -> bool {
        return type >= 0 && type <= 0xFFFF;
    }
};

}  // namespace optimized

#ifdef __linux__
class EnhancedElfParser {
public:
    explicit EnhancedElfParser(std::string_view file, bool useOptimized = true);

    auto parse() -> bool;
    auto getElfHeader() const -> std::optional<ElfHeader>;
    auto findSymbolByName(std::string_view name) const -> std::optional<Symbol>;
    auto getSymbolTable() const -> std::span<const Symbol>;
    auto comparePerformance() -> void;

private:
    std::string filePath_;
    bool useOptimized_;
    std::unique_ptr<optimized::OptimizedElfParser> optimizedParser_;
    std::unique_ptr<ElfParser> standardParser_;
};

auto createElfParser(std::string_view file, bool preferOptimized = true)
    -> std::unique_ptr<EnhancedElfParser>;
auto migrateToEnhancedParser(const ElfParser& oldParser,
                             std::string_view filePath)
    -> std::unique_ptr<EnhancedElfParser>;
#endif

}  // namespace lithium

#endif  // LITHIUM_DEBUG_ELF_HPP