// 探測（fix/halcon-accuracy，2026-10-13；不列進 tests.pro）：ADR-004 §5 的 B1–B3 在 HALCON 上的根因。
// 每種板子 × 每組 HalconSearchOptions：'auto' 選到的參數、模型點的分佈、整張影像的原始匹配（真／半格距／其他）、use case 的結果。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <halconcpp/HalconCpp.h>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "application/CancelToken.h"
#include "domain/image_buffer.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "infrastructure/backend_halcon/HalconBackend.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconPatternSearcher.h"
#include "infrastructure/backend_halcon/HalconTemplate.h"
#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

inline const char* aoiEnv(const char* name) {
    static std::map<std::string, std::string> cache;
    const QByteArray v = qgetenv(name);
    if (v.isEmpty()) return nullptr;
    return cache.emplace(name, v.toStdString()).first->second.c_str();
}

SyntheticBoardSpec abBoardSpec() {
    SyntheticBoardSpec s;
    s.width = 2592;
    s.height = 1944;
    s.bodyWidthPx = 56.0;
    s.bodyHeightPx = 24.0;
    s.pitchXPx = 84.0;
    s.pitchYPx = 60.0;
    s.originXPx = 120.0;
    s.originYPx = 110.0;
    s.cols = static_cast<int>((s.width - 2 * s.originXPx) / s.pitchXPx) + 1;
    s.rows = static_cast<int>((s.height - 2 * s.originYPx) / s.pitchYPx) + 1;
    s.noiseAmplitude = 4;
    s.blurRadius = 1;
    s.seed = 17;
    return s;
}

SyntheticBoardSpec baseSpec() {
    SyntheticBoardSpec s;
    s.width = 1280;
    s.height = 960;
    s.rows = 12;
    s.cols = 14;
    s.seed = 11;
    return s;
}

struct BoardCase {
    std::string name;
    SyntheticBoardSpec spec;
};

std::vector<BoardCase> boards() {
    std::vector<BoardCase> out;
    const char* only = aoiEnv("AOI_PROBE_BOARDS");
    const std::string filter = only != nullptr ? only : "";
    const auto add = [&](const char* name, SyntheticBoardSpec s) {
        if (filter.empty() || filter.find(name) != std::string::npos) out.push_back({name, s});
    };
    SyntheticBoardSpec b1 = abBoardSpec();
    b1.bodyLevel = 72;
    b1.padLevel = 84;
    add("B1", b1);
    SyntheticBoardSpec b2 = abBoardSpec();
    b2.blurRadius = 4;
    add("B2", b2);
    SyntheticBoardSpec b3 = baseSpec();
    b3.noiseAmplitude = 12;
    b3.blurRadius = 2;
    add("B3", b3);
    // 契約測試（tst_contract_end_to_end）原本的 8 種板子（1280×960）。
    const SyntheticBoardSpec base = baseSpec();
    add("c-clean", base);
    SyntheticBoardSpec cMissing = base;
    cMissing.missingFraction = 0.10;
    add("c-missing10", cMissing);
    SyntheticBoardSpec cNoisy = base;
    cNoisy.noiseAmplitude = 12;
    cNoisy.blurRadius = 1;
    add("c-noise12blur1", cNoisy);
    SyntheticBoardSpec cGlare = base;
    cGlare.glareFraction = 0.10;
    add("c-glare10", cGlare);
    SyntheticBoardSpec cTilt = base;
    cTilt.tiltDeg = 3.0;
    add("c-tilt3", cTilt);
    SyntheticBoardSpec cJitter = base;
    cJitter.positionJitterPx = 2.0;
    add("c-jitter2", cJitter);
    SyntheticBoardSpec b3ab = abBoardSpec();
    b3ab.noiseAmplitude = 12;
    b3ab.blurRadius = 2;
    add("B3ab", b3ab);
    add("noise4", abBoardSpec());
    SyntheticBoardSpec n25 = abBoardSpec();
    n25.noiseAmplitude = 25;
    add("noise25", n25);
    SyntheticBoardSpec glare = abBoardSpec();
    glare.glareFraction = 0.40;
    add("glare40", glare);
    SyntheticBoardSpec lcNoNoise = abBoardSpec();
    lcNoNoise.bodyLevel = 72;
    lcNoNoise.padLevel = 84;
    lcNoNoise.noiseAmplitude = 0;
    lcNoNoise.blurRadius = 0;
    add("B1nn", lcNoNoise);
    SyntheticBoardSpec lcNoBlur = abBoardSpec();
    lcNoBlur.bodyLevel = 72;
    lcNoBlur.padLevel = 84;
    lcNoBlur.blurRadius = 0;
    add("B1nb", lcNoBlur);
    return out;
}

HalconCpp::HTuple autoOr(int value) { return value > 0 ? HalconCpp::HTuple(static_cast<Hlong>(value)) : HalconCpp::HTuple("auto"); }

struct Variant {
    std::string name;
    HalconSearchOptions options;
    std::vector<double> contrastTuple;  // 非空：create_shape_model 的 Contrast 用這個 tuple（例如 {low, high, minSize}），模型自己建
    int minContrastOverride = -1;       // ≥ 0：MinContrast 用這個值（0 ＝ 'auto'）；和 contrastTuple 一起用時模型自己建
    int preSmoothRadius = 0;            // > 0：建模型與搜尋之前先把整張影像做 3 次方框模糊（半徑 r）
    int preGaussSize = 0;               // > 0：整張影像先用 HALCON 的 gauss_filter（大小）；和 preSmoothRadius 擇一
    int preBinomialSize = 0;            // > 0：整張影像先用 HALCON 的 binomial_filter（大小）
    int preMeanRadius = 0;              // > 0：整張影像先用 HALCON 的 mean_image（2r+1）三次（＝ box³）
    bool adaptHighNoise = false;        // adapt_shape_model_high_noise（估最低的金字塔層）
};

std::vector<Variant> variants() {
    std::vector<Variant> out;
    const char* only = aoiEnv("AOI_PROBE_VARIANTS");
    const std::string filter = only != nullptr ? only : "";
    const auto add = [&](const std::string& name, Variant v) {
        v.name = name;
        if (filter.empty() || filter.find(name) != std::string::npos) out.push_back(std::move(v));
    };
    const char* set = aoiEnv("AOI_PROBE_SET");
    const std::string which = set != nullptr ? set : "round1";
    if (which == "round1") {
        add("default", {});
        for (const int c : {4, 6, 8, 12, 20, 30}) {
            Variant v;
            v.options.contrast = c;
            add("contrast" + std::to_string(c), v);
        }
        for (const int m : {1, 2, 3, 5}) {
            Variant v;
            v.options.minContrast = m;
            add("minContrast" + std::to_string(m), v);
        }
        Variant g;
        g.options.greediness = 0.0;
        add("greedy0", g);
        Variant o;
        o.options.optimization = "none";
        add("optNone", o);
        Variant ls;
        ls.options.subPixel = "least_squares_high";
        add("lsHigh", ls);
        for (const int l : {1, 2}) {
            Variant v;
            v.options.numLevels = l;
            add("levels" + std::to_string(l), v);
        }
        return out;
    }
    if (which == "round10") {
        add("default", {});
        for (const int lowest : {2, 3}) {
            Variant v;
            v.options.wideEdgeLowestLevel = lowest;
            add("lowest" + std::to_string(lowest), v);
            Variant w = v;
            w.options.subPixel = "least_squares";
            add("lowest" + std::to_string(lowest) + "LS", w);
        }
        return out;
    }
    if (which == "round9") {
        const auto mk = [&](const std::string& name, int meanRadius, const char* subPixel, double corner = 0.5, std::vector<double> c = {}, int minC = -1) {
            Variant v;
            v.options.maxOverlap = 0.2;
            v.options.adaptiveSmoothing = false;
            v.options.subPixel = subPixel;
            v.options.cornerShiftPx = corner;
            v.preMeanRadius = meanRadius;
            v.contrastTuple = std::move(c);
            v.minContrastOverride = minC;
            add(name, v);
        };
        mk("m2-LS", 2, "least_squares");
        mk("m2-LS-c0.25", 2, "least_squares", 0.25);
        mk("m2-LS-c1.0", 2, "least_squares", 1.0);
        mk("m2-LS-h3-8s6m1", 2, "least_squares", 0.5, {3, 8, 6}, 1);
        mk("m2-LS-h3-8s6m1-c0.25", 2, "least_squares", 0.25, {3, 8, 6}, 1);
        mk("m2-lsH-h3-8s6m1", 2, "least_squares_high", 0.5, {3, 8, 6}, 1);
        mk("m2-interp-h3-8s6m1", 2, "interpolation", 0.5, {3, 8, 6}, 1);
        mk("m2-LS-h4-12s6m2", 2, "least_squares", 0.5, {4, 12, 6}, 2);
        mk("m2-LS-c6m2", 2, "least_squares", 0.5, {6}, 2);
        mk("m1-lsH", 1, "least_squares_high");
        mk("m1-LS", 1, "least_squares");
        mk("m1-lsH-c0.25", 1, "least_squares_high", 0.25);
        mk("m1-lsH-c1.0", 1, "least_squares_high", 1.0);
        mk("m1-lsH-h4-8s6m2", 1, "least_squares_high", 0.5, {4, 8, 6}, 2);
        mk("m1-LS-h4-8s6m2", 1, "least_squares", 0.5, {4, 8, 6}, 2);
        return out;
    }
    if (which == "round8") {
        const auto mk = [&](const std::string& name, int binomial, int meanRadius, const char* subPixel) {
            Variant v;
            v.options.maxOverlap = 0.2;
            v.options.adaptiveSmoothing = false;
            v.options.subPixel = subPixel;
            v.preBinomialSize = binomial;
            v.preMeanRadius = meanRadius;
            add(name, v);
        };
        mk("mean2x3-interp", 0, 2, "interpolation");
        mk("mean2x3-LS", 0, 2, "least_squares");
        mk("mean2x3-lsH", 0, 2, "least_squares_high");
        mk("mean1x3-lsH", 0, 1, "least_squares_high");
        mk("mean1x3-LS", 0, 1, "least_squares");
        mk("binom25-interp", 25, 0, "interpolation");
        mk("binom25-LS", 25, 0, "least_squares");
        mk("binom9-lsH", 9, 0, "least_squares_high");
        mk("binom9-LS", 9, 0, "least_squares");
        mk("binom13-lsH", 13, 0, "least_squares_high");
        return out;
    }
    if (which == "round7") {
        // 隔離「濾波器的種類」與「在 adapter 裡裁一塊再平滑」：整張影像在外面先平滑、adapter 的自適應關掉。
        const auto mk = [&](const std::string& name, int box, int gauss, const char* subPixel) {
            Variant v;
            v.options.maxOverlap = 0.2;
            v.options.adaptiveSmoothing = false;
            v.options.subPixel = subPixel;
            v.preSmoothRadius = box;
            v.preGaussSize = gauss;
            add(name, v);
        };
        add("default", {});
        mk("box2-interp", 2, 0, "interpolation");
        mk("box2-LS", 2, 0, "least_squares");
        mk("gauss11-interp", 0, 11, "interpolation");
        mk("gauss11-LS", 0, 11, "least_squares");
        mk("gauss9-interp", 0, 9, "interpolation");
        mk("gauss7-interp", 0, 7, "interpolation");
        mk("gauss7-lsH", 0, 7, "least_squares_high");
        mk("gauss5-lsH", 0, 5, "least_squares_high");
        mk("box1-lsH", 1, 0, "least_squares_high");
        return out;
    }
    if (which == "round6") {
        // 真的 builder（adaptiveSmoothing）：平滑的大小與門檻的變體。
        const auto mk = [&](const std::string& name, int medium, int wide, double interpWidth = 7.0, const char* subPixel = "least_squares_high", double w1 = 3.0, double w2 = 5.0) {
            Variant v;
            v.options.noisyRadius = medium;
            v.options.wideRadius = wide;
            (void)interpWidth;
            v.options.subPixel = subPixel;
            (void)w1;
            v.options.wideEdgeWidth = w2;
            add(name, v);
        };
        add("default", {});
        mk("m5w11", 5, 11);
        mk("m5w9", 5, 9);
        mk("m7w9", 7, 9);
        mk("m5w11i6", 5, 11, 6.0);
        mk("m7w11i6", 7, 11, 6.0);
        mk("m5w11i6LS", 5, 11, 6.0, "least_squares");
        mk("m5w11LS", 5, 11, 7.0, "least_squares");
        mk("m5w11VH", 5, 11, 7.0, "least_squares_very_high");
        mk("m5w9i6", 5, 9, 6.0);
        return out;
    }
    if (which == "round5") {
        const auto mk = [&](const std::string& name, int smooth, const char* subPixel = "least_squares", double maxOverlap = 0.2) {
            Variant v;
            v.options.maxOverlap = maxOverlap;
            v.preSmoothRadius = smooth;
            v.options.subPixel = subPixel;
            add(name, v);
        };
        mk("default", 0, "least_squares", 0.5);
        mk("ov0.2", 0);
        mk("C1-s1", 1);
        mk("C2-s2", 2);
        mk("C3-s2interp", 2, "interpolation");
        mk("C4-s1lsH", 1, "least_squares_high");
        mk("C5-s1interp", 1, "interpolation");
        return out;
    }
    if (which == "round4") {
        const auto mk = [&](const std::string& name, int smooth, const char* subPixel = "least_squares", double corner = 0.5, std::vector<double> c = {}, int minC = -1) {
            Variant v;
            v.options.maxOverlap = 0.2;
            v.preSmoothRadius = smooth;
            v.options.subPixel = subPixel;
            v.options.cornerShiftPx = corner;
            v.contrastTuple = std::move(c);
            v.minContrastOverride = minC;
            add(name, v);
        };
        mk("s1", 1);
        mk("s2", 2);
        mk("s3", 3);
        mk("s1interp", 1, "interpolation");
        mk("s2interp", 2, "interpolation");
        mk("s1lsH", 1, "least_squares_high");
        mk("s2lsH", 2, "least_squares_high");
        mk("s1step0.25", 1, "least_squares", 0.25);
        mk("s1step1.0", 1, "least_squares", 1.0);
        mk("s2step0.25", 2, "least_squares", 0.25);
        mk("s2step1.0", 2, "least_squares", 1.0);
        mk("s2c4m1", 2, "least_squares", 0.5, {4}, 1);
        mk("s2h3-8s6m1", 2, "least_squares", 0.5, {3, 8, 6}, 1);
        mk("s2h4-10s8m2", 2, "least_squares", 0.5, {4, 10, 8}, 2);
        mk("s3h3-8s6m1", 3, "least_squares", 0.5, {3, 8, 6}, 1);
        return out;
    }
    if (which == "round3") {
        add("default", {});
        Variant ov;
        ov.options.maxOverlap = 0.2;
        add("ov0.2", ov);
        Variant s1 = ov;
        s1.preSmoothRadius = 1;
        add("s1ov0.2", s1);
        Variant s2 = ov;
        s2.preSmoothRadius = 2;
        add("s2ov0.2", s2);
        Variant s1vh = s1;
        s1vh.options.subPixel = "least_squares_very_high";
        add("s1ov0.2lsVH", s1vh);
        Variant ovvh = ov;
        ovvh.options.subPixel = "least_squares_very_high";
        add("ov0.2lsVH", ovvh);
        Variant s1hn = s1;
        s1hn.adaptHighNoise = true;
        add("s1ov0.2hn", s1hn);
        Variant hn = ov;
        hn.adaptHighNoise = true;
        add("ov0.2hn", hn);
        Variant s1h = s1;
        s1h.contrastTuple = {4, 8, 6};
        s1h.minContrastOverride = 2;
        add("s1ov0.2h4-8s6m2", s1h);
        Variant s1h2 = s1;
        s1h2.contrastTuple = {10, 30, 10};
        s1h2.minContrastOverride = 8;
        add("s1ov0.2h10-30s10m8", s1h2);
        Variant s1l2 = s1;
        s1l2.options.numLevels = 2;
        add("s1ov0.2levels2", s1l2);
        Variant s1md = s1;
        s1md.options.subPixel = "least_squares";
        add("s1ov0.2", s1md);
        return out;
    }
    // round2：對比的 tuple（hysteresis ＋ min_size）、MinContrast 明確給、MaxOverlap、預先平滑。
    add("default", {});
    const auto tuple = [&](const std::string& name, std::vector<double> c, int minC, double maxOverlap = 0.5, int smooth = 0) {
        Variant v;
        v.contrastTuple = std::move(c);
        v.minContrastOverride = minC;
        v.options.maxOverlap = maxOverlap;
        v.preSmoothRadius = smooth;
        add(name, v);
    };
    // 單一對比 ＋ 明確的 MinContrast
    tuple("c8m2", {8}, 2);
    tuple("c8m3", {8}, 3);
    tuple("c6m2", {6}, 2);
    tuple("c5m2", {5}, 2);
    tuple("c12m3", {12}, 3);
    tuple("c20m5", {20}, 5);
    // hysteresis（low, high, min_size）
    tuple("h4-8s6m2", {4, 8, 6}, 2);
    tuple("h5-10s8m2", {5, 10, 8}, 2);
    tuple("h4-10s10m3", {4, 10, 10}, 3);
    tuple("h6-16s10m3", {6, 16, 10}, 3);
    tuple("h8-20s10m5", {8, 20, 10}, 5);
    tuple("h10-30s10m8", {10, 30, 10}, 8);
    tuple("h12-38s10m12", {12, 38, 10}, 12);
    tuple("h15-38s10m20", {15, 38, 10}, 20);
    tuple("h15-38s10mA", {15, 38, 10}, 0);
    // MaxOverlap
    {
        Variant v;
        v.options.maxOverlap = 0.3;
        add("ov0.3", v);
        v.options.maxOverlap = 0.2;
        add("ov0.2", v);
    }
    tuple("h5-10s8m2ov0.3", {5, 10, 8}, 2, 0.3);
    tuple("h8-20s10m5ov0.3", {8, 20, 10}, 5, 0.3);
    // 預先平滑（半徑 1 ≈ σ 1.4）
    {
        Variant v;
        v.preSmoothRadius = 1;
        add("smooth1", v);
    }
    tuple("smooth1h4-8s6m2", {4, 8, 6}, 2, 0.5, 1);
    tuple("smooth1h15-38s10m20", {15, 38, 10}, 20, 0.5, 1);
    tuple("smooth1h10-30s10m8", {10, 30, 10}, 8, 0.5, 1);
    return out;
}

// 3 次方框模糊（和 synthetic_board 的模糊同一種）：整張 Gray8 影像的複本。
ImageBuffer smoothed(const ImageView& view, int r) {
    ImageBuffer out(view.width, view.height, PixelFormat::Gray8);
    std::vector<std::uint8_t> gray(static_cast<std::size_t>(view.width) * static_cast<std::size_t>(view.height));
    for (int y = 0; y < view.height; ++y)
        std::memcpy(gray.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.width), view.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.strideBytes), static_cast<std::size_t>(view.width));
    const auto boxLine = [](const std::uint8_t* in, std::uint8_t* o, int n, std::ptrdiff_t step, int radius) {
        const int window = 2 * radius + 1;
        auto at = [&](int i) { return static_cast<int>(in[static_cast<std::ptrdiff_t>(std::clamp(i, 0, n - 1)) * step]); };
        int sum = 0;
        for (int i = -radius; i <= radius; ++i) sum += at(i);
        for (int i = 0; i < n; ++i) {
            o[static_cast<std::ptrdiff_t>(i) * step] = static_cast<std::uint8_t>((sum + window / 2) / window);
            sum += at(i + radius + 1) - at(i - radius);
        }
    };
    std::vector<std::uint8_t> tmp(gray.size());
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < view.height; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(view.width);
            boxLine(gray.data() + row, tmp.data() + row, view.width, 1, r);
        }
        for (int x = 0; x < view.width; ++x) boxLine(tmp.data() + x, gray.data() + x, view.height, view.width, r);
    }
    for (int y = 0; y < view.height; ++y) std::memcpy(out.row(y), gray.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.width), static_cast<std::size_t>(view.width));
    return out;
}

// 自己建模型（照 HalconTemplateBuilder 的做法：整張影像零複製、ReduceDomain 到樣本、角度步進與餘量相同），Contrast 與 MinContrast 自訂。
Result<TemplatePtr> buildCustom(const ImageView& image, const Rect& sample, const SearchParams& params, const Variant& v) {
    try {
        const int x0 = static_cast<int>(std::lround(sample.x)), y0 = static_cast<int>(std::lround(sample.y));
        const int width = static_cast<int>(std::lround(sample.w)), height = static_cast<int>(std::lround(sample.h));
        const HalconGray gray = toHalconGray(image, PixelRect{0, 0, image.width, image.height}, false);
        HalconCpp::HRegion domain;
        domain.GenRectangle1(y0, x0, y0 + height - 1, x0 + width - 1);
        const HalconCpp::HImage reduced = gray.image.ReduceDomain(domain);
        const double stepDeg = radiansToDegrees(std::atan2(v.options.cornerShiftPx, 0.5 * std::hypot(width, height)));
        const double margin = v.options.angleMarginSteps * stepDeg;
        HalconCpp::HTuple contrast;
        if (v.contrastTuple.empty()) contrast = autoOr(v.options.contrast);
        else for (const double c : v.contrastTuple) contrast.Append(HalconCpp::HTuple(static_cast<Hlong>(c)));
        const HalconCpp::HTuple minContrast = v.minContrastOverride >= 0 ? autoOr(v.minContrastOverride) : autoOr(v.options.minContrast);
        HalconCpp::HShapeModel model;
        model.CreateShapeModel(reduced, autoOr(v.options.numLevels), degreesToRadians(params.angleMinDeg - margin),
                               degreesToRadians(params.angleMaxDeg - params.angleMinDeg + 2.0 * margin), HalconCpp::HTuple(degreesToRadians(stepDeg)),
                               HalconCpp::HTuple(v.options.optimization.c_str()), v.options.metric.c_str(), contrast, minContrast);
        double a0 = 0, ae = 0, as = 0;
        HalconCpp::HTuple smin, smax, sstep;
        HalconCpp::HString metric;
        Hlong minC = 0;
        const HalconCpp::HTuple levels = model.GetShapeModelParams(&a0, &ae, &as, &smin, &smax, &sstep, &metric, &minC);
        return TemplatePtr(std::make_shared<const HalconTemplate>(Size2D{static_cast<double>(width), static_cast<double>(height)}, params.angleMinDeg,
                                                                  params.angleMaxDeg, std::move(model), static_cast<int>(levels[0].L())));
    } catch (const HalconCpp::HException& e) {
        return makeError(ErrorCode::BackendFailed, std::string("create_shape_model: ") + e.ErrorMessage().Text());
    }
}

// use case 用「已經建好的範本」（探測要讓 use case 用和原始搜尋同一個模型）。
class FixedTemplateBuilder final : public ITemplateBuilder {
public:
    explicit FixedTemplateBuilder(TemplatePtr t) : t_(std::move(t)) {}
    std::string_view backendId() const noexcept override { return t_->backendId(); }
    Result<TemplatePtr> build(const ImageView&, const Rect&, const SearchParams&, const CancelToken&) override { return t_; }
private:
    TemplatePtr t_;
};

struct Truth {
    std::vector<Point2D> centers;  // present parts
    double pitchX = 0.0;
};

// 匹配的分類：離最近的元件 ≤ 3 px ＝ 真；離「兩個橫向相鄰元件的中點」≤ 3 px ＝ 半格距；其餘 ＝ 其他。
struct RawStats {
    std::size_t total = 0, trueHits = 0, halfPitch = 0, other = 0;
    std::vector<double> trueScores, halfScores, otherScores, trueErrors, trueAngles;
    std::size_t partsHit = 0;
};

RawStats classify(const SyntheticBoard& board, const std::vector<Match>& matches, Point2D offset, double minScore) {
    RawStats s;
    std::vector<bool> hit(board.parts.size(), false);
    for (const Match& m : matches) {
        if (m.score < minScore) continue;
        ++s.total;
        double best = std::numeric_limits<double>::max();
        std::size_t bi = 0;
        for (std::size_t i = 0; i < board.parts.size(); ++i) {
            const SyntheticPart& p = board.parts[i];
            if (!p.present) continue;
            const double d = distance(m.pose.position(), {p.pose.x + offset.x, p.pose.y + offset.y});
            if (d < best) {
                best = d;
                bi = i;
            }
        }
        if (best <= 3.0) {
            ++s.trueHits;
            s.trueScores.push_back(m.score);
            s.trueErrors.push_back(best);
            s.trueAngles.push_back(std::abs(m.pose.angleDeg));
            hit[bi] = true;
            continue;
        }
        // 半格距：x 離元件中心 ≈ pitchX/2，y 差 ≤ 3
        const SyntheticPart& p = board.parts[bi];
        const double dx = std::abs(m.pose.x - (p.pose.x + offset.x));
        const double dy = std::abs(m.pose.y - (p.pose.y + offset.y));
        if (dy <= 3.0 && std::abs(dx - board.spec.pitchXPx * 0.5) <= 3.0) {
            ++s.halfPitch;
            s.halfScores.push_back(m.score);
        } else {
            ++s.other;
            s.otherScores.push_back(m.score);
        }
    }
    for (const bool h : hit) s.partsHit += h ? 1 : 0;
    return s;
}

QString range(std::vector<double> v) {
    if (v.empty()) return QStringLiteral("-");
    std::sort(v.begin(), v.end());
    const double p95 = v[static_cast<std::size_t>(0.95 * static_cast<double>(v.size() - 1))];
    return QStringLiteral("min %1 med %2 p95 %3 max %4").arg(v.front(), 0, 'f', 3).arg(v[v.size() / 2], 0, 'f', 3).arg(p95, 0, 'f', 3).arg(v.back(), 0, 'f', 3);
}

// use case 的評估（照 tst_contract_end_to_end）。
struct Outcome {
    std::size_t present = 0, foundPresent = 0, falsePositives = 0, duplicates = 0, missing = 0, missingInferred = 0;
    double maxErrorPx = 0.0, p95ErrorPx = 0.0, worstAngle = 0.0;
    std::map<std::string, std::size_t> bySource, falseBySource, dupBySource;
};

const char* sourceName(CandidateSource s) {
    switch (s) {
        case CandidateSource::Sample: return "sample";
        case CandidateSource::Seed: return "seed";
        case CandidateSource::Verified: return "verified";
        case CandidateSource::Predicted: return "predicted";
    }
    return "?";
}

Point2D sampleOffset(const SyntheticBoard& board, const SampleProposal& proposal) {
    const Candidate& sample = proposal.candidates.front();
    const SyntheticPart* nearest = nullptr;
    double best = std::numeric_limits<double>::max();
    for (const SyntheticPart& p : board.parts) {
        const double d = distance(sample.pose.position(), p.pose.position());
        if (p.present && d < best) {
            best = d;
            nearest = &p;
        }
    }
    if (nearest == nullptr) return {};
    return {sample.pose.x - nearest->pose.x, sample.pose.y - nearest->pose.y};
}

Outcome evaluate(const SyntheticBoard& board, const SampleProposal& proposal, double tolerance = 3.0) {
    Outcome o;
    std::vector<double> errors;
    const Point2D offset = proposal.candidates.empty() ? Point2D{} : sampleOffset(board, proposal);
    for (const SyntheticPart& p : board.parts) {
        const Point2D expected{p.pose.x + offset.x, p.pose.y + offset.y};
        const Candidate* nearest = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const Candidate& c : proposal.candidates) {
            const double d = distance(c.pose.position(), expected);
            if (d < best) {
                best = d;
                nearest = &c;
            }
        }
        const bool isNear = nearest != nullptr && best <= tolerance;
        if (p.present) {
            ++o.present;
            if (isNear && nearest->hasScore()) {
                ++o.foundPresent;
                errors.push_back(best);
                o.maxErrorPx = std::max(o.maxErrorPx, best);
            }
        } else {
            ++o.missing;
            if (isNear && nearest->source == CandidateSource::Predicted) ++o.missingInferred;
        }
    }
    std::vector<std::size_t> hits(board.parts.size(), 0);
    for (const Candidate& c : proposal.candidates) {
        ++o.bySource[sourceName(c.source)];
        if (!c.hasScore()) continue;
        o.worstAngle = std::max(o.worstAngle, std::abs(c.pose.angleDeg));
        std::size_t nearestPart = board.parts.size();
        double nearestDistance = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < board.parts.size(); ++i) {
            const SyntheticPart& p = board.parts[i];
            const double d = distance(c.pose.position(), {p.pose.x + offset.x, p.pose.y + offset.y});
            if (p.present && d < nearestDistance) {
                nearestDistance = d;
                nearestPart = i;
            }
        }
        if (nearestDistance <= tolerance) {
            if (++hits[nearestPart] > 1) ++o.dupBySource[sourceName(c.source)];
        } else {
            ++o.falsePositives;
            ++o.falseBySource[sourceName(c.source)];
        }
    }
    for (const std::size_t n : hits) o.duplicates += n > 1 ? n - 1 : 0;
    if (!errors.empty()) {
        std::sort(errors.begin(), errors.end());
        o.p95ErrorPx = errors[static_cast<std::size_t>(0.95 * static_cast<double>(errors.size() - 1))];
    }
    return o;
}

QString mapText(const std::map<std::string, std::size_t>& m) {
    QString s;
    for (const auto& [k, v] : m) s += QStringLiteral("%1=%2 ").arg(QString::fromStdString(k)).arg(v);
    return s.trimmed();
}


}  // namespace

class ProbeHalconAccuracy : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        const Result<Backend> be = makeHalconBackend();
        if (!be.ok()) QSKIP(qPrintable(QString::fromStdString(be.error().message)));
        setHalconThreads(15);
    }

    void probe() {
        SearchParams params;
        params.minScore = 0.70;
        params.angleMinDeg = -5.0;
        params.angleMaxDeg = 5.0;
        const double rawMin = aoiEnv("AOI_PROBE_RAWMIN") != nullptr ? std::atof(aoiEnv("AOI_PROBE_RAWMIN")) : 0.30;
        for (const BoardCase& bc : boards()) {
            const SyntheticBoard board = makeSyntheticBoard(bc.spec);
            QVERIFY(board.image.valid());
            const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
            qInfo("==== board %s: %dx%d, parts %zu, sample (%g,%g,%g,%g), body %gx%g, pitch %gx%g, levels bg %d body %d pad %d, noise %d blur %d",
                  bc.name.c_str(), board.image.width, board.image.height, board.presentCount(), sample.x, sample.y, sample.w, sample.h,
                  bc.spec.bodyWidthPx, bc.spec.bodyHeightPx, bc.spec.pitchXPx, bc.spec.pitchYPx, bc.spec.backgroundLevel, bc.spec.bodyLevel,
                  bc.spec.padLevel, bc.spec.noiseAmplitude, bc.spec.blurRadius);

            // 樣本的邊有多寬：灰階範圍（p95 − p5）／最大梯度（3×3 Sobel，整數）。模糊越重，梯度峰越低、比值越大。
            {
                const int x0 = static_cast<int>(sample.x), y0 = static_cast<int>(sample.y), w = static_cast<int>(sample.w), h = static_cast<int>(sample.h);
                std::vector<int> values;
                double maxGrad = 0.0;
                std::vector<double> grads;
                const auto px = [&](int x, int y) { return static_cast<int>(board.image.data[static_cast<std::size_t>(y) * static_cast<std::size_t>(board.image.strideBytes) + static_cast<std::size_t>(x)]); };
                for (int y = y0; y < y0 + h; ++y)
                    for (int x = x0; x < x0 + w; ++x) {
                        values.push_back(px(x, y));
                        const double gx = (px(x + 1, y - 1) + 2 * px(x + 1, y) + px(x + 1, y + 1)) - (px(x - 1, y - 1) + 2 * px(x - 1, y) + px(x - 1, y + 1));
                        const double gy = (px(x - 1, y + 1) + 2 * px(x, y + 1) + px(x + 1, y + 1)) - (px(x - 1, y - 1) + 2 * px(x, y - 1) + px(x + 1, y - 1));
                        const double g = std::hypot(gx, gy) / 8.0;
                        grads.push_back(g);
                        maxGrad = std::max(maxGrad, g);
                    }
                std::sort(values.begin(), values.end());
                std::sort(grads.begin(), grads.end());
                const int p5 = values[values.size() / 20], p95 = values[values.size() * 19 / 20];
                const double g99 = grads[grads.size() * 99 / 100], g90 = grads[grads.size() * 9 / 10];
                qInfo("  sample: gray p5 %d p95 %d range %d | grad max %.2f p99 %.2f p90 %.2f | edge width range/p99 %.2f, range/max %.2f", p5, p95, p95 - p5, maxGrad, g99, g90,
                      (p95 - p5) / std::max(1e-9, g99), (p95 - p5) / std::max(1e-9, maxGrad));
            }
            // 'auto' 會選到什麼（determine_shape_model_params，樣本的區域）。
            try {
                const HalconGray gray = toHalconGray(board.image, PixelRect{0, 0, board.image.width, board.image.height}, false);
                HalconCpp::HRegion domain;
                domain.GenRectangle1(sample.y, sample.x, sample.bottom() - 1, sample.right() - 1);
                const HalconCpp::HImage reduced = gray.image.ReduceDomain(domain);
                HalconCpp::HTuple values;
                const HalconCpp::HTuple names = reduced.DetermineShapeModelParams(
                    HalconCpp::HTuple("auto"), degreesToRadians(-5.0), degreesToRadians(10.0), HalconCpp::HTuple(1.0), HalconCpp::HTuple(1.0), "auto",
                    "use_polarity", HalconCpp::HTuple("auto"), HalconCpp::HTuple("auto"),
                    HalconCpp::HTuple("num_levels").TupleConcat("contrast_hyst").TupleConcat("min_size").TupleConcat("min_contrast").TupleConcat("optimization").TupleConcat("angle_step"),
                    &values);
                QString text;
                for (Hlong i = 0; i < names.Length(); ++i) {
                    text += QString::fromLatin1(names[i].S().Text()) + QLatin1Char('=');
                    text += static_cast<int>(values[i].Type()) == static_cast<int>(HalconCpp::eTupleTypeString) ? QString::fromLatin1(values[i].S().Text()) : QString::number(values[i].D());
                    text += QLatin1Char(' ');
                }
                qInfo("  auto params: %s", qPrintable(text));
            } catch (const HalconCpp::HException& e) {
                qInfo("  determine_shape_model_params failed: %s", e.ErrorMessage().Text());
            }

            for (const Variant& v : variants()) {
                HalconTemplateBuilder builder(v.options);
                HalconPatternSearcher searcher(v.options);
                const ImageBuffer smoothedImage = v.preSmoothRadius > 0 ? smoothed(board.image, v.preSmoothRadius) : ImageBuffer();
                ImageView image = v.preSmoothRadius > 0 ? smoothedImage.view() : board.image;
                if (v.preGaussSize > 0 || v.preBinomialSize > 0 || v.preMeanRadius > 0) {
                    const HalconGray whole = toHalconGray(board.image, PixelRect{0, 0, board.image.width, board.image.height}, false);
                    HalconCpp::HImage filtered = whole.image;
                    if (v.preGaussSize > 0) filtered = filtered.GaussFilter(v.preGaussSize);
                    if (v.preBinomialSize > 0) filtered = filtered.BinomialFilter(v.preBinomialSize, v.preBinomialSize);
                    for (int pass = 0; pass < (v.preMeanRadius > 0 ? 3 : 0); ++pass) filtered = filtered.MeanImage(2 * v.preMeanRadius + 1, 2 * v.preMeanRadius + 1);
                    image = viewOfHalconImage(filtered, board.image.umPerPixel);
                    QVERIFY(image.valid());
                }
                const bool custom = !v.contrastTuple.empty() || v.minContrastOverride >= 0;
                const Result<TemplatePtr> built = custom ? buildCustom(image, sample, params, v) : builder.build(image, sample, params, CancelToken());
                if (!built.ok()) {
                    qInfo("  [%s] build failed: %s", v.name.c_str(), built.error().message.c_str());
                    continue;
                }
                const auto* t = dynamic_cast<const HalconTemplate*>(built.value().get());
                QVERIFY(t != nullptr);
                if (!t->hasModel()) {
                    qInfo("  [%s] no model (sample problem)", v.name.c_str());
                    continue;
                }
                if (v.adaptHighNoise) {
                    try {
                        const HalconGray g = toHalconGray(image, PixelRect{0, 0, image.width, image.height}, false);
                        HalconCpp::HRegion around;
                        around.GenRectangle1(std::max(0.0, sample.y - 40.0), std::max(0.0, sample.x - 50.0), sample.bottom() + 40.0, sample.right() + 50.0);
                        HalconCpp::HShapeModel m = t->model();
                        const HalconCpp::HDict d = m.AdaptShapeModelHighNoise(g.image.ReduceDomain(around), HalconCpp::HTuple("all"));
                        const HalconCpp::HTuple keys = d.GetDictParam("keys", HalconCpp::HTuple());
                        QString text;
                        for (Hlong i = 0; i < keys.Length(); ++i) {
                            const HalconCpp::HTuple val = d.GetDictTuple(HalconCpp::HTuple(keys[i]));
                            text += QString::fromLatin1(keys[i].S().Text()) + QLatin1Char('=') + QString::fromLatin1(val.ToString().Text()) + QLatin1Char(' ');
                        }
                        qInfo("  [%s] adapt_shape_model_high_noise: %s", v.name.c_str(), qPrintable(text));
                    } catch (const HalconCpp::HException& e) {
                        qInfo("  [%s] adapt_shape_model_high_noise failed: %s", v.name.c_str(), e.ErrorMessage().Text());
                    }
                }
                // 模型參數與每層的點數（依帶分：主體上下邊、電極外側、電極內側、其他）。
                const bool mapWanted = aoiEnv("AOI_PROBE_MAP") != nullptr;
                std::vector<std::string> map(41, std::string(73, '.'));
                QString levelsText;
                try {
                    double a0 = 0, ae = 0, as = 0;
                    HalconCpp::HTuple smin, smax, sstep;
                    HalconCpp::HString metric;
                    Hlong minC = 0;
                    const HalconCpp::HTuple levels = t->model().GetShapeModelParams(&a0, &ae, &as, &smin, &smax, &sstep, &metric, &minC);
                    const int numLevels = static_cast<int>(levels[0].L());
                    levelsText = QStringLiteral("levels %1 angleStep %2 deg minContrast %3 |").arg(numLevels).arg(radiansToDegrees(as), 0, 'f', 3).arg(minC);
                    const double hw = bc.spec.bodyWidthPx * 0.5, hh = bc.spec.bodyHeightPx * 0.5;
                    const double padIn = hw - bc.spec.bodyWidthPx * 0.22;
                    for (int lv = 1; lv <= numLevels; ++lv) {
                        const HalconCpp::HXLDCont contours = t->model().GetShapeModelContours(lv);
                        const double scale = std::pow(2.0, lv - 1);
                        std::size_t total = 0, bodyH = 0, padOuter = 0, padInner = 0, padH = 0, otherPts = 0;
                        for (Hlong i = 1; i <= contours.CountObj(); ++i) {
                            HalconCpp::HTuple rows, cols;
                            contours.SelectObj(i).GetContourXld(&rows, &cols);
                            for (Hlong k = 0; k < rows.Length(); ++k) {
                                const double x = cols[k].D() * scale, y = rows[k].D() * scale;  // 模型座標（相對於參考點）
                                if (lv == 1 && mapWanted) {
                                    const int mx = static_cast<int>(std::lround(x)) + 36, my = static_cast<int>(std::lround(y)) + 20;
                                    if (mx >= 0 && mx < 73 && my >= 0 && my < 41) map[static_cast<std::size_t>(my)][static_cast<std::size_t>(mx)] = '#';
                                }
                                ++total;
                                const double tol = 1.5 * scale;
                                if (std::abs(std::abs(y) - hh) <= tol && std::abs(x) < padIn) ++bodyH;
                                else if (std::abs(std::abs(y) - hh) <= tol) ++padH;
                                else if (std::abs(std::abs(x) - hw) <= tol) ++padOuter;
                                else if (std::abs(std::abs(x) - padIn) <= tol) ++padInner;
                                else ++otherPts;
                            }
                        }
                        levelsText += QStringLiteral(" L%1: %2 pts (bodyTopBottom %3, padTopBottom %4, padOuter %5, padInner %6, other %7)")
                                          .arg(lv).arg(total).arg(bodyH).arg(padH).arg(padOuter).arg(padInner).arg(otherPts);
                    }
                } catch (const HalconCpp::HException& e) {
                    levelsText = QStringLiteral("model query failed: ") + QString::fromLatin1(e.ErrorMessage().Text());
                }
                qInfo("  [%s] model: %s | tuning: edge width %.2f, noise ratio %.4f, radius %d, subpixel %s, lowest %d", v.name.c_str(), qPrintable(levelsText), t->tuning().edgeWidthPx, t->tuning().noiseRatio, t->tuning().smoothRadius, t->tuning().subPixel.c_str(), t->tuning().lowestLevel);
                if (mapWanted) for (const std::string& line : map) qInfo("      %s", line.c_str());

                // 原始匹配：整張影像、分數下限 rawMin。
                SearchParams raw = params;
                raw.minScore = rawMin;
                const Result<std::vector<Match>> found = searcher.find(image, *t, SearchRegion::wholeImage(), raw, CancelToken());
                if (!found.ok()) {
                    qInfo("  [%s] find failed: %s", v.name.c_str(), found.error().message.c_str());
                    continue;
                }
                const Point2D offset{sample.center().x - board.parts[middlePresentIndex(board)].pose.x, 0.0};
                // offset：樣本框中心與元件中心的固定偏移（x、y 各自算）
                Point2D off{};
                {
                    double best = std::numeric_limits<double>::max();
                    for (const SyntheticPart& p : board.parts) {
                        const double d = distance(sample.center(), p.pose.position());
                        if (p.present && d < best) {
                            best = d;
                            off = {sample.center().x - p.pose.x, sample.center().y - p.pose.y};
                        }
                    }
                }
                (void)offset;
                for (const double threshold : {rawMin, 0.70}) {
                    const RawStats s = classify(board, found.value(), off, threshold);
                    qInfo("  [%s] raw >= %.2f: %zu matches: true %zu (parts hit %zu/%zu; score %s; err %s; |angle| %s) halfPitch %zu (score %s) other %zu (score %s)",
                          v.name.c_str(), threshold, s.total, s.trueHits, s.partsHit, board.presentCount(), qPrintable(range(s.trueScores)),
                          qPrintable(range(s.trueErrors)), qPrintable(range(s.trueAngles)), s.halfPitch, qPrintable(range(s.halfScores)), s.other, qPrintable(range(s.otherScores)));
                }

                // use case。
                FixedTemplateBuilder fixedBuilder(built.value());
                GenerateObjectsFromSample uc(fixedBuilder, searcher, nullptr);
                SampleRequest request;
                request.image = image;
                request.sample = sample;
                request.search = params;
                const Result<SampleProposal> r = uc.run(request, CancelToken());
                if (!r.ok()) {
                    qInfo("  [%s] use case failed: %s", v.name.c_str(), r.error().message.c_str());
                    continue;
                }
                const SampleProposal& p = r.value();
                const Outcome o = evaluate(board, p);
                qInfo("  [%s] use case: present %zu found %zu false %zu dup %zu | missing %zu inferred %zu | err p95 %.3f max %.3f | angle max %.3f | lattice %d (%zu) holes %zu | by source %s | false by %s | dup by %s | %.1f ms (template %.1f seed %.1f verify %.1f)",
                      v.name.c_str(), o.present, o.foundPresent, o.falsePositives, o.duplicates, o.missing, o.missingInferred, o.p95ErrorPx, o.maxErrorPx,
                      o.worstAngle, p.grid.found ? 1 : 0, p.grid.lattices, p.grid.holes, qPrintable(mapText(o.bySource)), qPrintable(mapText(o.falseBySource)),
                      qPrintable(mapText(o.dupBySource)), p.timings.totalMs, p.timings.templateMs, p.timings.seedMs, p.timings.verifyMs);
            }
        }
    }
};

QTEST_GUILESS_MAIN(ProbeHalconAccuracy)
#include "probe_halcon_accuracy.moc"
