#pragma once

#include "common.hpp"

#include <array>
#include <vector>

namespace kg {

struct VisualImageFeatures {
    float brightness{};   // mittlere Luminanz [0..1]
    float contrast{};     // Luminanz-Standardabweichung, normalisiert [0..1]
    float saturation{};   // mittlere RGB-Saettigung [0..1]
    float warmth{0.5f};   // 0=kuehl/blau, 1=warm/rot
    float edgeDensity{};  // lokale Detail-/Kantendichte [0..1]
    float landscape{};    // 1 bei Querformat, 0 bei Hochformat, 0.5 quadratisch
    float darkness{};     // 1-brightness
    float vividness{};    // kombinierte Farb-/Kontrastwirkung [0..1]
};

struct VisualImageProfile {
    fs::path path;
    VisualImageFeatures features{};
};

class ImageAnalyzer {
public:
    [[nodiscard]] bool Analyze(const fs::path& path, VisualImageProfile& out, std::wstring& error) const;
    [[nodiscard]] bool Analyze(const std::vector<fs::path>& paths, std::vector<VisualImageProfile>& out,
                               std::wstring& error) const;
};

} // namespace kg
