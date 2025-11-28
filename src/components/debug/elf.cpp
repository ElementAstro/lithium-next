#ifdef __linux__

#include "elf.hpp"

#include <cxxabi.h>
#include <elf.h>
#include <algorithm>
#include <fstream>
#include <regex>
#include <unordered_map>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace lithium {

class ElfParser::Impl {
public:
    explicit Impl(std::string_view file) : filePath_(file) {
        LOG_F(INFO, "ElfParser::Impl created for file: {}", file);
    }

    auto parse() -> bool {
        LOG_F(INFO, "Parsing ELF file: {}", filePath_);
        std::ifstream file(filePath_, std::ios::binary);
        if (!file) {
            LOG_F(ERROR, "Failed to open file: {}", filePath_);
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
            LOG_F(INFO, "Successfully parsed ELF file: {}", filePath_);
        } else {
            LOG_F(ERROR, "Failed to parse ELF file: {}", filePath_);
        }
        return result;
    }

    [[nodiscard]] auto getElfHeader() const -> std::optional<ElfHeader> {
        LOG_F(INFO, "Getting ELF header");
        return elfHeader_;
    }

    [[nodiscard]] auto getProgramHeaders() const
        -> std::span<const ProgramHeader> {
        LOG_F(INFO, "Getting program headers");
        return programHeaders_;
    }

    [[nodiscard]] auto getSectionHeaders() const
        -> std::span<const SectionHeader> {
        LOG_F(INFO, "Getting section headers");
        return sectionHeaders_;
    }

    [[nodiscard]] auto getSymbolTable() const -> std::span<const Symbol> {
        LOG_F(INFO, "Getting symbol table");
        return symbolTable_;
    }

    [[nodiscard]] auto findSymbolByName(std::string_view name) const
        -> std::optional<Symbol> {
        LOG_F(INFO, "Finding symbol by name: {}", name);
        auto cachedSymbol = findSymbolInCache(name);
        if (cachedSymbol) {
            return cachedSymbol;
        }
        auto it = std::ranges::find_if(
            symbolTable_,
            [name](const auto& symbol) { return symbol.name == name; });
        if (it != symbolTable_.end()) {
            LOG_F(INFO, "Found symbol: {}", name);
            symbolCache_[std::string(name)] = *it;
            return *it;
        }
        LOG_F(WARNING, "Symbol not found: {}", name);
        return std::nullopt;
    }

    [[nodiscard]] auto findSymbolByAddress(uint64_t address) const
        -> std::optional<Symbol> {
        LOG_F(INFO, "Finding symbol by address: {}", address);
        auto cachedSymbol = findSymbolByAddressInCache(address);
        if (cachedSymbol) {
            return cachedSymbol;
        }
        auto it = std::ranges::find_if(
            symbolTable_,
            [address](const auto& symbol) { return symbol.value == address; });
        if (it != symbolTable_.end()) {
            LOG_F(INFO, "Found symbol at address: {}", address);
            addressCache_[address] = *it;
            return *it;
        }
        LOG_F(WARNING, "Symbol not found at address: {}", address);
        return std::nullopt;
    }

    [[nodiscard]] auto findSection(std::string_view name) const
        -> std::optional<SectionHeader> {
        LOG_F(INFO, "Finding section by name: {}", name);
        auto it = std::ranges::find_if(
            sectionHeaders_,
            [name](const auto& section) { return section.name == name; });
        if (it != sectionHeaders_.end()) {
            LOG_F(INFO, "Found section: {}", name);
            return *it;
        }
        LOG_F(WARNING, "Section not found: {}", name);
        return std::nullopt;
    }

    [[nodiscard]] auto getSectionData(const SectionHeader& section) const
        -> std::vector<uint8_t> {
        LOG_F(INFO, "Getting data for section: {}", section.name);
        if (section.offset + section.size > fileSize_) {
            LOG_F(ERROR, "Section data out of bounds: {}", section.name);
            THROW_OUT_OF_RANGE("Section data out of bounds");
        }
        return {fileContent_.begin() + section.offset,
                fileContent_.begin() + section.offset + section.size};
    }

    [[nodiscard]] auto getSymbolsInRange(uint64_t start, uint64_t end) const
        -> std::vector<Symbol> {
        LOG_F(INFO, "Getting symbols in range: [{:x}, {:x}]", start, end);
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
        LOG_F(INFO, "Getting executable segments");
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

        LOG_F(INFO, "Verifying ELF file integrity");

        // 验证文件头魔数
        const auto* ident =
            reinterpret_cast<const unsigned char*>(fileContent_.data());
        if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
            ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3) {
            LOG_F(ERROR, "Invalid ELF magic number");
            return false;
        }

        // 验证段表和节表的完整性
        if (!elfHeader_) {
            LOG_F(ERROR, "Missing ELF header");
            return false;
        }

        const auto totalSize =
            elfHeader_->shoff + (elfHeader_->shnum * elfHeader_->shentsize);
        if (totalSize > fileSize_) {
            LOG_F(ERROR, "File size too small for section headers");
            return false;
        }

        verified_ = true;
        return true;
    }

    void clearCache() {
        LOG_F(INFO, "Clearing parser cache");
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

    auto parseElfHeader() -> bool {
        LOG_F(INFO, "Parsing ELF header");
        if (fileSize_ < sizeof(Elf64_Ehdr)) {
            LOG_F(ERROR, "File size too small for ELF header");
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

        LOG_F(INFO, "Parsed ELF header successfully");
        return true;
    }

    auto parseProgramHeaders() -> bool {
        LOG_F(INFO, "Parsing program headers");
        if (!elfHeader_) {
            LOG_F(ERROR, "ELF header not parsed");
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

        LOG_F(INFO, "Parsed program headers successfully");
        return true;
    }

    auto parseSectionHeaders() -> bool {
        LOG_F(INFO, "Parsing section headers");
        if (!elfHeader_) {
            LOG_F(ERROR, "ELF header not parsed");
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

        LOG_F(INFO, "Parsed section headers successfully");
        return true;
    }

    auto parseSymbolTable() -> bool {
        LOG_F(INFO, "Parsing symbol table");
        auto symtabSection = std::ranges::find_if(
            sectionHeaders_,
            [](const auto& section) { return section.type == SHT_SYMTAB; });

        if (symtabSection == sectionHeaders_.end()) {
            LOG_F(WARNING, "No symbol table found");
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

        LOG_F(INFO, "Parsed symbol table successfully");
        return true;
    }

    auto parseDynamicEntries() -> bool {
        LOG_F(INFO, "Parsing dynamic entries");
        auto dynamicSection = std::ranges::find_if(
            sectionHeaders_,
            [](const auto& section) { return section.type == SHT_DYNAMIC; });

        if (dynamicSection == sectionHeaders_.end()) {
            LOG_F(INFO, "No dynamic section found");
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

        LOG_F(INFO, "Parsed {} dynamic entries", dynamicEntries_.size());
        return true;
    }

    auto parseRelocationEntries() -> bool {
        LOG_F(INFO, "Parsing relocation entries");
        std::vector<SectionHeader> relaSections;

        // 收集所有重定位节
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

        LOG_F(INFO, "Parsed {} relocation entries", relocationEntries_.size());
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

// ElfParser method implementations
ElfParser::ElfParser(std::string_view file)
    : pImpl_(std::make_unique<Impl>(file)) {
    LOG_F(INFO, "ElfParser created for file: {}", file);
}

ElfParser::~ElfParser() = default;

auto ElfParser::parse() -> bool {
    LOG_F(INFO, "ElfParser::parse called");
    return pImpl_->parse();
}

auto ElfParser::getElfHeader() const -> std::optional<ElfHeader> {
    LOG_F(INFO, "ElfParser::getElfHeader called");
    return pImpl_->getElfHeader();
}

auto ElfParser::getProgramHeaders() const -> std::span<const ProgramHeader> {
    LOG_F(INFO, "ElfParser::getProgramHeaders called");
    return pImpl_->getProgramHeaders();
}

auto ElfParser::getSectionHeaders() const -> std::span<const SectionHeader> {
    LOG_F(INFO, "ElfParser::getSectionHeaders called");
    return pImpl_->getSectionHeaders();
}

auto ElfParser::getSymbolTable() const -> std::span<const Symbol> {
    LOG_F(INFO, "ElfParser::getSymbolTable called");
    return pImpl_->getSymbolTable();
}

auto ElfParser::findSymbolByName(std::string_view name) const
    -> std::optional<Symbol> {
    LOG_F(INFO, "ElfParser::findSymbolByName called with name: {}", name);
    return pImpl_->findSymbolByName(name);
}

auto ElfParser::findSymbolByAddress(uint64_t address) const
    -> std::optional<Symbol> {
    LOG_F(INFO, "ElfParser::findSymbolByAddress called with address: {}",
          address);
    return pImpl_->findSymbolByAddress(address);
}

auto ElfParser::findSection(std::string_view name) const
    -> std::optional<SectionHeader> {
    LOG_F(INFO, "ElfParser::findSection called with name: {}", name);
    return pImpl_->findSection(name);
}

auto ElfParser::getSectionData(const SectionHeader& section) const
    -> std::vector<uint8_t> {
    LOG_F(INFO, "ElfParser::getSectionData called for section: {}",
          section.name);
    return pImpl_->getSectionData(section);
}

auto ElfParser::getSymbolsInRange(uint64_t start,
                                  uint64_t end) const -> std::vector<Symbol> {
    LOG_F(INFO, "ElfParser::getSymbolsInRange called");
    return pImpl_->getSymbolsInRange(start, end);
}

auto ElfParser::getExecutableSegments() const -> std::vector<ProgramHeader> {
    LOG_F(INFO, "ElfParser::getExecutableSegments called");
    return pImpl_->getExecutableSegments();
}

auto ElfParser::verifyIntegrity() const -> bool {
    LOG_F(INFO, "ElfParser::verifyIntegrity called");
    return pImpl_->verifyIntegrity();
}

void ElfParser::clearCache() {
    LOG_F(INFO, "ElfParser::clearCache called");
    pImpl_->clearCache();
}

auto ElfParser::demangleSymbolName(const std::string& name) const
    -> std::string {
    // 使用 abi::__cxa_demangle 进行符号名称解除修饰
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
    auto section = findSection(".gnu.version");
    if (!section) {
        return std::nullopt;
    }
    // 实现符号版本查找逻辑
    return std::nullopt;
}

// 符号相关查询
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

// 节和段相关
auto ElfParser::getSectionsByType(uint32_t type) const
    -> std::vector<SectionHeader> {
    if (auto it = sectionTypeCache_.find(type); it != sectionTypeCache_.end()) {
        return it->second;
    }

    std::vector<SectionHeader> result;
    for (const auto& section : getSectionHeaders()) {
        if (section.type == type) {
            result.push_back(section);
        }
    }
    sectionTypeCache_[type] = result;
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

// 工具方法
auto ElfParser::calculateChecksum() const -> uint64_t {
    if (!verifyIntegrity()) {
        return 0;
    }
    // 简单的校验和实现
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
    auto dynstr = findSection(".dynstr");
    auto dynamic = findSection(".dynamic");
    if (!dynstr || !dynamic) {
        return deps;
    }

    auto dynstrData = getSectionData(*dynstr);

    const auto extractString = [&dynstrData](uint64_t offset) -> std::string {
        if (offset >= dynstrData.size()) {
            return {};
        }

        const char* begin =
            reinterpret_cast<const char*>(dynstrData.data() + offset);
        size_t length = 0;
        while (offset + length < dynstrData.size() && begin[length] != '\0') {
            ++length;
        }
        return std::string(begin, length);
    };

    const auto& entries = pImpl_->dynamicEntries_;
    for (const auto& entry : entries) {
        if (entry.tag != DT_NEEDED) {
            continue;
        }

        std::string dependency = extractString(entry.d_un.val);
        if (!dependency.empty()) {
            deps.push_back(std::move(dependency));
        }
    }

    return deps;
}

// 缓存控制
void ElfParser::enableCache(bool enable) {
    if (!enable) {
        clearCache();
    }
}

void ElfParser::setParallelProcessing(bool enable) {
    useParallelProcessing_ = enable;
}

void ElfParser::setCacheSize(size_t size) {
    maxCacheSize_ = size;
    if (symbolCache_.size() > maxCacheSize_) {
        clearCache();
    }
}

void ElfParser::preloadSymbols() {
    for (const auto& symbol : getSymbolTable()) {
        symbolCache_[symbol.name] = symbol;
        addressCache_[symbol.value] = symbol;
    }
}

auto ElfParser::getRelocationEntries() const
    -> std::span<const RelocationEntry> {
    LOG_F(INFO, "ElfParser::getRelocationEntries called");
    return pImpl_->relocationEntries_;
}

auto ElfParser::getDynamicEntries() const -> std::span<const DynamicEntry> {
    LOG_F(INFO, "ElfParser::getDynamicEntries called");
    return pImpl_->dynamicEntries_;
}

}  // namespace lithium

#endif  // __linux__
