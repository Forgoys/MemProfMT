#include "MemoryProfiler.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

void AccessAnalyzer::recordAccess(uint64_t address) {
    if (!accessHistory.empty()) {
        uint64_t lastAddr = accessHistory.back();
        uint64_t stride = (address > lastAddr) ? 
                         (address - lastAddr) : (lastAddr - address);
                         
        if (stride <= MAX_STRIDE) {
            updatePatternCache(stride);
            strideFrequencies[stride]++;
        }
    }
    
    accessHistory.push_back(address);
    if (accessHistory.size() > PATTERN_CACHE_SIZE) {
        accessHistory.erase(accessHistory.begin());
    }
}

void AccessAnalyzer::updatePatternCache(uint64_t stride) {
    // 使用滑动窗口检测连续的访问模式
    if (accessHistory.size() >= 3) {
        size_t n = accessHistory.size();
        uint64_t stride1 = accessHistory[n-1] - accessHistory[n-2];
        uint64_t stride2 = accessHistory[n-2] - accessHistory[n-3];
        
        if (stride1 == stride2 && stride1 == stride) {
            // 检测到连续的相同步长，增加其权重
            strideFrequencies[stride] += 2;
        }
    }
}

void AccessAnalyzer::finalizeAnalysis() {
    calculateLocality();
    
    // 按频率排序找出主导模式
    std::vector<std::pair<uint64_t, uint32_t>> patterns(
        strideFrequencies.begin(), strideFrequencies.end());
    
    std::sort(patterns.begin(), patterns.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
        
    AccessStats stats;
    stats.patterns.reserve(patterns.size());
    
    uint32_t totalFreq = std::accumulate(patterns.begin(), patterns.end(), 0,
        [](uint32_t sum, const auto& p) { return sum + p.second; });
        
    for (const auto& p : patterns) {
        AccessPattern pattern;
        pattern.stride = p.first;
        pattern.frequency = p.second;
        pattern.probability = static_cast<double>(p.second) / totalFreq;
        stats.patterns.push_back(pattern);
    }
    
    if (!stats.patterns.empty()) {
        stats.dominantPattern = stats.patterns[0];
        if (stats.patterns.size() > 1) {
            stats.secondaryPattern = stats.patterns[1];
        }
    }
}

void AccessAnalyzer::calculateLocality() {
    spatialLocality = computeSpatialScore(accessHistory);
    temporalLocality = computeTemporalScore(accessHistory);
}

double AccessAnalyzer::computeSpatialScore(
    const std::vector<uint64_t>& accesses) const {
    if (accesses.size() < 2) return 1.0;
    
    double score = 0;
    double weight = 1.0;
    
    for (size_t i = 1; i < accesses.size(); ++i) {
        uint64_t dist = std::abs((int64_t)accesses[i] - (int64_t)accesses[i-1]);
        
        // 使用对数尺度评估空间局部性
        double localScore = 1.0 / (1.0 + std::log2(1 + dist));
        score += localScore * weight;
        weight *= 0.9; // 较新的访问权重更大
    }
    
    return score / (1 - std::pow(0.9, accesses.size() - 1));
}

double AccessAnalyzer::computeTemporalScore(
    const std::vector<uint64_t>& accesses) const {
    if (accesses.size() < 2) return 1.0;
    
    std::unordered_map<uint64_t, size_t> lastSeen;
    double score = 0;
    double weight = 1.0;
    double totalWeight = 0;
    
    for (size_t i = 0; i < accesses.size(); ++i) {
        uint64_t addr = accesses[i];
        auto it = lastSeen.find(addr);
        
        if (it != lastSeen.end()) {
            size_t timeDiff = i - it->second;
            // 使用指数衰减评估时间局部性
            score += weight * std::exp(-0.1 * timeDiff);
        }
        
        lastSeen[addr] = i;
        totalWeight += weight;
        weight *= 0.9; // 较新的访问权重更大
    }
    
    return score / totalWeight;
}

AccessAnalyzer::AccessStats AccessAnalyzer::getStatistics() const {
    AccessStats stats;
    stats.spatialLocality = spatialLocality;
    stats.temporalLocality = temporalLocality;
    return stats;
}

void MemoryProfiler::initializeRegion(const std::string& name,
                                    const std::string& scope,
                                    void* addr, size_t size) {
    RegionInfo region;
    region.name = name;
    region.scopeName = scope;
    region.baseAddr = reinterpret_cast<uint64_t>(addr);
    region.size = size;
    region.accessCount = 0;

    regions[name] = region;
    analyzers[name] = AccessAnalyzer();
}

void MemoryProfiler::recordAccess(const std::string& name, void* addr) {
    auto it = regions.find(name);
    if (it != regions.end()) {
        it->second.accessCount++;

        uint64_t address = reinterpret_cast<uint64_t>(addr);
        analyzers[name].recordAccess(address);
    }
}

void MemoryProfiler::finalizeRegion(const std::string& name) {
    auto it = regions.find(name);
    if (it != regions.end()) {
        analyzers[name].finalizeAnalysis();
        it->second.stats = analyzers[name].getStatistics();
    }
}

std::vector<MemoryProfiler::RegionInfo> MemoryProfiler::generateReport() const {
    std::vector<RegionInfo> report;
    report.reserve(regions.size());

    for (const auto& pair : regions) {
        report.push_back(pair.second);
    }

    // 按访问次数排序
    std::sort(report.begin(), report.end(),
        [](const RegionInfo& a, const RegionInfo& b) {
            return a.accessCount > b.accessCount;
        });

    return report;
}

// 辅助函数：生成访存特征报告
std::string generateAccessReport(const MemoryProfiler::RegionInfo& region) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "Memory Region: " << region.name << "\n"
       << "  Scope: " << region.scopeName << "\n"
       << "  Base Address: 0x" << std::hex << region.baseAddr << "\n"
       << "  Size: " << std::dec << region.size << " bytes\n"
       << "  Total Accesses: " << region.accessCount << "\n\n"
       << "Access Patterns:\n";

    const auto& stats = region.stats;
    if (!stats.patterns.empty()) {
        ss << "  Dominant Pattern:\n"
           << "    Stride: " << stats.dominantPattern.stride << " bytes\n"
           << "    Frequency: " << stats.dominantPattern.frequency << "\n"
           << "    Probability: " << (stats.dominantPattern.probability * 100) << "%\n";

        if (stats.patterns.size() > 1) {
            ss << "  Secondary Pattern:\n"
               << "    Stride: " << stats.secondaryPattern.stride << " bytes\n"
               << "    Frequency: " << stats.secondaryPattern.frequency << "\n"
               << "    Probability: " << (stats.secondaryPattern.probability * 100) << "%\n";
        }
    }

    ss << "\nLocality Metrics:\n"
       << "  Spatial Locality: " << (stats.spatialLocality * 100) << "%\n"
       << "  Temporal Locality: " << (stats.temporalLocality * 100) << "%\n";

    return ss.str();
}