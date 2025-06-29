#ifdef __linux__

#include <iostream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <elf.h>
#include "../src/components/debug/optimized_elf.hpp"

using namespace lithium::optimized;

void demonstrateBasicUsage() {
    std::cout << "\n=== Basic OptimizedElfParser Usage ===" << std::endl;
    
    // Create parser with default balanced configuration
    auto parser = OptimizedElfParser("/usr/bin/ls");
    
    if (parser.parse()) {
        std::cout << "✓ Successfully parsed ELF file" << std::endl;
        
        // Get basic information
        if (auto header = parser.getElfHeader()) {
            std::cout << "ELF Type: " << header->type << std::endl;
            std::cout << "Machine: " << header->machine << std::endl;
            std::cout << "Entry Point: 0x" << std::hex << header->entry << std::dec << std::endl;
        }
        
        // Get symbol statistics
        auto symbols = parser.getSymbolTable();
        std::cout << "Total Symbols: " << symbols.size() << std::endl;
        
        // Find a specific symbol
        if (auto symbol = parser.findSymbolByName("main")) {
            std::cout << "Found 'main' symbol at address: 0x" 
                      << std::hex << symbol->value << std::dec << std::endl;
        }
        
    } else {
        std::cout << "✗ Failed to parse ELF file" << std::endl;
    }
}

void demonstratePerformanceProfiles() {
    std::cout << "\n=== Performance Profile Comparison ===" << std::endl;
    
    const std::string testFile = "/usr/bin/ls";
    
    // Test different performance profiles
    std::vector<std::pair<OptimizedElfParserFactory::PerformanceProfile, std::string>> profiles = {
        {OptimizedElfParserFactory::PerformanceProfile::Memory, "Memory Optimized"},
        {OptimizedElfParserFactory::PerformanceProfile::Speed, "Speed Optimized"},
        {OptimizedElfParserFactory::PerformanceProfile::Balanced, "Balanced"},
        {OptimizedElfParserFactory::PerformanceProfile::LowLatency, "Low Latency"}
    };
    
    for (const auto& [profile, name] : profiles) {
        auto parser = OptimizedElfParserFactory::create(testFile, profile);
        
        auto start = std::chrono::high_resolution_clock::now();
        bool success = parser->parse();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << name << ": ";
        if (success) {
            std::cout << "✓ " << duration.count() << "μs";
            std::cout << " (Memory: " << parser->getMemoryUsage() / 1024 << "KB)";
        } else {
            std::cout << "✗ Failed";
        }
        std::cout << std::endl;
    }
}

void demonstrateAdvancedFeatures() {
    std::cout << "\n=== Advanced Features Demonstration ===" << std::endl;
    
    // Create parser with custom configuration
    OptimizedElfParser::OptimizationConfig config;
    config.enableParallelProcessing = true;
    config.enableSymbolCaching = true;
    config.enablePrefetching = true;
    config.cacheSize = 2 * 1024 * 1024; // 2MB cache
    
    auto parser = OptimizedElfParser("/usr/bin/ls", config);
    
    if (parser.parse()) {
        std::cout << "✓ Parser initialized with custom configuration" << std::endl;
        
        // Demonstrate batch symbol lookup
        std::vector<std::string> symbolNames = {"main", "printf", "malloc", "free", "exit"};
        auto results = parser.batchFindSymbols(symbolNames);
        
        std::cout << "\nBatch Symbol Lookup Results:" << std::endl;
        for (size_t i = 0; i < symbolNames.size(); ++i) {
            std::cout << "  " << symbolNames[i] << ": ";
            if (results[i]) {
                std::cout << "Found at 0x" << std::hex << results[i]->value << std::dec;
            } else {
                std::cout << "Not found";
            }
            std::cout << std::endl;
        }
        
        // Demonstrate range-based symbol search
        auto rangeSymbols = parser.getSymbolsInRange(0x1000, 0x2000);
        std::cout << "\nSymbols in range [0x1000, 0x2000): " << rangeSymbols.size() << std::endl;
        
        // Demonstrate template-based symbol filtering
        auto functionSymbols = parser.findSymbolsIf([](const lithium::Symbol& sym) {
            return sym.type == STT_FUNC && sym.size > 0;
        });
        std::cout << "Function symbols found: " << functionSymbols.size() << std::endl;
        
        // Get performance metrics
        auto metrics = parser.getMetrics();
        std::cout << "\nPerformance Metrics:" << std::endl;
        std::cout << "  Parse Time: " << metrics.parseTime.load() << "ns" << std::endl;
        std::cout << "  Cache Hits: " << metrics.cacheHits.load() << std::endl;
        std::cout << "  Cache Misses: " << metrics.cacheMisses.load() << std::endl;
        
        if (metrics.cacheHits.load() + metrics.cacheMisses.load() > 0) {
            double hitRate = static_cast<double>(metrics.cacheHits.load()) / 
                           (metrics.cacheHits.load() + metrics.cacheMisses.load()) * 100.0;
            std::cout << "  Cache Hit Rate: " << std::fixed << std::setprecision(2) 
                      << hitRate << "%" << std::endl;
        }
        
        // Optimize memory layout
        parser.optimizeMemoryLayout();
        std::cout << "\n✓ Memory layout optimized for better cache performance" << std::endl;
        
        // Validate integrity
        if (parser.validateIntegrity()) {
            std::cout << "✓ ELF file integrity validated successfully" << std::endl;
        }
        
        // Export symbols to JSON
        auto jsonExport = parser.exportSymbols("json");
        std::cout << "\n✓ Exported " << parser.getSymbolTable().size() 
                  << " symbols to JSON format (" << jsonExport.length() << " characters)" << std::endl;
    }
}

void demonstrateAsyncParsing() {
    std::cout << "\n=== Asynchronous Parsing Demonstration ===" << std::endl;
    
    auto parser = OptimizedElfParserFactory::create("/usr/bin/ls", 
        OptimizedElfParserFactory::PerformanceProfile::Speed);
    
    std::cout << "Starting asynchronous parsing..." << std::endl;
    auto future = parser->parseAsync();
    
    // Simulate other work being done
    std::cout << "Performing other work while parsing..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Wait for parsing to complete
    if (future.get()) {
        std::cout << "✓ Asynchronous parsing completed successfully" << std::endl;
        
        auto symbols = parser->getSymbolTable();
        std::cout << "Parsed " << symbols.size() << " symbols asynchronously" << std::endl;
    } else {
        std::cout << "✗ Asynchronous parsing failed" << std::endl;
    }
}

void demonstrateMemoryManagement() {
    std::cout << "\n=== Memory Management Demonstration ===" << std::endl;
    
    // Test memory usage with different configurations
    std::vector<std::pair<std::string, OptimizedElfParser::OptimizationConfig>> configs = {
        {"Minimal Memory", {
            .enableParallelProcessing = false,
            .enableMemoryMapping = true,
            .enableSymbolCaching = false,
            .enablePrefetching = false,
            .cacheSize = 64 * 1024
        }},
        {"High Performance", {
            .enableParallelProcessing = true,
            .enableMemoryMapping = true,
            .enableSymbolCaching = true,
            .enablePrefetching = true,
            .cacheSize = 4 * 1024 * 1024
        }}
    };
    
    for (const auto& [name, config] : configs) {
        auto parser = OptimizedElfParser("/usr/bin/ls", config);
        
        size_t memoryBefore = parser.getMemoryUsage();
        bool parseResult = parser.parse();
        size_t memoryAfter = parser.getMemoryUsage();
        
        std::cout << name << ":" << std::endl;
        std::cout << "  Memory before parsing: " << memoryBefore / 1024 << "KB" << std::endl;
        std::cout << "  Memory after parsing: " << memoryAfter / 1024 << "KB" << std::endl;
        std::cout << "  Memory increase: " << (memoryAfter - memoryBefore) / 1024 << "KB" << std::endl;
    }
}

void demonstrateConstexprFeatures() {
    std::cout << "\n=== Compile-time Features Demonstration ===" << std::endl;
    
    // Demonstrate constexpr validation
    constexpr bool validType = ConstexprSymbolFinder::isValidElfType(ET_EXEC);
    constexpr bool invalidType = ConstexprSymbolFinder::isValidElfType(-1);
    
    std::cout << "Constexpr type validation:" << std::endl;
    std::cout << "  ET_EXEC is valid: " << (validType ? "yes" : "no") << std::endl;
    std::cout << "  -1 is valid: " << (invalidType ? "yes" : "no") << std::endl;
    
    // Note: Symbol-based constexpr operations are limited due to std::string members
    std::cout << "Note: Symbol lookup is optimized at runtime due to std::string usage" << std::endl;
}

int main() {
    std::cout << "OptimizedElfParser Comprehensive Example" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    try {
        demonstrateBasicUsage();
        demonstratePerformanceProfiles();
        demonstrateAdvancedFeatures();
        demonstrateAsyncParsing();
        demonstrateMemoryManagement();
        demonstrateConstexprFeatures();
        
        std::cout << "\n✓ All demonstrations completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

#endif  // __linux__
