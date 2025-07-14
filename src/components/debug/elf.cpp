#ifdef __linux__

#include "elf.hpp"

#include <cxxabi.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <future>
#include <memory_resource>
#include <regex>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace lithium {

class ElfParser::Impl {
public:
    explicit Impl(std::string_view file) : filePath_(file) {
        spdlog::info("ElfParser::Impl created for file: {}", file);
    }

    auto parse() -> bool {
        spdlog::info("Parsing ELF file: {}", filePath_);
        std::ifstream file(filePath_, std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open file: {}", filePath_);
            return false;
        }

        file.seekg(0, std::ios::end);
        fileSize_ = file.tellg();
        file.seekg(0, std::ios::beg);

        fileContent_.resize(fileSize_);
        file.read(reinterpret_cast<char*>(fileContent_.data()), fileSize_);

        bool result = parseElfHeader() && parseProgramHeaders() &&
                      parseSectionHeaders() && parseSymbolTable() &&
                      parseDynamicEntries() && parseRelocationEntries();
        if (result) {
            spdlog::info("Successfully parsed ELF file: {}", filePath_);
        } else {
            spdlog::error("Failed to parse ELF file: {}", filePath_);
        }
        return result;
    }

    [[nodiscard]] auto getElfHeader() const -> std::optional<ElfHeader> {
        spdlog::info("Getting ELF header");
        return elfHeader_;
    }

    [[nodiscard]] auto getProgramHeaders() const
        -> std::span<const ProgramHeader> {
        spdlog::info("Getting program headers");
        return programHeaders_;
    }

    [[nodiscard]] auto getSectionHeaders() const
        -> std::span<const SectionHeader> {
        spdlog::info("Getting section headers");
        return sectionHeaders_;
    }

    [[nodiscard]] auto getSymbolTable() const -> std::span<const Symbol> {
        spdlog::info("Getting symbol table");
        return symbolTable_;
    }

    [[nodiscard]] auto findSymbolByName(std::string_view name) const
        -> std::optional<Symbol> {
        spdlog::info("Finding symbol by name: {}", name);
        auto cachedSymbol = findSymbolInCache(name);
        if (cachedSymbol) {
            return cachedSymbol;
        }
        auto it = std::ranges::find_if(
            symbolTable_,
            [name](const auto& symbol) { return symbol.name == name; });
        if (it != symbolTable_.end()) {
            spdlog::info("Found symbol: {}", name);
            symbolCache_[std::string(name)] = *it;
            return *it;
        }
        spdlog::warn("Symbol not found: {}", name);
        return std::nullopt;
    }

    [[nodiscard]] auto findSymbolByAddress(uint64_t address) const
        -> std::optional<Symbol> {
        spdlog::info("Finding symbol by address: {}", address);
        auto cachedSymbol = findSymbolByAddressInCache(address);
        if (cachedSymbol) {
            return cachedSymbol;
        }
        auto it = std::ranges::find_if(
            symbolTable_,
            [address](const auto& symbol) { return symbol.value == address; });
        if (it != symbolTable_.end()) {
            spdlog::info("Found symbol at address: {}", address);
            addressCache_[address] = *it;
            return *it;
        }
        spdlog::warn("Symbol not found at address: {}", address);
        return std::nullopt;
    }

    [[nodiscard]] auto findSection(std::string_view name) const
        -> std::optional<SectionHeader> {
        spdlog::info("Finding section by name: {}", name);
        auto it = std::ranges::find_if(
            sectionHeaders_,
            [name](const auto& section) { return section.name == name; });
        if (it != sectionHeaders_.end()) {
            spdlog::info("Found section: {}", name);
            return *it;
        }
        spdlog::warn("Section not found: {}", name);
        return std::nullopt;
    }

    [[nodiscard]] auto getSectionData(const SectionHeader& section) const
        -> std::vector<uint8_t> {
        spdlog::info("Getting data for section: {}", section.name);
        if (section.offset + section.size > fileSize_) {
            spdlog::error("Section data out of bounds: {}", section.name);
            THROW_OUT_OF_RANGE("Section data out of bounds");
        }
        return {fileContent_.begin() + section.offset,
                fileContent_.begin() + section.offset + section.size};
    }

    [[nodiscard]] auto getSymbolsInRange(uint64_t start, uint64_t end) const
        -> std::vector<Symbol> {
        spdlog::info("Getting symbols in range: [{:x}, {:x}]", start, end);
        std::vector<Symbol> result;
        for (const auto& symbol : symbolTable_) {
            if (symbol.value >= start && symbol.value < end) {
                result.push_back(symbol);
            }
        }
        return result;
    }

    [[nodiscard]] auto getExecutableSegments() const
        -> std::vector<ProgramHeader> {
        spdlog::info("Getting executable segments");
        std::vector<ProgramHeader> result;
        for (const auto& ph : programHeaders_) {
            if (ph.flags & PF_X) {
                result.push_back(ph);
            }
        }
        return result;
    }

    [[nodiscard]] auto verifyIntegrity() const -> bool {
        if (verified_) {
            return true;
        }

        spdlog::info("Verifying ELF file integrity");

        const auto* ident =
            reinterpret_cast<const unsigned char*>(fileContent_.data());
        if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
            ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
            spdlog::error("Invalid ELF magic number");
            return false;
        }

        if (!elfHeader_) {
            spdlog::error("Missing ELF header");
            return false;
        }

        const auto totalSize =
            elfHeader_->shoff + (elfHeader_->shnum * elfHeader_->shentsize);
        if (totalSize > fileSize_) {
            spdlog::error("File size too small for section headers");
            return false;
        }

        verified_ = true;
        return true;
    }

    void clearCache() {
        spdlog::info("Clearing parser cache");
        symbolCache_.clear();
        addressCache_.clear();
        relocationEntries_.clear();
        dynamicEntries_.clear();
        verified_ = false;
    }

    std::string filePath_;
    std::vector<uint8_t> fileContent_;
    size_t fileSize_{};

    std::optional<ElfHeader> elfHeader_;
    std::vector<ProgramHeader> programHeaders_;
    std::vector<SectionHeader> sectionHeaders_;
    std::vector<Symbol> symbolTable_;
    std::vector<RelocationEntry> relocationEntries_;
    std::vector<DynamicEntry> dynamicEntries_;

    mutable std::unordered_map<std::string, Symbol> symbolCache_;
    mutable std::unordered_map<uint64_t, Symbol> addressCache_;
    mutable bool verified_{false};
    mutable std::unordered_map<uint32_t, std::vector<SectionHeader>>
        sectionTypeCache_;
    bool useParallelProcessing_ = false;
    size_t maxCacheSize_ = 1000;
    mutable std::unordered_map<std::string, std::string> demangledNameCache_;

    auto parseElfHeader() -> bool {
        spdlog::info("Parsing ELF header");
        if (fileSize_ < sizeof(Elf64_Ehdr)) {
            spdlog::error("File size too small for ELF header");
            return false;
        }

        const auto* ehdr =
            reinterpret_cast<const Elf64_Ehdr*>(fileContent_.data());
        elfHeader_ = ElfHeader{.type = ehdr->e_type,
                               .machine = ehdr->e_machine,
                               .version = ehdr->e_version,
                               .entry = ehdr->e_entry,
                               .phoff = ehdr->e_phoff,
                               .shoff = ehdr->e_shoff,
                               .flags = ehdr->e_flags,
                               .ehsize = ehdr->e_ehsize,
                               .phentsize = ehdr->e_phentsize,
                               .phnum = ehdr->e_phnum,
                               .shentsize = ehdr->e_shentsize,
                               .shnum = ehdr->e_shnum,
                               .shstrndx = ehdr->e_shstrndx};

        spdlog::info("Parsed ELF header successfully");
        return true;
    }

    auto parseProgramHeaders() -> bool {
        spdlog::info("Parsing program headers");
        if (!elfHeader_) {
            spdlog::error("ELF header not parsed");
            return false;
        }

        const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(
            fileContent_.data() + elfHeader_->phoff);
        for (uint16_t i = 0; i < elfHeader_->phnum; ++i) {
            programHeaders_.push_back(ProgramHeader{.type = phdr[i].p_type,
                                                    .offset = phdr[i].p_offset,
                                                    .vaddr = phdr[i].p_vaddr,
                                                    .paddr = phdr[i].p_paddr,
                                                    .filesz = phdr[i].p_filesz,
                                                    .memsz = phdr[i].p_memsz,
                                                    .flags = phdr[i].p_flags,
                                                    .align = phdr[i].p_align});
        }

        spdlog::info("Parsed program headers successfully");
        return true;
    }

    auto parseSectionHeaders() -> bool {
        spdlog::info("Parsing section headers");
        if (!elfHeader_) {
            spdlog::error("ELF header not parsed");
            return false;
        }

        const auto* shdr = reinterpret_cast<const Elf64_Shdr*>(
            fileContent_.data() + elfHeader_->shoff);
        const auto* strtab = reinterpret_cast<const char*>(
            fileContent_.data() + shdr[elfHeader_->shstrndx].sh_offset);

        for (uint16_t i = 0; i < elfHeader_->shnum; ++i) {
            sectionHeaders_.push_back(
                SectionHeader{.name = std::string(strtab + shdr[i].sh_name),
                              .type = shdr[i].sh_type,
                              .flags = shdr[i].sh_flags,
                              .addr = shdr[i].sh_addr,
                              .offset = shdr[i].sh_offset,
                              .size = shdr[i].sh_size,
                              .link = shdr[i].sh_link,
                              .info = shdr[i].sh_info,
                              .addralign = shdr[i].sh_addralign,
                              .entsize = shdr[i].sh_entsize});
        }

        spdlog::info("Parsed section headers successfully");
        return true;
    }

    auto parseSymbolTable() -> bool {
        spdlog::info("Parsing symbol table");
        auto symtabSection = std::ranges::find_if(
            sectionHeaders_,
            [](const auto& section) { return section.type == SHT_SYMTAB; });

        if (symtabSection == sectionHeaders_.end()) {
            spdlog::warn("No symbol table found");
            return true;  // No symbol table, but not an error
        }

        const auto* symtab = reinterpret_cast<const Elf64_Sym*>(
            fileContent_.data() + symtabSection->offset);
        size_t numSymbols = symtabSection->size / sizeof(Elf64_Sym);

        const auto* strtab = reinterpret_cast<const char*>(
            fileContent_.data() + sectionHeaders_[symtabSection->link].offset);

        for (size_t i = 0; i < numSymbols; ++i) {
            symbolTable_.push_back(
                Symbol{.name = std::string(strtab + symtab[i].st_name),
                       .value = symtab[i].st_value,
                       .size = symtab[i].st_size,
                       .bind = static_cast<unsigned char>(
                           ELF64_ST_BIND(symtab[i].st_info)),
                       .type = static_cast<unsigned char>(
                           ELF64_ST_TYPE(symtab[i].st_info)),
                       .shndx = symtab[i].st_shndx});
        }

        spdlog::info("Parsed symbol table successfully");
        return true;
    }

    auto parseDynamicEntries() -> bool {
        spdlog::info("Parsing dynamic entries");
        auto dynamicSection = std::ranges::find_if(
            sectionHeaders_,
            [](const auto& section) { return section.type == SHT_DYNAMIC; });

        if (dynamicSection == sectionHeaders_.end()) {
            spdlog::info("No dynamic section found");
            return true;  // Not an error, just no dynamic entries
        }

        const auto* dyn = reinterpret_cast<const Elf64_Dyn*>(
            fileContent_.data() + dynamicSection->offset);
        size_t numEntries = dynamicSection->size / sizeof(Elf64_Dyn);

        for (size_t i = 0; i < numEntries && dyn[i].d_tag != DT_NULL; ++i) {
            dynamicEntries_.push_back(
                DynamicEntry{.tag = static_cast<uint64_t>(dyn[i].d_tag),
                             .d_un = {.val = dyn[i].d_un.d_val}});
        }

        spdlog::info("Parsed {} dynamic entries", dynamicEntries_.size());
        return true;
    }

    auto parseRelocationEntries() -> bool {
        spdlog::info("Parsing relocation entries");
        std::vector<SectionHeader> relaSections;

        for (const auto& section : sectionHeaders_) {
            if (section.type == SHT_RELA) {
                relaSections.push_back(section);
            }
        }

        for (const auto& relaSection : relaSections) {
            const auto* rela = reinterpret_cast<const Elf64_Rela*>(
                fileContent_.data() + relaSection.offset);
            size_t numEntries = relaSection.size / sizeof(Elf64_Rela);

            for (size_t i = 0; i < numEntries; ++i) {
                relocationEntries_.push_back(
                    RelocationEntry{.offset = rela[i].r_offset,
                                    .info = rela[i].r_info,
                                    .addend = rela[i].r_addend});
            }
        }

        spdlog::info("Parsed {} relocation entries", relocationEntries_.size());
        return true;
    }

    auto findSymbolInCache(std::string_view name) const
        -> std::optional<Symbol> {
        auto it = symbolCache_.find(std::string(name));
        if (it != symbolCache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    auto findSymbolByAddressInCache(uint64_t address) const
        -> std::optional<Symbol> {
        auto it = addressCache_.find(address);
        if (it != addressCache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

ElfParser::ElfParser(std::string_view file)
    : pImpl_(std::make_unique<Impl>(file)) {
    spdlog::info("ElfParser created for file: {}", file);
}

ElfParser::~ElfParser() = default;

auto ElfParser::parse() -> bool {
    spdlog::info("ElfParser::parse called");
    return pImpl_->parse();
}

auto ElfParser::getElfHeader() const -> std::optional<ElfHeader> {
    spdlog::info("ElfParser::getElfHeader called");
    return pImpl_->getElfHeader();
}

auto ElfParser::getProgramHeaders() const noexcept -> std::span<const ProgramHeader> {
    spdlog::info("ElfParser::getProgramHeaders called");
    return pImpl_->getProgramHeaders();
}

auto ElfParser::getSectionHeaders() const noexcept -> std::span<const SectionHeader> {
    spdlog::info("ElfParser::getSectionHeaders called");
    return pImpl_->getSectionHeaders();
}

auto ElfParser::getSymbolTable() const noexcept -> std::span<const Symbol> {
    spdlog::info("ElfParser::getSymbolTable called");
    return pImpl_->getSymbolTable();
}

auto ElfParser::findSymbolByName(std::string_view name) const
    -> std::optional<Symbol> {
    spdlog::info("ElfParser::findSymbolByName called with name: {}", name);
    return pImpl_->findSymbolByName(name);
}

auto ElfParser::findSymbolByAddress(uint64_t address) const
    -> std::optional<Symbol> {
    spdlog::info("ElfParser::findSymbolByAddress called with address: {}",
                 address);
    return pImpl_->findSymbolByAddress(address);
}

auto ElfParser::findSection(std::string_view name) const
    -> std::optional<SectionHeader> {
    spdlog::info("ElfParser::findSection called with name: {}", name);
    return pImpl_->findSection(name);
}

auto ElfParser::getSectionData(const SectionHeader& section) const
    -> std::vector<uint8_t> {
    spdlog::info("ElfParser::getSectionData called for section: {}",
                 section.name);
    return pImpl_->getSectionData(section);
}

auto ElfParser::getSymbolsInRange(uint64_t start, uint64_t end) const
    -> std::vector<Symbol> {
    spdlog::info("ElfParser::getSymbolsInRange called");
    return pImpl_->getSymbolsInRange(start, end);
}

auto ElfParser::getExecutableSegments() const -> std::vector<ProgramHeader> {
    spdlog::info("ElfParser::getExecutableSegments called");
    return pImpl_->getExecutableSegments();
}

auto ElfParser::verifyIntegrity() const -> bool {
    spdlog::info("ElfParser::verifyIntegrity called");
    return pImpl_->verifyIntegrity();
}

void ElfParser::clearCache() {
    spdlog::info("ElfParser::clearCache called");
    pImpl_->clearCache();
}

auto ElfParser::demangleSymbolName(const std::string& name) const
    -> std::string {
    int status;
    char* demangled =
        abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    return name;
}

auto ElfParser::getSymbolVersion(const Symbol& symbol) const
    -> std::optional<std::string> {
    auto verdefSection = pImpl_->findSection(".gnu.version_d");
    auto vernumSection = pImpl_->findSection(".gnu.version");
    auto dynsymSection = pImpl_->findSection(".dynsym");

    if (!verdefSection || !vernumSection || !dynsymSection) {
        spdlog::warn(
            "Missing .gnu.version_d, .gnu.version, or .dynsym section for "
            "symbol versioning.");
        return std::nullopt;
    }

    size_t symbolIndex = 0;
    bool found = false;
    const auto& dynSyms =
        pImpl_->symbolTable_;
    for (size_t i = 0; i < dynSyms.size(); ++i) {
        if (dynSyms[i].name == symbol.name) {
            symbolIndex = i;
            found = true;
            break;
        }
    }

    if (!found) {
        spdlog::warn("Symbol {} not found in dynamic symbol table.",
                     symbol.name);
        return std::nullopt;
    }

    const Elf64_Half* vernum = reinterpret_cast<const Elf64_Half*>(
        pImpl_->fileContent_.data() + vernumSection->offset);

    if (symbolIndex >= vernumSection->size / sizeof(Elf64_Half)) {
        spdlog::warn("Symbol index {} out of bounds for .gnu.version section.",
                     symbolIndex);
        return std::nullopt;
    }

    Elf64_Half versionIndex = vernum[symbolIndex];

    if (versionIndex == 0 || versionIndex == 1) { // VER_NDX_LOCAL or VER_NDX_GLOBAL
        return std::nullopt;
    }

    const Elf64_Verdef* verdef = reinterpret_cast<const Elf64_Verdef*>(
        pImpl_->fileContent_.data() + verdefSection->offset);

    uint64_t currentOffset = 0;
    while (currentOffset < verdefSection->size) {
        const Elf64_Verdef* currentVerdef =
            reinterpret_cast<const Elf64_Verdef*>(
                reinterpret_cast<const char*>(verdef) + currentOffset);

        if (currentVerdef->vd_ndx == versionIndex) {
            const Elf64_Verdaux* verdaux =
                reinterpret_cast<const Elf64_Verdaux*>(
                    reinterpret_cast<const char*>(currentVerdef) +
                    currentVerdef->vd_aux);

            const char* dynstr = reinterpret_cast<const char*>(
                pImpl_->fileContent_.data() +
                pImpl_->findSection(".dynstr")->offset);

            return std::string(dynstr + verdaux->vda_name);
        }
        if (currentVerdef->vd_next == 0) {
            break;
        }
        currentOffset += currentVerdef->vd_next;
    }

    return std::nullopt;
}

auto ElfParser::getWeakSymbols() const -> std::vector<Symbol> {
    std::vector<Symbol> weakSymbols;
    for (const auto& symbol : getSymbolTable()) {
        if (symbol.bind == STB_WEAK) {
            weakSymbols.push_back(symbol);
        }
    }
    return weakSymbols;
}

auto ElfParser::getSymbolsByType(unsigned char type) const
    -> std::vector<Symbol> {
    std::vector<Symbol> result;
    for (const auto& symbol : getSymbolTable()) {
        if (symbol.type == type) {
            result.push_back(symbol);
        }
    }
    return result;
}

auto ElfParser::getExportedSymbols() const -> std::vector<Symbol> {
    std::vector<Symbol> result;
    for (const auto& symbol : getSymbolTable()) {
        if (symbol.bind == STB_GLOBAL && symbol.shndx != SHN_UNDEF) {
            result.push_back(symbol);
        }
    }
    return result;
}

auto ElfParser::getImportedSymbols() const -> std::vector<Symbol> {
    std::vector<Symbol> result;
    for (const auto& symbol : getSymbolTable()) {
        if (symbol.shndx == SHN_UNDEF) {
            result.push_back(symbol);
        }
    }
    return result;
}

auto ElfParser::findSymbolsByPattern(const std::string& pattern) const
    -> std::vector<Symbol> {
    std::vector<Symbol> result;
    std::regex regexPattern(pattern);
    for (const auto& symbol : getSymbolTable()) {
        if (std::regex_match(symbol.name, regexPattern)) {
            result.push_back(symbol);
        }
    }
    return result;
}

auto ElfParser::getSectionsByType(uint32_t type) const
    -> std::vector<SectionHeader> {
    if (auto it = pImpl_->sectionTypeCache_.find(type); it != pImpl_->sectionTypeCache_.end()) {
        return it->second;
    }

    std::vector<SectionHeader> result;
    for (const auto& section : getSectionHeaders()) {
        if (section.type == type) {
            result.push_back(section);
        }
    }
    pImpl_->sectionTypeCache_[type] = result;
    return result;
}

auto ElfParser::getSegmentPermissions(const ProgramHeader& header) const
    -> std::string {
    std::string perms;
    perms += (header.flags & PF_R) ? "r" : "-";
    perms += (header.flags & PF_W) ? "w" : "-";
    perms += (header.flags & PF_X) ? "x" : "-";
    return perms;
}

auto ElfParser::calculateChecksum() const -> uint64_t {
    if (!verifyIntegrity()) {
        return 0;
    }
    uint64_t checksum = 0;
    for (const auto& byte : pImpl_->fileContent_) {
        checksum = ((checksum << 5) + checksum) + byte;
    }
    return checksum;
}

auto ElfParser::isStripped() const -> bool {
    return getSymbolTable().empty() || !findSection(".symtab").has_value();
}

auto ElfParser::getDependencies() const -> std::vector<std::string> {
    std::vector<std::string> deps;
    auto dynstrSection = pImpl_->findSection(".dynstr");
    auto dynamicSection = pImpl_->findSection(".dynamic");

    if (!dynstrSection || !dynamicSection) {
        spdlog::warn("Missing .dynstr or .dynamic section for dependencies.");
        return deps;
    }

    const char* dynstr = reinterpret_cast<const char*>(
        pImpl_->fileContent_.data() + dynstrSection->offset);

    const Elf64_Dyn* dyn = reinterpret_cast<const Elf64_Dyn*>(
        pImpl_->fileContent_.data() + dynamicSection->offset);

    for (size_t i = 0; dyn[i].d_tag != DT_NULL; ++i) {
        if (dyn[i].d_tag == DT_NEEDED) {
            deps.push_back(std::string(dynstr + dyn[i].d_un.d_val));
        }
    }
    return deps;
}

void ElfParser::enableCache(bool enable) {
    if (!enable) {
        clearCache();
    }
}

void ElfParser::setParallelProcessing(bool enable) {
    pImpl_->useParallelProcessing_ = enable;
}

void ElfParser::setCacheSize(size_t size) {
    pImpl_->maxCacheSize_ = size;
    if (pImpl_->symbolCache_.size() > pImpl_->maxCacheSize_) {
        clearCache();
    }
}

void ElfParser::preloadSymbols() {
    for (const auto& symbol : getSymbolTable()) {
        pImpl_->symbolCache_[symbol.name] = symbol;
        pImpl_->addressCache_[symbol.value] = symbol;
    }
}

auto ElfParser::getRelocationEntries() const
    -> std::span<const RelocationEntry> {
    spdlog::info("ElfParser::getRelocationEntries called");
    return pImpl_->relocationEntries_;
}

auto ElfParser::getDynamicEntries() const -> std::span<const DynamicEntry> {
    spdlog::info("ElfParser::getDynamicEntries called");
    return pImpl_->dynamicEntries_;
}

namespace optimized {

class OptimizedElfParser::Impl {
public:
    explicit Impl(std::string_view file, const OptimizationConfig& config, PerformanceMetrics* metrics)
        : filePath_(file), config_(config), metrics_(metrics) {
        initializeResources();
        spdlog::info("OptimizedElfParser::Impl created for file: {} with optimizations enabled", file);
    }

    ~Impl() {
        cleanup();
    }

    auto parse() -> bool {
        auto timer = atom::utils::StopWatcher();
        timer.start();

        spdlog::info("Starting optimized parsing of ELF file: {}", filePath_);

        bool result = false;
        if (config_.enableMemoryMapping) {
            result = parseWithMemoryMapping();
        } else {
            result = parseWithBuffering();
        }

        timer.stop();
        metrics_->parseTime.store(static_cast<uint64_t>(timer.elapsedMilliseconds() * 1000000));

        if (result) {
            spdlog::info("Successfully parsed ELF file: {} in {}ns",
                        filePath_, metrics_->parseTime.load());
            if (config_.enablePrefetching) {
                prefetchCommonData();
            }
        } else {
            spdlog::error("Failed to parse ELF file: {}", filePath_);
        }

        return result;
    }

    auto parseAsync() -> std::future<bool> {
        return std::async(std::launch::async, [this]() {
            return parse();
        });
    }

    [[nodiscard]] auto getElfHeader() const -> std::optional<ElfHeader> {
        if (!elfHeader_.has_value()) {
            metrics_->cacheMisses.fetch_add(1);
            return std::nullopt;
        }
        metrics_->cacheHits.fetch_add(1);
        return elfHeader_;
    }

    [[nodiscard]] auto getProgramHeaders() const noexcept
        -> std::span<const ProgramHeader> {
        return programHeaders_;
    }

    [[nodiscard]] auto getSectionHeaders() const noexcept
        -> std::span<const SectionHeader> {
        return sectionHeaders_;
    }

    [[nodiscard]] auto getSymbolTable() const noexcept
        -> std::span<const Symbol> {
        return symbolTable_;
    }

    [[nodiscard]] auto findSymbolByName(std::string_view name) const
        -> std::optional<Symbol> {
        if (config_.enableSymbolCaching) {
            if (auto it = symbolNameCache_.find(std::string(name));
                it != symbolNameCache_.end()) {
                metrics_->cacheHits.fetch_add(1);
                return it->second;
            }
        }

        metrics_->cacheMisses.fetch_add(1);

        auto symbols = getSymbolTable();
        auto result = std::ranges::find_if(symbols,
            [name](const auto& symbol) { return symbol.name == name; });

        if (result != symbols.end()) {
            if (config_.enableSymbolCaching) {
                symbolNameCache_[std::string(name)] = *result;
            }
            return *result;
        }

        return std::nullopt;
    }

    [[nodiscard]] auto findSymbolByAddress(uint64_t address) const
        -> std::optional<Symbol> {
        if (config_.enableSymbolCaching) {
            if (auto it = symbolAddressCache_.find(address);
                it != symbolAddressCache_.end()) {
                metrics_->cacheHits.fetch_add(1);
                return it->second;
            }
        }

        metrics_->cacheMisses.fetch_add(1);

        auto symbols = getSymbolTable();
        if (symbolsSortedByAddress_) {
            auto it = std::lower_bound(symbols.begin(), symbols.end(), address,
                [](const Symbol& sym, uint64_t addr) {
                    return sym.value < addr;
                });

            if (it != symbols.end() && it->value == address) {
                if (config_.enableSymbolCaching) {
                    symbolAddressCache_[address] = *it;
                }
                return *it;
            }
        } else {
            auto result = std::ranges::find_if(symbols,
                [address](const auto& symbol) { return symbol.value == address; });

            if (result != symbols.end()) {
                if (config_.enableSymbolCaching) {
                    symbolAddressCache_[address] = *result;
                }
                return *result;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] auto getSymbolsInRange(uint64_t start, uint64_t end) const
        -> std::vector<Symbol> {
        std::vector<Symbol> result;
        auto symbols = getSymbolTable();

        if (config_.enableParallelProcessing && symbols.size() > 1000) {
            std::vector<Symbol> temp;
            std::copy_if(std::execution::par_unseq,
                        symbols.begin(), symbols.end(),
                        std::back_inserter(temp),
                        [start, end](const Symbol& sym) {
                            return sym.value >= start && sym.value < end;
                        });
            result = std::move(temp);
        } else {
            std::ranges::copy_if(symbols, std::back_inserter(result),
                               [start, end](const Symbol& sym) {
                                   return sym.value >= start && sym.value < end;
                               });
        }

        return result;
    }

    [[nodiscard]] auto getSectionsByType(uint32_t type) const
        -> std::vector<SectionHeader> {
        if (auto it = sectionTypeCache_.find(type);
            it != sectionTypeCache_.end()) {
            metrics_->cacheHits.fetch_add(1);
            return it->second;
        }

        metrics_->cacheMisses.fetch_add(1);

        std::vector<SectionHeader> result;
        auto sections = getSectionHeaders();

        std::ranges::copy_if(sections, std::back_inserter(result),
                           [type](const SectionHeader& section) {
                               return section.type == type;
                           });

        sectionTypeCache_[type] = result;
        return result;
    }

    [[nodiscard]] auto batchFindSymbols(const std::vector<std::string>& names) const
        -> std::vector<std::optional<Symbol>> {
        std::vector<std::optional<Symbol>> results;
        results.reserve(names.size());

        if (config_.enableParallelProcessing && names.size() > 10) {
            results.resize(names.size());
            std::transform(std::execution::par_unseq,
                         names.begin(), names.end(),
                         results.begin(),
                         [this](const std::string& name) {
                             return findSymbolByName(name);
                         });
        } else {
            std::ranges::transform(names, std::back_inserter(results),
                                 [this](const std::string& name) {
                                     return findSymbolByName(name);
                                 });
        }

        return results;
    }

    void prefetchData(const std::vector<uint64_t>& addresses) const {
        if (!config_.enablePrefetching || !mmappedData_) {
            return;
        }

        for (uint64_t addr : addresses) {
            if (addr < fileSize_) {
                volatile auto dummy = mmappedData_[addr];
                (void)dummy;
            }
        }
    }

    void optimizeMemoryLayout() {
        if (!symbolsSortedByAddress_) {
            std::ranges::sort(symbolTable_,
                            [](const Symbol& a, const Symbol& b) {
                                return a.value < b.value;
                            });
            symbolsSortedByAddress_ = true;
        }

        symbolTable_.shrink_to_fit();
        sectionHeaders_.shrink_to_fit();
        programHeaders_.shrink_to_fit();

        spdlog::info("Memory layout optimized for better cache performance");
    }

    [[nodiscard]] auto validateIntegrity() const -> bool {
        if (validated_) {
            return true;
        }

        auto futures = std::vector<std::future<bool>>{};

        futures.emplace_back(std::async(std::launch::async, [this]() {
            return validateElfHeader();
        }));

        futures.emplace_back(std::async(std::launch::async, [this]() {
            return validateSectionHeaders();
        }));

        futures.emplace_back(std::async(std::launch::async, [this]() {
            return validateProgramHeaders();
        }));

        bool result = std::ranges::all_of(futures, [](auto& future) {
            return future.get();
        });

        validated_ = result;
        return result;
    }

    [[nodiscard]] auto getMemoryUsage() const -> size_t {
        size_t usage = 0;
        usage += fileContent_.capacity();
        usage += symbolTable_.capacity() * sizeof(Symbol);
        usage += sectionHeaders_.capacity() * sizeof(SectionHeader);
        usage += programHeaders_.capacity() * sizeof(ProgramHeader);
        usage += symbolNameCache_.size() * (sizeof(std::string) + sizeof(Symbol));
        usage += symbolAddressCache_.size() * (sizeof(uint64_t) + sizeof(Symbol));
        return usage;
    }

private:
    std::string filePath_;
    OptimizationConfig config_;

    uint8_t* mmappedData_ = nullptr;
    size_t fileSize_ = 0;

#ifdef LITHIUM_OPTIMIZED_ELF_UNIX
    int fileDescriptor_ = -1;
#elif defined(LITHIUM_OPTIMIZED_ELF_WINDOWS)
    HANDLE fileHandle_ = INVALID_HANDLE_VALUE;
    HANDLE fileMappingHandle_ = nullptr;
#endif

    std::vector<uint8_t> fileContent_;

    std::optional<ElfHeader> elfHeader_;
    std::vector<ProgramHeader> programHeaders_;
    std::vector<SectionHeader> sectionHeaders_;
    std::vector<Symbol> symbolTable_;

    mutable std::unordered_map<std::string, Symbol> symbolNameCache_;
    mutable std::unordered_map<uint64_t, Symbol> symbolAddressCache_;
    mutable std::unordered_map<uint32_t, std::vector<SectionHeader>> sectionTypeCache_;

    mutable bool validated_ = false;
    bool symbolsSortedByAddress_ = false;

    PerformanceMetrics* metrics_;

    void initializeResources() {
        if (config_.enableSymbolCaching) {
            symbolNameCache_.reserve(config_.cacheSize / sizeof(Symbol));
            symbolAddressCache_.reserve(config_.cacheSize / sizeof(Symbol));
        }
    }

    void cleanup() {
#ifdef LITHIUM_OPTIMIZED_ELF_UNIX
        if (mmappedData_ && mmappedData_ != MAP_FAILED) {
            munmap(mmappedData_, fileSize_);
            mmappedData_ = nullptr;
        }

        if (fileDescriptor_ >= 0) {
            close(fileDescriptor_);
            fileDescriptor_ = -1;
        }
#elif defined(LITHIUM_OPTIMIZED_ELF_WINDOWS)
        if (mmappedData_) {
            UnmapViewOfFile(mmappedData_);
            mmappedData_ = nullptr;
        }

        if (fileMappingHandle_) {
            CloseHandle(fileMappingHandle_);
            fileMappingHandle_ = nullptr;
        }

        if (fileHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle_);
            fileHandle_ = INVALID_HANDLE_VALUE;
        }
#endif
    }

    auto parseWithMemoryMapping() -> bool {
#ifdef LITHIUM_OPTIMIZED_ELF_UNIX
        fileDescriptor_ = open(filePath_.c_str(), O_RDONLY);
        if (fileDescriptor_ < 0) {
            spdlog::error("Failed to open file: {}", filePath_);
            return false;
        }

        struct stat fileInfo;
        if (fstat(fileDescriptor_, &fileInfo) < 0) {
            spdlog::error("Failed to get file info: {}", filePath_);
            return false;
        }

        fileSize_ = fileInfo.st_size;
        mmappedData_ = static_cast<uint8_t*>(
            mmap(nullptr, fileSize_, PROT_READ, MAP_PRIVATE, fileDescriptor_, 0));

        if (mmappedData_ == MAP_FAILED) {
            spdlog::error("Failed to memory map file: {}", filePath_);
            return parseWithBuffering();
        }

        madvise(mmappedData_, fileSize_, MADV_SEQUENTIAL);

#elif defined(LITHIUM_OPTIMIZED_ELF_WINDOWS)
        fileHandle_ = CreateFileA(filePath_.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileHandle_ == INVALID_HANDLE_VALUE) {
            spdlog::error("Failed to open file: {}", filePath_);
            return false;
        }

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(fileHandle_, &fileSize)) {
            spdlog::error("Failed to get file size: {}", filePath_);
            CloseHandle(fileHandle_);
            return false;
        }

        fileSize_ = fileSize.QuadPart;

        fileMappingHandle_ = CreateFileMappingA(fileHandle_, nullptr, PAGE_READONLY,
                                              fileSize.HighPart, fileSize.LowPart, nullptr);
        if (fileMappingHandle_ == nullptr) {
            spdlog::error("Failed to create file mapping: {}", filePath_);
            CloseHandle(fileHandle_);
            return false;
        }

        mmappedData_ = static_cast<uint8_t*>(
            MapViewOfFile(fileMappingHandle_, FILE_MAP_READ, 0, 0, fileSize_));

        if (mmappedData_ == nullptr) {
            spdlog::error("Failed to map view of file: {}", filePath_);
            CloseHandle(fileMappingHandle_);
            CloseHandle(fileHandle_);
            return parseWithBuffering();
        }
#else
        spdlog::warn("Memory mapping not supported on this platform, using buffered I/O");
        return parseWithBuffering();
#endif

        return parseElfStructures();
    }

    auto parseWithBuffering() -> bool {
        std::ifstream file(filePath_, std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open file: {}", filePath_);
            return false;
        }

        file.seekg(0, std::ios::end);
        fileSize_ = file.tellg();
        file.seekg(0, std::ios::beg);

        fileContent_.resize(fileSize_);
        file.read(reinterpret_cast<char*>(fileContent_.data()), fileSize_);

        return parseElfStructures();
    }

    auto parseElfStructures() -> bool {
        const uint8_t* data = mmappedData_ ? mmappedData_ : fileContent_.data();

        return parseElfHeader(data) &&
               parseProgramHeaders(data) &&
               parseSectionHeaders(data) &&
               parseSymbolTable(data);
    }

    auto parseElfHeader(const uint8_t* data) -> bool {
        if (fileSize_ < sizeof(Elf64_Ehdr)) {
            return false;
        }

        const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

        if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
            ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
            ehdr->e_ident[EI_MAG3] != ELFMAG3) {
            return false;
        }

        elfHeader_ = ElfHeader{
            .type = ehdr->e_type,
            .machine = ehdr->e_machine,
            .version = ehdr->e_version,
            .entry = ehdr->e_entry,
            .phoff = ehdr->e_phoff,
            .shoff = ehdr->e_shoff,
            .flags = ehdr->e_flags,
            .ehsize = ehdr->e_ehsize,
            .phentsize = ehdr->e_phentsize,
            .phnum = ehdr->e_phnum,
            .shentsize = ehdr->e_shentsize,
            .shnum = ehdr->e_shnum,
            .shstrndx = ehdr->e_shstrndx
        };

        return true;
    }

    auto parseProgramHeaders(const uint8_t* data) -> bool {
        if (!elfHeader_) return false;

        const auto* phdr = reinterpret_cast<const Elf64_Phdr*>(
            data + elfHeader_->phoff);

        programHeaders_.reserve(elfHeader_->phnum);

        for (uint16_t i = 0; i < elfHeader_->phnum; ++i) {
            programHeaders_.emplace_back(ProgramHeader{
                .type = phdr[i].p_type,
                .offset = phdr[i].p_offset,
                .vaddr = phdr[i].p_vaddr,
                .paddr = phdr[i].p_paddr,
                .filesz = phdr[i].p_filesz,
                .memsz = phdr[i].p_memsz,
                .flags = phdr[i].p_flags,
                .align = phdr[i].p_align
            });
        }

        return true;
    }

    auto parseSectionHeaders(const uint8_t* data) -> bool {
        if (!elfHeader_) return false;

        const auto* shdr = reinterpret_cast<const Elf64_Shdr*>(
            data + elfHeader_->shoff);
        const auto* strtab = reinterpret_cast<const char*>(
            data + shdr[elfHeader_->shstrndx].sh_offset);

        sectionHeaders_.reserve(elfHeader_->shnum);

        for (uint16_t i = 0; i < elfHeader_->shnum; ++i) {
            sectionHeaders_.emplace_back(SectionHeader{
                .name = std::string(strtab + shdr[i].sh_name),
                .type = shdr[i].sh_type,
                .flags = shdr[i].sh_flags,
                .addr = shdr[i].sh_addr,
                .offset = shdr[i].sh_offset,
                .size = shdr[i].sh_size,
                .link = shdr[i].sh_link,
                .info = shdr[i].sh_info,
                .addralign = shdr[i].sh_addralign,
                .entsize = shdr[i].sh_entsize
            });
        }

        return true;
    }

    auto parseSymbolTable(const uint8_t* data) -> bool {
        auto symtabSection = std::ranges::find_if(sectionHeaders_,
            [](const auto& section) { return section.type == SHT_SYMTAB; });

        if (symtabSection == sectionHeaders_.end()) {
            return true;
        }

        const auto* symtab = reinterpret_cast<const Elf64_Sym*>(
            data + symtabSection->offset);
        size_t numSymbols = symtabSection->size / sizeof(Elf64_Sym);

        const auto* strtab = reinterpret_cast<const char*>(
            data + sectionHeaders_[symtabSection->link].offset);

        symbolTable_.reserve(numSymbols);

        for (size_t i = 0; i < numSymbols; ++i) {
            symbolTable_.emplace_back(Symbol{
                .name = std::string(strtab + symtab[i].st_name),
                .value = symtab[i].st_value,
                .size = symtab[i].st_size,
                .bind = static_cast<unsigned char>(ELF64_ST_BIND(symtab[i].st_info)),
                .type = static_cast<unsigned char>(ELF64_ST_TYPE(symtab[i].st_info)),
                .shndx = symtab[i].st_shndx
            });
        }

        return true;
    }

    void prefetchCommonData() const {
        if (!config_.enablePrefetching || !mmappedData_) {
            return;
        }

        for (const auto& symbol : symbolTable_) {
            if (symbol.value < fileSize_) {
                volatile auto dummy = mmappedData_[symbol.value];
                (void)dummy;
            }
        }

        spdlog::debug("Prefetched common data for improved performance");
    }

    auto validateElfHeader() const -> bool {
        if (!elfHeader_) return false;

        const uint8_t* data = mmappedData_ ? mmappedData_ : fileContent_.data();
        const auto* ident = reinterpret_cast<const unsigned char*>(data);

        return ident[EI_MAG0] == ELFMAG0 &&
               ident[EI_MAG1] == ELFMAG1 &&
               ident[EI_MAG2] == ELFMAG2 &&
               ident[EI_MAG3] == ELFMAG3;
    }

    auto validateSectionHeaders() const -> bool {
        if (!elfHeader_) return false;

        const auto totalSize = elfHeader_->shoff +
                              (elfHeader_->shnum * elfHeader_->shentsize);
        return totalSize <= fileSize_;
    }

    auto validateProgramHeaders() const -> bool {
        if (!elfHeader_) return false;

        const auto totalSize = elfHeader_->phoff +
                              (elfHeader_->phnum * elfHeader_->phentsize);
        return totalSize <= fileSize_;
    }
};

OptimizedElfParser::OptimizedElfParser(std::string_view file,
                                       const OptimizationConfig& config)
    : pImpl_(std::make_unique<Impl>(file, config, &metrics_)),
      config_(config) {
    initializeOptimizations();
    spdlog::info("OptimizedElfParser created for file: {}", file);
}

OptimizedElfParser::OptimizedElfParser(std::string_view file)
    : OptimizedElfParser(file, OptimizationConfig{}) {
}

OptimizedElfParser::OptimizedElfParser(OptimizedElfParser&& other) noexcept
    : pImpl_(std::move(other.pImpl_)), config_(std::move(other.config_)) {
}

OptimizedElfParser& OptimizedElfParser::operator=(OptimizedElfParser&& other) noexcept {
    if (this != &other) {
        pImpl_ = std::move(other.pImpl_);
        config_ = std::move(other.config_);
    }
    return *this;
}

OptimizedElfParser::~OptimizedElfParser() = default;

auto OptimizedElfParser::parse() -> bool {
    return pImpl_->parse();
}

auto OptimizedElfParser::parseAsync() -> std::future<bool> {
    return pImpl_->parseAsync();
}

auto OptimizedElfParser::getElfHeader() const -> std::optional<ElfHeader> {
    return pImpl_->getElfHeader();
}

auto OptimizedElfParser::getProgramHeaders() const noexcept
    -> std::span<const ProgramHeader> {
    return pImpl_->getProgramHeaders();
}

auto OptimizedElfParser::getSectionHeaders() const noexcept
    -> std::span<const SectionHeader> {
    return pImpl_->getSectionHeaders();
}

auto OptimizedElfParser::getSymbolTable() const noexcept
    -> std::span<const Symbol> {
    return pImpl_->getSymbolTable();
}

auto OptimizedElfParser::findSymbolByName(std::string_view name) const
    -> std::optional<Symbol> {
    return pImpl_->findSymbolByName(name);
}

auto OptimizedElfParser::findSymbolByAddress(uint64_t address) const
    -> std::optional<Symbol> {
    return pImpl_->findSymbolByAddress(address);
}

auto OptimizedElfParser::getSymbolsInRange(uint64_t start, uint64_t end) const
    -> std::vector<Symbol> {
    return pImpl_->getSymbolsInRange(start, end);
}

auto OptimizedElfParser::getSectionsByType(uint32_t type) const
    -> std::vector<SectionHeader> {
    return pImpl_->getSectionsByType(type);
}

auto OptimizedElfParser::batchFindSymbols(const std::vector<std::string>& names) const
    -> std::vector<std::optional<Symbol>> {
    return pImpl_->batchFindSymbols(names);
}

void OptimizedElfParser::prefetchData(const std::vector<uint64_t>& addresses) const {
    pImpl_->prefetchData(addresses);
}

auto OptimizedElfParser::getMetrics() const -> PerformanceMetrics {
    return metrics_;
}

void OptimizedElfParser::resetMetrics() {
    metrics_.parseTime.store(0);
    metrics_.cacheHits.store(0);
    metrics_.cacheMisses.store(0);
    metrics_.memoryAllocations.store(0);
    metrics_.peakMemoryUsage.store(0);
    metrics_.threadsUsed.store(0);
}

void OptimizedElfParser::optimizeMemoryLayout() {
    pImpl_->optimizeMemoryLayout();
}

void OptimizedElfParser::updateConfig(const OptimizationConfig& config) {
    config_ = config;
    initializeOptimizations();
}

auto OptimizedElfParser::validateIntegrity() const -> bool {
    return pImpl_->validateIntegrity();
}

auto OptimizedElfParser::getMemoryUsage() const -> size_t {
    return pImpl_->getMemoryUsage();
}

auto OptimizedElfParser::exportSymbols(std::string_view format) const -> std::string {
    const auto symbols = getSymbolTable();

    if (format == "json") {
        std::string result = "[\n";
        for (size_t i = 0; i < symbols.size(); ++i) {
            const auto& sym = symbols[i];
            result += "  {\n";
            result += "    \"name\": \"" + sym.name + "\",\n";
            result += "    \"value\": " + std::to_string(sym.value) + ",\n";
            result += "    \"size\": " + std::to_string(sym.size) + ",\n";
            result += "    \"type\": " + std::to_string(sym.type) + "\n";
            result += "  }";
            if (i < symbols.size() - 1) result += ",";
            result += "\n";
        }
        result += "]";
        return result;
    }

    return "Unsupported format";
}

void OptimizedElfParser::initializeOptimizations() {
    setupMemoryPools();
    performanceTimer_ = std::make_unique<atom::utils::StopWatcher>();
}

void OptimizedElfParser::setupMemoryPools() {
    memoryPool_ = std::make_unique<MemoryPool>();
    bufferResource_ = std::make_unique<std::pmr::monotonic_buffer_resource>(
        config_.cacheSize, std::pmr::get_default_resource());

    if (config_.enableSymbolCaching) {
        symbolCache_ = std::make_unique<SymbolCache>(bufferResource_.get());
        addressCache_ = std::make_unique<AddressCache>(bufferResource_.get());
        sectionCache_ = std::make_unique<SectionCache>(bufferResource_.get());
    }
}

void OptimizedElfParser::warmupCaches() {
    if (config_.enableSymbolCaching) {
        const auto symbols = getSymbolTable();
        for (const auto& symbol : symbols) {
            if (!symbol.name.empty()) {
                (*symbolCache_)[symbol.name] = symbol;
                (*addressCache_)[symbol.value] = symbol;
            }
        }
    }
}

} // namespace optimized

#ifdef __linux__
EnhancedElfParser::EnhancedElfParser(std::string_view file, bool useOptimized)
    : filePath_(file), useOptimized_(useOptimized) {
    if (useOptimized_) {
        optimizedParser_ = std::make_unique<optimized::OptimizedElfParser>(file);
    } else {
        standardParser_ = std::make_unique<ElfParser>(file);
    }
}

auto EnhancedElfParser::parse() -> bool {
    if (useOptimized_) {
        return optimizedParser_->parse();
    } else {
        return standardParser_->parse();
    }
}

auto EnhancedElfParser::getElfHeader() const -> std::optional<ElfHeader> {
    if (useOptimized_) {
        return optimizedParser_->getElfHeader();
    } else {
        return standardParser_->getElfHeader();
    }
}

auto EnhancedElfParser::findSymbolByName(std::string_view name) const -> std::optional<Symbol> {
    if (useOptimized_) {
        return optimizedParser_->findSymbolByName(name);
    } else {
        return standardParser_->findSymbolByName(name);
    }
}

auto EnhancedElfParser::getSymbolTable() const -> std::span<const Symbol> {
    if (useOptimized_) {
        return optimizedParser_->getSymbolTable();
    } else {
        return standardParser_->getSymbolTable();
    }
}

auto EnhancedElfParser::comparePerformance() -> void {
    if (!useOptimized_) {
        return;
    }

    spdlog::info("Enhanced ELF Parser performance comparison for: {}", filePath_);
    auto metrics = optimizedParser_->getMetrics();
    spdlog::info("Parse time: {}ms", metrics.parseTime.load() / 1000000.0);
    spdlog::info("Cache hits: {}", metrics.cacheHits.load());
    spdlog::info("Cache misses: {}", metrics.cacheMisses.load());

    if (metrics.cacheHits.load() + metrics.cacheMisses.load() > 0) {
        double hitRate = static_cast<double>(metrics.cacheHits.load()) /
                       (metrics.cacheHits.load() + metrics.cacheMisses.load()) * 100.0;
        spdlog::info("Cache hit rate: {:.2f}%", hitRate);
    }
}

auto createElfParser(std::string_view file, bool preferOptimized) -> std::unique_ptr<EnhancedElfParser> {
    return std::make_unique<EnhancedElfParser>(file, preferOptimized);
}

auto migrateToEnhancedParser(const ElfParser& oldParser, std::string_view filePath) -> std::unique_ptr<EnhancedElfParser> {
    auto enhanced = createElfParser(filePath, true);
    enhanced->parse();
    return enhanced;
}
#endif

} // namespace lithium

#endif // __linux__
