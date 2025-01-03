#ifndef MEMORY_PROFILER_H
#define MEMORY_PROFILER_H

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

// 内存访问模式分析结构
struct AccessPattern {
    uint64_t stride;           // 步长
    uint32_t frequency;        // 出现频率
    double probability;        // 概率权重
};

// 内存区域描述符
struct MemoryRegion {
    uint64_t baseAddress;      // 基地址
    uint64_t size;            // 区域大小
    uint64_t totalAccesses;   // 总访问次数
    bool isActive;            // 是否活跃
};

// 访存特征分析器类
class AccessAnalyzer {
public:
    struct AccessStats {
        std::vector<AccessPattern> patterns;
        AccessPattern dominantPattern;
        AccessPattern secondaryPattern;
        double spatialLocality;
        double temporalLocality;
    };

    void recordAccess(uint64_t address);
    void finalizeAnalysis();
    AccessStats getStatistics() const;

private:
    static constexpr size_t PATTERN_CACHE_SIZE = 16;
    static constexpr uint64_t MAX_STRIDE = 1024*1024;

    std::vector<uint64_t> accessHistory;
    std::unordered_map<uint64_t, uint32_t> strideFrequencies;
    double spatialLocality = 0.0;  // 添加成员变量
    double temporalLocality = 0.0; // 添加成员变量

    void updatePatternCache(uint64_t stride);
    void calculateLocality();
    double computeSpatialScore(const std::vector<uint64_t>& accesses) const;
    double computeTemporalScore(const std::vector<uint64_t>& accesses) const;
};

// 内存访问分析器类
class MemoryProfiler {
public:
    struct RegionInfo {
        std::string name;                  // 变量名称
        std::string scopeName;             // 作用域名称
        uint64_t baseAddr;                 // 基地址
        uint64_t size;                     // 大小
        uint64_t accessCount;              // 访问计数
        AccessAnalyzer::AccessStats stats; // 访问统计
    };

    static MemoryProfiler& getInstance() {
        static MemoryProfiler instance;
        return instance;
    }

    void initializeRegion(const std::string& name, const std::string& scope,
                         void* addr, size_t size);

    void recordAccess(const std::string& name, void* addr);

    void finalizeRegion(const std::string& name);

    std::vector<RegionInfo> generateReport() const;

private:
    MemoryProfiler() = default;
    std::unordered_map<std::string, RegionInfo> regions;
    std::unordered_map<std::string, AccessAnalyzer> analyzers;
};

// 内联辅助函数
inline void initMemoryRegion(const char* name, const char* scope, void* addr, size_t size) {
    MemoryProfiler::getInstance().initializeRegion(name, scope, addr, size);
}

inline void recordMemoryAccess(const char* name, void* addr) {
    MemoryProfiler::getInstance().recordAccess(name, addr);
}

inline void finalizeMemoryRegion(const char* name) {
    MemoryProfiler::getInstance().finalizeRegion(name);
}

#endif // MEMORY_PROFILER_H