// ADR-031 的差異測試（R05.1）：同一個執行檔裡同時有 Rust 的實作（domain 的公開標頭，AOI_DOMAIN_IMPL=rust）與
// C++ 的實作（aoi::legacy，aoi_domain_legacy 程式庫），用固定 seed 的隨機輸入逐筆比對。
//
// 比對的規則：
//   - 索引、布林、個數（NMS 留下哪些框、OverlapIndex 的回答與大小）：完全相同。
//   - 重疊面積與比例（浮點數）：相對誤差 ≤ 1e-12（kRelTol）。理由：Rust 與 MSVC 對 f64 都遵守 IEEE 754，Rust 的實作也照 C++ 的
//     運算順序寫（不合併成 fma），所以四則運算與 sqrt 的結果一位不差；可能不同的只有 sin／cos（MSVC 可能用編譯器內建的版本、
//     Rust 呼叫 CRT 的 cos/sin），差在最後一位（約 1e-16 的相對誤差），經過裁切的二、三十次運算放大到 1e-14 左右。
//     1e-12 還有 100 倍的餘裕，又比任何影響 NMS 判斷的差異小很多（判斷只在重疊比例剛好等於容差時才會翻轉；那種輸入另外列出）。
//     實際有幾筆不是一位不差，印在 log（qInfo），容差不可以為了讓測試通過而放寬。
//   - NaN：兩邊要同樣是 NaN 或同樣不是。
//
// R05.2 擴充到晶格（累加器、空間格子、分群、找基底、連通、接受規則、估計、預測）：
//   - 索引、格子、個數、布林、沒有晶格的原因、內點與格子的對應：完全相同（任何一個不同就是行為不同）。
//   - 浮點數（基底、殘差、信心、正規方程的總和、鄰居的距離、群的中心）：一樣用 agree()（相對 1e-12）。理由和上面相同：
//     Rust 照 C++ 的運算順序寫（不合併成 fma），四則運算與 sqrt 一位不差；可能差在最後一位的只有 CRT 的函式
//     （信心的 exp、分群排序的 hypot），差異經過幾十次運算也遠小於 1e-12。每一組比對都把一位不差的個數印在 log。
//     如果 exp 或 hypot 真的差了一位，會先在「整數完全相同」那一條失敗（門檻比較翻轉），不會被容差蓋掉。
//   - 掃描的情境：tst_lattice_scenarios 的 1 029 種（同樣的 seed）＋兩組晶格、一排、間距差 20 倍、散佈的點、很少的點、
//     重複與非有限值的點、別的選項與分群策略。
//
// R05.3 擴充到比對區與自動擷取（PixelMask::count、runsFromMask、maskFromRuns、matchSpecFor、matchSettingsFromPixels、
// pickBrightBlob、growMask）：
//   - 遮罩（位置、大小、每個 bit）、runs、Otsu 的門檻、找不找得到：完全相同。
//   - 浮點數（matchSpecFor 的矩形、matchSettingsFromPixels 的 µm）：agree()，理由同上（只有四則運算，實際一位不差，印在 log）。
//   - 輸入：五種像素格式、stride 有填充、Gray16 的列起點不對齊、各種有效位元數；within 部分在影像外、整個在外、空的、NaN；
//     點擊在亮塊上、亮塊外、NaN、很遠；遮罩的寬高 ≤ 0、bits 的個數不對、不合法的 runs；校正值 0、負的、NaN、∞。
//
// R05.4 擴充到重複性的樣本統計（repeatability::summarize）：
//   - 個數、expected、complete()、每個 optional 有沒有值：完全相同。
//   - 浮點數（最小、最大、平均、Range、標準差、P/T）：agree()，理由同上（四則運算與 sqrt，實際一位不差，印在 log）。
//   - 輸入：長度 0–64；像真實量測的 µm、很大（差與平方和溢位）、次正規數、全部相同、±0、混著 NaN 與 ±∞；
//     SPEC 沒有、0、−0、負的、NaN、±∞、很小、很大。有 AOI_REPEATABILITY_DATA 時另外拿真實的 BOX LEVEL 報表
//     （那個資料夾與底下每個子資料夾各算一組，每個 (Box, 欄) 在每份 CSV 的值是一次 summarize 的輸入）。
#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/feature_extract.h"
#include "domain/ffi/aoi_domain_rs.h"
#include "domain/image_view.h"
#include "domain/legacy/feature_extract_legacy.h"
#include "domain/legacy/match_region_legacy.h"
#include "domain/legacy/repeatability_legacy.h"
#include "domain/match_region.h"
#include "domain/repeatability.h"
#include "domain/lattice/basis_finder.h"
#include "domain/lattice/cell_components.h"
#include "domain/lattice/lattice_acceptance.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_accumulator_access.h"
#include "domain/lattice/lattice_detail.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_predictor.h"
#include "domain/lattice/legacy/basis_finder_legacy.h"
#include "domain/lattice/legacy/cell_components_legacy.h"
#include "domain/lattice/legacy/lattice_acceptance_legacy.h"
#include "domain/lattice/legacy/lattice_accumulator_legacy.h"
#include "domain/lattice/legacy/lattice_estimator_legacy.h"
#include "domain/lattice/legacy/lattice_predictor_legacy.h"
#include "domain/lattice/legacy/non_max_suppression_legacy.h"
#include "domain/lattice/legacy/spatial_grid_legacy.h"
#include "domain/lattice/legacy/vector_clustering_legacy.h"
#include "domain/lattice/non_max_suppression.h"
#include "domain/lattice/spatial_grid.h"
#include "domain/lattice/vector_clustering.h"

using namespace aoi;
using namespace aoi::test;

namespace {

constexpr double kRelTol = 1e-12;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

RectObj box(double x, double y, double w, double h, double angle) { return {{x, y, angle}, w, h}; }

// 隨機的旋轉矩形：大小從很小到很大、角度有一半是特別的值（0、90、45、−90、180、3）、偶爾寬或高 ≤ 0。
RectObj randomBox(Rng& rng, double spread) {
    static const double special[] = {0.0, 90.0, 45.0, -90.0, 180.0, 3.0, -0.0, 360.0};
    const double angle = rng.uniform() < 0.5 ? special[rng.below(std::size(special))] : rng.symmetric(180.0);
    double w = 0.5 + rng.uniform() * 60.0;
    double h = 0.5 + rng.uniform() * 60.0;
    const double roll = rng.uniform();
    if (roll < 0.02) w = 0.0;
    else if (roll < 0.03) h = -rng.uniform() * 5.0;
    else if (roll < 0.10) h = w;  // 正方形
    return box(rng.symmetric(spread), rng.symmetric(spread), w, h, angle);
}

bool agree(double rust, double cpp) {
    if (rust == cpp) return true;  // R05.3：兩邊同樣是 ±∞（校正值 ∞）時 rust − cpp 是 NaN，不能用下面的相減
    if (std::isnan(rust) || std::isnan(cpp)) return std::isnan(rust) && std::isnan(cpp);
    return std::abs(rust - cpp) <= kRelTol * std::max(1.0, std::abs(cpp));
}

std::vector<ScoredBox> randomScene(Rng& rng, int kind, std::size_t n) {
    std::vector<ScoredBox> boxes;
    for (std::size_t k = 0; k < n; ++k) {
        ScoredBox b;
        switch (kind) {
        case 0:  // 密集的格子＋重複的匹配（perf_lattice 的 NMS-* 那種）
        {
            const int c = static_cast<int>(k % 40);
            const int r = static_cast<int>(k / 40);
            const Point2D p = rotate({c * 30.0, r * 22.0}, 3.0);
            b.rect = box(p.x + rng.symmetric(3.0), p.y + rng.symmetric(3.0), 24.0, 16.0, 3.0 + rng.symmetric(1.0));
            break;
        }
        case 1:  // 隨機的雜亂框
            b.rect = randomBox(rng, 200.0);
            break;
        default:  // 很多一模一樣的框：同分時的排序規則
            b.rect = box(static_cast<double>(rng.below(5)) * 10.0, static_cast<double>(rng.below(5)) * 10.0, 12.0, 12.0, 0.0);
            break;
        }
        b.score = kind == 2 ? 0.5 + 0.1 * static_cast<double>(rng.below(3)) : rng.uniform();
        // 非有限值的框與分數要被丟掉（兩邊一樣）。
        const double roll = rng.uniform();
        if (roll < 0.01) b.score = kNaN;
        else if (roll < 0.015) b.rect.pose.x = kInf;
        else if (roll < 0.02) b.rect.width = kNaN;
        boxes.push_back(b);
    }
    return boxes;
}

// ---- 晶格（R05.2）的比對工具 ----

// 浮點數的比對：agree() 的容差，另外數一位不差的個數（印在 log）。
struct FloatTally {
    std::size_t values = 0;
    std::size_t bitIdentical = 0;
    double worst = 0.0;

    bool same(double rust, double cpp) {
        ++values;
        if (std::memcmp(&rust, &cpp, sizeof(double)) == 0) ++bitIdentical;
        if (!std::isnan(cpp) && !std::isnan(rust)) worst = std::max(worst, std::abs(rust - cpp) / std::max(1.0, std::abs(cpp)));
        return agree(rust, cpp);
    }
    QString summary() const {
        return QStringLiteral("%1 floating-point values, %2 bit-identical, worst relative difference %3").arg(values).arg(bitIdentical).arg(worst, 0, 'g', 3);
    }
};

QString pointText(Point2D p) { return QStringLiteral("(%1, %2)").arg(p.x, 0, 'g', 17).arg(p.y, 0, 'g', 17); }

// 兩個值不同時回傳說明（空字串 ＝ 相同）。
QString diffPoint(const char* what, Point2D r, Point2D c, FloatTally& t) {
    const bool x = t.same(r.x, c.x);
    const bool y = t.same(r.y, c.y);
    return x && y ? QString() : QStringLiteral("%1: rust %2 vs cpp %3").arg(what, pointText(r), pointText(c));
}

QString diffBasis(const LatticeBasis& r, const LatticeBasis& c, FloatTally& t) {
    if (r.hasV != c.hasV) return QStringLiteral("hasV: rust %1 vs cpp %2").arg(r.hasV).arg(c.hasV);
    for (const QString& d : {diffPoint("origin", r.origin, c.origin, t), diffPoint("u", r.u, c.u, t), diffPoint("v", r.v, c.v, t)}) {
        if (!d.isEmpty()) return d;
    }
    return {};
}

QString diffSums(const detail::LatticeSums& r, const detail::LatticeSums& c, FloatTally& t) {
    if (r.count != c.count || r.referenceCell != c.referenceCell) return QStringLiteral("accumulator count or reference cell");
    if (const QString d = diffPoint("accumulator reference point", r.referencePoint, c.referencePoint, t); !d.isEmpty()) return d;
    const double rv[] = {r.n, r.si, r.sj, r.sii, r.sij, r.sjj, r.sx, r.six, r.sjx, r.sxx, r.sy, r.siy, r.sjy, r.syy};
    const double cv[] = {c.n, c.si, c.sj, c.sii, c.sij, c.sjj, c.sx, c.six, c.sjx, c.sxx, c.sy, c.siy, c.sjy, c.syy};
    for (std::size_t k = 0; k < std::size(rv); ++k) {
        if (!t.same(rv[k], cv[k])) return QStringLiteral("accumulator sum #%1: rust %2 vs cpp %3").arg(k).arg(rv[k], 0, 'g', 17).arg(cv[k], 0, 'g', 17);
    }
    return {};
}

QString diffModel(const LatticeModel& r, const LatticeModel& c, FloatTally& t) {
    if (r.inliers != c.inliers) return QStringLiteral("inliers differ (rust %1, cpp %2)").arg(r.inliers.size()).arg(c.inliers.size());
    if (r.cells != c.cells) return QStringLiteral("cells differ");
    if (r.minI != c.minI || r.maxI != c.maxI || r.minJ != c.minJ || r.maxJ != c.maxJ) return QStringLiteral("index range differs");
    if (r.suggestCheck != c.suggestCheck) return QStringLiteral("suggestCheck differs");
    if (const QString d = diffBasis(r.basis, c.basis, t); !d.isEmpty()) return d;
    const double rv[] = {r.rmsPx, r.tolerancePx, r.coverage, r.consistency, r.confidence};
    const double cv[] = {c.rmsPx, c.tolerancePx, c.coverage, c.consistency, c.confidence};
    const char* names[] = {"rmsPx", "tolerancePx", "coverage", "consistency", "confidence"};
    for (std::size_t k = 0; k < std::size(rv); ++k) {
        if (!t.same(rv[k], cv[k])) return QStringLiteral("%1: rust %2 vs cpp %3").arg(names[k]).arg(rv[k], 0, 'g', 17).arg(cv[k], 0, 'g', 17);
    }
    return diffSums(detail::LatticeAccumulatorAccess::get(r.equations), detail::LatticeAccumulatorAccess::get(c.equations), t);
}

QString diffFit(const LatticeFit& r, const LatticeFit& c, FloatTally& t) {
    if (r.reason != c.reason) return QStringLiteral("reason: rust %1 vs cpp %2").arg(static_cast<int>(r.reason)).arg(static_cast<int>(c.reason));
    if (!t.same(r.rejectedConfidence, c.rejectedConfidence)) return QStringLiteral("rejectedConfidence differs");
    if (r.models.size() != c.models.size()) return QStringLiteral("model count: rust %1 vs cpp %2").arg(r.models.size()).arg(c.models.size());
    for (std::size_t k = 0; k < r.models.size(); ++k) {
        if (const QString d = diffModel(r.models[k], c.models[k], t); !d.isEmpty()) return QStringLiteral("model %1: %2").arg(k).arg(d);
    }
    return {};
}

QString diffCells(const std::vector<PredictedCell>& r, const std::vector<PredictedCell>& c, FloatTally& t) {
    if (r.size() != c.size()) return QStringLiteral("count: rust %1 vs cpp %2").arg(r.size()).arg(c.size());
    for (std::size_t k = 0; k < r.size(); ++k) {
        if (r[k].cell != c[k].cell) return QStringLiteral("cell %1 differs").arg(k);
        if (const QString d = diffPoint("position", r[k].position, c[k].position, t); !d.isEmpty()) return d;
    }
    return {};
}

// 分群策略：記下被呼叫幾次（tst_lattice 的 CountingClustering），內層可以是 Rust 或 C++ 的策略。
class CountingClustering final : public IVectorClustering {
public:
    explicit CountingClustering(std::shared_ptr<const IVectorClustering> inner) : inner_(std::move(inner)) {}
    VectorClusters cluster(std::span<const Point2D> vectors, double scale) const override {
        ++calls;
        return inner_->cluster(vectors, scale);
    }
    mutable int calls = 0;

private:
    std::shared_ptr<const IVectorClustering> inner_;
};

// 一律丟出例外（例外要原樣回到呼叫端，不可以穿過 Rust 的堆疊）。
class ThrowingClustering final : public IVectorClustering {
public:
    VectorClusters cluster(std::span<const Point2D>, double) const override { throw std::runtime_error("clustering failed on purpose"); }
};

// 違反契約：標號超出群數（C++ 的實作遇到是未定義行為，Rust 的包裝明確丟 std::logic_error）。
class BrokenClustering final : public IVectorClustering {
public:
    VectorClusters cluster(std::span<const Point2D> vectors, double) const override {
        VectorClusters out;
        out.clusters.push_back({{1.0, 0.0}, vectors.size()});
        out.labels.assign(vectors.size(), 5);
        return out;
    }
};

// 晶格掃描的情境：點、分群策略（Rust 用公開的類別，C++ 用 legacy 的類別）、選項。
struct LatticeCase {
    QString name;
    std::vector<Point2D> points;
    LatticeOptions options;
    bool greedy = false;
};

GridSpec sweepGrid(double angleDeg) {
    GridSpec g;
    g.cols = 12;
    g.rows = 8;
    g.pitchX = 30.0;
    g.pitchY = 22.0;
    g.angleDeg = angleDeg;
    g.origin = {300.0, 300.0};
    return g;
}

// tst_lattice_scenarios 的 1 029 種（同樣的 seed）。
std::vector<LatticeCase> sweepCases() {
    std::vector<LatticeCase> out;
    for (const double noise : {0.0, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0}) {
        for (const double missing : {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6}) {
            for (const double angle : {0.0, 5.0, 17.0, 33.0, 45.0, 90.0, -60.0}) {
                for (const int outliers : {0, 5, 20}) {
                    Scenario s;
                    s.grid = sweepGrid(angle);
                    s.noisePx = noise;
                    s.missingFraction = missing;
                    s.outliers = outliers;
                    s.seed = 1000u + static_cast<std::uint64_t>(noise * 10) * 97u + static_cast<std::uint64_t>(missing * 100) * 13u +
                             static_cast<std::uint64_t>(angle + 100.0) * 7u + static_cast<std::uint64_t>(outliers);
                    out.push_back({QStringLiteral("sweep noise=%1 missing=%2 angle=%3 outliers=%4").arg(noise).arg(missing).arg(angle).arg(outliers),
                                   generate(s).points, LatticeOptions{}, false});
                }
            }
        }
    }
    return out;
}

// 掃描以外的情境：兩組晶格、一排、間距差 20 倍、散佈、很少的點、重複與非有限值、別的選項與策略。
std::vector<LatticeCase> extraCases() {
    std::vector<LatticeCase> out;
    const auto add = [&](QString name, std::vector<Point2D> pts, LatticeOptions o = {}, bool greedy = false) {
        out.push_back({std::move(name), std::move(pts), o, greedy});
    };
    Rng rng(4242);
    for (int k = 0; k < 40; ++k) {  // 兩組不同方向、不同間距的晶格
        GridSpec a;
        a.cols = 4 + static_cast<int>(rng.below(8));
        a.rows = 1 + static_cast<int>(rng.below(5));
        a.pitchX = 15.0 + rng.uniform() * 30.0;
        a.pitchY = 15.0 + rng.uniform() * 30.0;
        a.angleDeg = rng.symmetric(90.0);
        a.origin = {100.0, 100.0};
        GridSpec b = a;
        b.pitchX *= 1.3 + rng.uniform();
        b.angleDeg = rng.symmetric(90.0);
        b.origin = {700.0 + rng.symmetric(200.0), 600.0 + rng.symmetric(200.0)};
        std::vector<Point2D> pts = makePoints(a);
        for (const Point2D& p : makePoints(b)) pts.push_back({p.x + rng.symmetric(1.0), p.y + rng.symmetric(1.0)});
        add(QStringLiteral("two lattices %1").arg(k), pts);
    }
    for (int k = 0; k < 20; ++k) {  // 一排（一維）
        GridSpec g;
        g.cols = 2 + static_cast<int>(rng.below(30));
        g.rows = 1;
        g.pitchX = 5.0 + rng.uniform() * 60.0;
        g.angleDeg = rng.symmetric(180.0);
        Scenario s{g, rng.uniform() * 2.0, rng.uniform() * 0.3, static_cast<int>(rng.below(4)), 50u + static_cast<std::uint64_t>(k)};
        add(QStringLiteral("row %1").arg(k), generate(s).points);
    }
    {  // 間距差 20 倍：v 要沿 u 分列找
        GridSpec far;
        far.cols = 10;
        far.rows = 3;
        far.pitchX = 10.0;
        far.pitchY = 200.0;
        add(QStringLiteral("ratio 20"), makePoints(far));
    }
    for (int k = 0; k < 20; ++k) {  // 散佈的點（不成陣列）
        std::vector<Point2D> pts;
        const int n = 3 + static_cast<int>(rng.below(60));
        for (int q = 0; q < n; ++q) pts.push_back({rng.uniform() * 1000.0, rng.uniform() * 800.0});
        add(QStringLiteral("scatter %1").arg(k), pts);
    }
    add(QStringLiteral("empty"), {});
    add(QStringLiteral("one point"), {{5.0, 5.0}});
    add(QStringLiteral("two points"), {{5.0, 5.0}, {25.0, 5.0}});
    add(QStringLiteral("three points"), {{5.0, 5.0}, {25.0, 5.0}, {5.0, 30.0}});
    add(QStringLiteral("all overlapping"), {{5.0, 5.0}, {5.0, 5.0}, {5.0, 5.0}});
    {
        std::vector<Point2D> pts = makePoints(GridSpec{});
        pts.push_back(pts[3]);  // 重複的點
        pts.push_back(pts[7]);
        pts.push_back({std::numeric_limits<double>::quiet_NaN(), 3.0});
        pts.push_back({std::numeric_limits<double>::infinity(), 3.0});
        add(QStringLiteral("duplicates and non-finite"), pts);
    }
    {
        std::vector<Point2D> pts = makePoints(GridSpec{});  // 座標很大：數值的有效位數
        for (Point2D& p : pts) p = {p.x + 25000.0, p.y + 18000.0};
        add(QStringLiteral("far from the origin"), pts);
    }
    for (int k = 0; k < 30; ++k) {  // 別的選項與分群策略（掃描的一部分情境）
        Scenario s;
        s.grid = sweepGrid(rng.symmetric(90.0));
        s.noisePx = rng.uniform() * 3.0;
        s.missingFraction = rng.uniform() * 0.4;
        s.outliers = static_cast<int>(rng.below(15));
        s.seed = 9000u + static_cast<std::uint64_t>(k);
        LatticeOptions o;
        o.inlierFraction = 0.15 + rng.uniform() * 0.2;
        o.ransacTrials = 1 + static_cast<int>(rng.below(32));
        o.maxGapCells = static_cast<int>(rng.below(4));
        o.minInliers = 2 + rng.below(6);
        o.minConfidence = rng.uniform() * 0.5;
        o.maxLattices = 1 + rng.below(5);
        o.neighbors = 2 + rng.below(6);
        o.maxSamples = 16 + rng.below(300);
        o.seed = rng.next();
        add(QStringLiteral("options %1%2").arg(k).arg(k % 2 == 0 ? " greedy" : ""), generate(s).points, o, k % 2 == 0);
    }
    return out;
}

// Rust（公開的類別）與 C++（legacy）各自的分群策略。
std::shared_ptr<const IVectorClustering> rustClustering(bool greedy) {
    if (greedy) return std::make_shared<GreedyClustering>();
    return std::make_shared<GridVoteClustering>();
}
std::shared_ptr<const IVectorClustering> cppClustering(bool greedy) {
    if (greedy) return std::make_shared<legacy::GreedyClustering>();
    return std::make_shared<legacy::GridVoteClustering>();
}

std::vector<Point2D> finitePoints(const std::vector<Point2D>& pts) {
    std::vector<Point2D> out;
    for (const Point2D& p : pts) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) out.push_back(p);
    }
    return out;
}


// ---- 比對區與自動擷取（R05.3）的比對工具 ----

QString maskText(const PixelMask& m) {
    return QStringLiteral("(%1, %2, %3x%4, %5 bits, %6 set)").arg(m.x).arg(m.y).arg(m.width).arg(m.height).arg(m.bits.size()).arg(legacy::maskCount(m));
}

// 遮罩完全相同（位置、大小、每個 bit）。
QString diffMask(const char* what, const PixelMask& r, const PixelMask& c) {
    if (r.x == c.x && r.y == c.y && r.width == c.width && r.height == c.height && r.bits == c.bits) return {};
    return QStringLiteral("%1: rust %2 vs cpp %3").arg(what, maskText(r), maskText(c));
}

QString diffRuns(const char* what, const MatchMask& r, const MatchMask& c) {
    if (r == c) return {};
    return QStringLiteral("%1: rust %2x%3 with %4 runs vs cpp %5x%6 with %7 runs")
        .arg(what)
        .arg(r.width)
        .arg(r.height)
        .arg(r.runs.size())
        .arg(c.width)
        .arg(c.height)
        .arg(c.runs.size());
}

// 隨機的遮罩：亮塊狀（幾個橢圓）或雜點，偶爾空的、寬或高 ≤ 0、bits 的個數不對（＝ 空的遮罩）。
PixelMask randomMask(Rng& rng, int maxW, int maxH) {
    PixelMask m;
    m.x = static_cast<int>(rng.symmetric(300.0));
    m.y = static_cast<int>(rng.symmetric(300.0));
    m.width = 1 + static_cast<int>(rng.below(static_cast<std::size_t>(maxW)));
    m.height = 1 + static_cast<int>(rng.below(static_cast<std::size_t>(maxH)));
    m.bits.assign(static_cast<std::size_t>(m.width) * static_cast<std::size_t>(m.height), 0);
    const double roll = rng.uniform();
    if (roll < 0.45) {
        const int ellipses = 1 + static_cast<int>(rng.below(4));
        for (int e = 0; e < ellipses; ++e) {
            const double cx = rng.uniform() * m.width, cy = rng.uniform() * m.height;
            const double rx = 0.5 + rng.uniform() * m.width / 2.0, ry = 0.5 + rng.uniform() * m.height / 2.0;
            for (int y = 0; y < m.height; ++y) {
                for (int x = 0; x < m.width; ++x) {
                    const double dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry;
                    if (dx * dx + dy * dy <= 1.0) m.bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(m.width) + static_cast<std::size_t>(x)] = 1;
                }
            }
        }
    } else if (roll < 0.85) {
        const double density = rng.uniform();
        for (std::uint8_t& b : m.bits) b = rng.uniform() < density ? static_cast<std::uint8_t>(1 + rng.below(255)) : 0;  // 非 0 都算 1
    } else if (roll < 0.90) {
        std::fill(m.bits.begin(), m.bits.end(), std::uint8_t{1});
    } else if (roll < 0.94) {
        // 全部是 0
    } else if (roll < 0.97) {
        m.bits.pop_back();  // bits 的個數不對
    } else {
        m.width = -static_cast<int>(rng.below(3));  // 寬 ≤ 0
    }
    return m;
}

// 隨機的 runs：大多合法（從遮罩來），偶爾超出格子、反過來、重疊、負的、格子的寬高 ≤ 0。
MatchMask randomRuns(Rng& rng) {
    MatchMask r;
    r.width = static_cast<int>(rng.below(50)) - 2;
    r.height = static_cast<int>(rng.below(40)) - 2;
    const std::size_t n = rng.below(30);
    for (std::size_t k = 0; k < n; ++k) {
        const int row = static_cast<int>(rng.below(static_cast<std::size_t>(std::max(1, r.height) + 4))) - 2;
        const int a = static_cast<int>(rng.below(static_cast<std::size_t>(std::max(1, r.width) + 8))) - 4;
        const int b = static_cast<int>(rng.below(static_cast<std::size_t>(std::max(1, r.width) + 8))) - 4;
        r.runs.push_back({row, a, b});
    }
    return r;
}

double pickUm(Rng& rng) {
    static const double special[] = {0.0, -2.0, kNaN, kInf, 6.08, 2.5, 1.0};
    return rng.uniform() < 0.25 ? special[rng.below(std::size(special))] : 0.5 + rng.uniform() * 20.0;
}

// 隨機的影像：五種格式、stride 有填充、Gray16 的列起點偶爾不是 2 byte 對齊；暗底上幾個亮塊，偶爾整張一樣亮。
struct RandomImage {
    std::vector<std::uint8_t> storage;
    ImageView view;
};

RandomImage randomImage(Rng& rng) {
    static const PixelFormat formats[] = {PixelFormat::Gray8, PixelFormat::Rgb24, PixelFormat::Bgrx32, PixelFormat::Bgr24, PixelFormat::Gray16};
    static const int bitsChoices[] = {0, 8, 10, 12, 14, 16, 20};
    RandomImage img;
    ImageView& v = img.view;
    v.format = formats[rng.below(std::size(formats))];
    v.width = 1 + static_cast<int>(rng.below(90));
    v.height = 1 + static_cast<int>(rng.below(70));
    const int bpp = bytesPerPixel(v.format);
    v.strideBytes = v.width * bpp + static_cast<int>(rng.below(8));
    v.significantBits = bitsChoices[rng.below(std::size(bitsChoices))];
    const std::size_t offset = rng.below(2);  // 1 ＝ 列的起點不對齊
    img.storage.assign(offset + static_cast<std::size_t>(v.strideBytes) * static_cast<std::size_t>(v.height), 0);
    v.data = img.storage.data() + offset;
    std::uint8_t* base = img.storage.data() + offset;
    const bool flat = rng.uniform() < 0.05;
    const int full = v.format == PixelFormat::Gray16 ? (rng.uniform() < 0.5 ? 4095 : 65535) : 255;
    struct Blob {
        double cx, cy, rx, ry;
    };
    std::vector<Blob> blobs;
    const int nb = flat ? 0 : static_cast<int>(rng.below(5));
    for (int k = 0; k < nb; ++k) blobs.push_back({rng.uniform() * v.width, rng.uniform() * v.height, 0.5 + rng.uniform() * 12.0, 0.5 + rng.uniform() * 10.0});
    const double noise = rng.uniform() * 0.25;
    for (int y = 0; y < v.height; ++y) {
        for (int x = 0; x < v.width; ++x) {
            bool in = false;
            for (const Blob& b : blobs) {
                const double dx = (x - b.cx) / b.rx, dy = (y - b.cy) / b.ry;
                in = in || dx * dx + dy * dy <= 1.0;
            }
            double level = flat ? 0.4 : (in ? 0.8 : 0.15) + rng.symmetric(noise);
            level = std::clamp(level, 0.0, 1.0);
            std::uint8_t* p = base + static_cast<std::size_t>(y) * static_cast<std::size_t>(v.strideBytes) + static_cast<std::size_t>(x) * static_cast<std::size_t>(bpp);
            if (v.format == PixelFormat::Gray16) {
                const auto g = static_cast<std::uint16_t>(std::lround(level * full));
                std::memcpy(p, &g, sizeof g);
            } else {
                for (int c = 0; c < bpp; ++c) p[c] = static_cast<std::uint8_t>(std::clamp<long>(std::lround(level * full) + static_cast<long>(rng.symmetric(30.0)), 0, 255));
            }
        }
    }
    return img;
}

Rect randomRect(Rng& rng, double extentX, double extentY) {
    const double roll = rng.uniform();
    if (roll < 0.03) return {kNaN, 0.0, 10.0, 10.0};
    if (roll < 0.06) return {5.0, 5.0, 0.0, 10.0};                         // 空的
    if (roll < 0.09) return {extentX + 50.0, extentY + 50.0, 10.0, 10.0};  // 在外面
    if (roll < 0.20) return {-1e6, -1e6, 2e6, 2e6};                        // 整個蓋住
    return {rng.symmetric(extentX * 0.3) + rng.uniform() * extentX * 0.5, rng.symmetric(extentY * 0.3) + rng.uniform() * extentY * 0.5,
            rng.uniform() * extentX * 1.2, rng.uniform() * extentY * 1.2};
}

// ---- 重複性統計（R05.4）的比對工具 ----

QString optText(const std::optional<double>& v) { return v ? QString::number(*v, 'g', 17) : QStringLiteral("nullopt"); }

// 兩份 Statistics 不同時回傳說明（空字串 ＝ 相同）：個數完全相同、每個 optional 同樣有或沒有、值用 agree()。
QString diffStatistics(const repeatability::Statistics& r, const repeatability::Statistics& c, FloatTally& t) {
    if (r.count != c.count || r.expected != c.expected || r.complete() != c.complete())
        return QStringLiteral("count %1/%2 vs %3/%4").arg(r.count).arg(r.expected).arg(c.count).arg(c.expected);
    const std::pair<const char*, std::pair<const std::optional<double>*, const std::optional<double>*>> fields[] = {
        {"minimum", {&r.minimum, &c.minimum}}, {"maximum", {&r.maximum, &c.maximum}}, {"average", {&r.average, &c.average}},
        {"range", {&r.range, &c.range}},       {"deviation", {&r.deviation, &c.deviation}}, {"pt", {&r.pt, &c.pt}}};
    for (const auto& [name, pair] : fields) {
        const std::optional<double>& a = *pair.first;
        const std::optional<double>& b = *pair.second;
        if (a.has_value() != b.has_value() || (a && !t.same(*a, *b)))
            return QStringLiteral("%1: rust %2 vs cpp %3").arg(QString::fromLatin1(name), optText(a), optText(b));
    }
    return {};
}

// 隨機的量測值：像真實的 µm（小數三位）、很大（平方和溢位）、很小（次正規數）、全部相同、±0、混著 NaN 與 ±∞。
std::vector<double> randomMeasurements(Rng& rng) {
    static const std::size_t lengths[] = {0, 1, 2, 3, 5, 5, 5, 10, 29, 30, 64};
    const std::size_t n = lengths[rng.below(std::size(lengths))];
    const int kind = static_cast<int>(rng.below(7));
    const double center = rng.symmetric(80.0), spread = rng.uniform() * 10.0;
    std::vector<double> v(n);
    for (double& x : v) {
        switch (kind) {
        case 0: case 1: x = std::round((center + rng.symmetric(spread)) * 1000.0) / 1000.0; break;  // 像真實的量測
        case 2: x = rng.symmetric(1.7e308); break;                                                  // 平方和、差會溢位
        case 3: x = rng.symmetric(1e-310); break;                                                   // 次正規數
        case 4: x = center; break;                                                                  // 全部相同（m2 ＝ 0）
        case 5: x = rng.below(2) == 0 ? 0.0 : -0.0; break;                                          // ±0
        default: x = center + rng.symmetric(spread) * 1e6; break;                                   // 大的離散
        }
        const double roll = rng.uniform();
        if (roll < 0.05) x = kNaN;
        else if (roll < 0.08) x = kInf;
        else if (roll < 0.10) x = -kInf;
    }
    return v;
}

std::optional<double> randomTolerance(Rng& rng) {
    static const double special[] = {0.0, -0.0, -200.0, kNaN, kInf, -kInf, 1e-320, 1e308, 200.0, 250.0, 350.0};
    const double roll = rng.uniform();
    if (roll < 0.15) return std::nullopt;
    if (roll < 0.45) return special[rng.below(std::size(special))];
    return rng.uniform() * 500.0;
}

// 真實的 BOX LEVEL 報表（ADR-029 的格式）：只取每份 CSV 的 BOX LEVEL 區段，第一欄是 Box（可以有引號與逗號），
// 其他欄能讀成數字的就是值；同一個 (Box, 欄) 在每份 CSV 的值收集成一組（和 application 的對齊方式相同，只是不分 Component）。
// 這裡不用 storage 的解析器（domain 的測試只連 domain），所以讀法簡化：不能讀成數字的欄（Decision 等）略過。
std::vector<QString> splitCsvLine(const QString& line) {
    std::vector<QString> out;
    QString cell;
    bool quoted = false;
    for (const QChar ch : line) {
        if (ch == QLatin1Char('"')) quoted = !quoted;
        else if (ch == QLatin1Char(',') && !quoted) { out.push_back(cell); cell.clear(); }
        else cell.append(ch);
    }
    out.push_back(cell);
    return out;
}

// 一個資料夾（不遞迴）的 CSV：(Box, 欄) → 每份 CSV 的值（依檔名的順序）。回傳讀到的 CSV 個數。
std::size_t readBoxLevel(const QString& folder, std::map<std::pair<QString, QString>, std::vector<double>>& series) {
    QStringList files = QDir(folder).entryList({QStringLiteral("*.csv")}, QDir::Files, QDir::Name);
    std::size_t reports = 0;
    for (const QString& name : files) {
        QFile file(QDir(folder).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        std::vector<QString> header;
        bool inBox = false;
        for (QString line : lines) {
            line = line.trimmed();
            if (line.startsWith(QStringLiteral("==="))) {
                inBox = line.contains(QStringLiteral("BOX LEVEL"));
                header.clear();
                continue;
            }
            if (!inBox || line.isEmpty()) continue;
            const std::vector<QString> cells = splitCsvLine(line);
            if (header.empty()) {
                header = cells;
                continue;
            }
            for (std::size_t c = 1; c < cells.size() && c < header.size(); ++c) {
                bool ok = false;
                const double v = cells[c].trimmed().toDouble(&ok);
                if (ok) series[{cells[0], header[c].trimmed()}].push_back(v);
            }
        }
        ++reports;
    }
    return reports;
}

}  // namespace

class TstDomainRsDiff : public QObject {
    Q_OBJECT

private slots:
    void abiVersionMatchesAndPanicsStayOnTheRustSide() {
        QCOMPARE(aoi_rs_abi_version(), AOI_RS_ABI_VERSION);
        // panic 被 catch_unwind 擋下，回傳狀態碼；行程沒有結束、這一行之後的測試照常執行。
        QCOMPARE(aoi_rs_test_panic(), AOI_RS_PANIC);
        double out = -1.0;
        QCOMPARE(aoi_rs_overlap_area(nullptr, nullptr, &out), AOI_RS_INVALID_ARGUMENT);
    }

    void overlapAreasAndRatiosAgree() {
        Rng rng(20261027);
        std::size_t pairs = 0, bitIdentical = 0, overlapping = 0;
        double worst = 0.0;
        const auto compare = [&](const RectObj& a, const RectObj& b) {
            const double values[3][2] = {
                {overlapArea(a, b), legacy::overlapArea(a, b)},
                {overlapRatio(a, b, OverlapMeasure::IntersectionOverUnion), legacy::overlapRatio(a, b, OverlapMeasure::IntersectionOverUnion)},
                {overlapRatio(a, b, OverlapMeasure::IntersectionOverSmaller), legacy::overlapRatio(a, b, OverlapMeasure::IntersectionOverSmaller)},
            };
            ++pairs;
            bool identical = true;
            for (const auto& v : values) {
                if (!agree(v[0], v[1])) {
                    QFAIL(qPrintable(QStringLiteral("rust %1 vs cpp %2 for a=(%3,%4,%5x%6,%7) b=(%8,%9,%10x%11,%12)")
                                         .arg(v[0], 0, 'g', 17).arg(v[1], 0, 'g', 17)
                                         .arg(a.pose.x).arg(a.pose.y).arg(a.width).arg(a.height).arg(a.pose.angleDeg)
                                         .arg(b.pose.x).arg(b.pose.y).arg(b.width).arg(b.height).arg(b.pose.angleDeg)));
                }
                if (std::memcmp(&v[0], &v[1], sizeof(double)) != 0) identical = false;
                if (!std::isnan(v[1])) worst = std::max(worst, std::abs(v[0] - v[1]) / std::max(1.0, std::abs(v[1])));
            }
            if (identical) ++bitIdentical;
            if (values[0][1] > 0.0) ++overlapping;
        };
        for (int k = 0; k < 200000; ++k) {
            const RectObj a = randomBox(rng, 40.0);
            RectObj b = randomBox(rng, 40.0);
            if (k % 10 == 0) {
                b = a;  // 完全相同
            } else if (k % 10 == 1) {
                // 只碰到邊（軸對齊）：交集是一條線，面積 0
                const RectObj axis = box(a.pose.x, a.pose.y, a.width, a.height, 0.0);
                compare(axis, box(a.pose.x + a.width, a.pose.y, a.width, a.height, 0.0));
            }
            compare(a, b);
            if (QTest::currentTestFailed()) return;
        }
        // 非有限值：兩邊同樣是 NaN 或同樣是 0。
        for (const RectObj& odd : {box(kNaN, 0, 10, 10, 0), box(0, kInf, 10, 10, 0), box(0, 0, kNaN, 10, 0), box(0, 0, 10, 10, kNaN), box(0, 0, kInf, 10, 0)}) {
            compare(odd, box(0, 0, 10, 10, 0));
            compare(box(0, 0, 10, 10, 30), odd);
        }
        qInfo().noquote() << QStringLiteral("%1 pairs (%2 overlapping): %3 bit-identical in all three values, worst relative difference %4")
                                 .arg(pairs).arg(overlapping).arg(bitIdentical).arg(worst, 0, 'g', 3);
        QVERIFY(overlapping > pairs / 4);  // 輸入真的有涵蓋重疊的情況
    }

    void nmsKeepsTheSameIndices_data() {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("count");
        QTest::addColumn<double>("maxOverlap");
        QTest::addColumn<int>("measure");
        const char* kinds[] = {"dense", "clutter", "ties"};
        for (int kind = 0; kind < 3; ++kind) {
            for (const int count : {0, 1, 37, 2000}) {
                for (const double tol : {0.0, 0.1, 0.3, 0.5, 0.9, 1.0}) {
                    for (const int measure : {0, 1}) {
                        QTest::addRow("%s-%d-%.1f-%s", kinds[kind], count, tol, measure == 0 ? "iou" : "smaller") << kind << count << tol << measure;
                    }
                }
            }
        }
    }

    void nmsKeepsTheSameIndices() {
        QFETCH(int, kind);
        QFETCH(int, count);
        QFETCH(double, maxOverlap);
        QFETCH(int, measure);
        Rng rng(static_cast<std::uint64_t>(1000 * kind + count));
        const std::vector<ScoredBox> boxes = randomScene(rng, kind, static_cast<std::size_t>(count));
        const NmsOptions options{maxOverlap, measure == 0 ? OverlapMeasure::IntersectionOverUnion : OverlapMeasure::IntersectionOverSmaller};
        const std::vector<std::size_t> rust = NonMaxSuppression(options).keep(boxes);
        const std::vector<std::size_t> cpp = legacy::keep(boxes, options);
        QCOMPARE(rust, cpp);
        if (count > 0 && kind == 0) QVERIFY(!cpp.empty());
    }

    void overlapIndexAnswersTheSame_data() {
        QTest::addColumn<double>("cellSize");
        QTest::addColumn<int>("measure");
        QTest::newRow("diagonal") << 30.0 << 0;
        QTest::newRow("tiny-cells") << 0.5 << 1;
        QTest::newRow("huge-cells") << 1.0e6 << 0;
        QTest::newRow("zero-cell") << 0.0 << 1;     // 不是正的有限值：兩邊都改用 1
        QTest::newRow("negative-cell") << -5.0 << 0;
        QTest::newRow("nan-cell") << kNaN << 1;
    }

    void overlapIndexAnswersTheSame() {
        QFETCH(double, cellSize);
        QFETCH(int, measure);
        const OverlapMeasure m = measure == 0 ? OverlapMeasure::IntersectionOverUnion : OverlapMeasure::IntersectionOverSmaller;
        OverlapIndex rust(cellSize, 0.3, m);
        legacy::OverlapIndex cpp(cellSize, 0.3, m);
        Rng rng(7);
        for (int k = 0; k < 3000; ++k) {
            RectObj b = randomBox(rng, 300.0);
            if (k % 97 == 0) b.pose.y = kNaN;  // 不加、也不和任何框重疊
            const bool r = rust.overlapsAny(b);
            const bool c = cpp.overlapsAny(b);
            if (r != c) QFAIL(qPrintable(QStringLiteral("box %1: rust %2, cpp %3").arg(k).arg(r).arg(c)));
            if (!c || k % 5 == 0) {  // 大多只加不重疊的（NMS 的用法），偶爾也加重疊的
                rust.add(b);
                cpp.add(b);
            }
            QCOMPARE(rust.size(), cpp.size());
        }
        QVERIFY(cpp.size() > 100);
    }

    // Rust 的 handle 由包裝擁有：移動之後舊的物件不再持有它（解構不會釋放兩次），而且回答安全的值。
    void aMovedFromIndexIsEmptyAndSafe() {
        OverlapIndex a(10.0, 0.5, OverlapMeasure::IntersectionOverUnion);
        a.add(box(0, 0, 10, 10, 0));
        OverlapIndex b = std::move(a);
        QCOMPARE(b.size(), std::size_t{1});
        QVERIFY(b.overlapsAny(box(1, 0, 10, 10, 0)));
        QCOMPARE(a.size(), std::size_t{0});  // NOLINT(bugprone-use-after-move)：這裡就是要測移動之後
        QVERIFY(!a.overlapsAny(box(1, 0, 10, 10, 0)));
        a = std::move(b);
        QCOMPARE(a.size(), std::size_t{1});
    }

    // ---- 晶格（R05.2） ----

    void accumulatorAgrees() {
        Rng rng(31);
        FloatTally t;
        for (int run = 0; run < 400; ++run) {
            const Point2D reference{rng.symmetric(30000.0), rng.symmetric(30000.0)};
            const LatticeIndex referenceCell{static_cast<int>(rng.below(7)) - 3, static_cast<int>(rng.below(7)) - 3};
            LatticeAccumulator rust(reference, referenceCell);
            legacy::LatticeAccumulator cpp(reference, referenceCell);
            const bool oneRow = run % 5 == 0;
            const bool oneColumn = run % 7 == 0;
            std::vector<std::pair<LatticeIndex, Point2D>> added;
            const int steps = static_cast<int>(rng.below(60));
            for (int s = 0; s < steps; ++s) {
                if (!added.empty() && rng.uniform() < 0.2) {  // 拿掉一個先前加過的點
                    const std::size_t k = rng.below(added.size());
                    rust.remove(added[k].first, added[k].second);
                    cpp.remove(added[k].first, added[k].second);
                    added.erase(added.begin() + static_cast<std::ptrdiff_t>(k));
                } else {
                    const LatticeIndex cell{oneColumn ? 2 : static_cast<int>(rng.below(40)) - 20, oneRow ? 0 : static_cast<int>(rng.below(30)) - 15};
                    const Point2D p{reference.x + cell.i * 31.7 + cell.j * 1.3 + rng.symmetric(2.0), reference.y + cell.i * 0.9 + cell.j * 22.1 + rng.symmetric(2.0)};
                    rust.add(cell, p);
                    cpp.add(cell, p);
                    added.push_back({cell, p});
                }
                QCOMPARE(rust.count(), cpp.count());
                QCOMPARE(rust.twoDimensional(), cpp.twoDimensional());
                const std::optional<LatticeBasis> rs = rust.solve();
                const std::optional<LatticeBasis> cs = cpp.solve();
                QCOMPARE(rs.has_value(), cs.has_value());
                if (cs) {
                    if (const QString d = diffBasis(*rs, *cs, t); !d.isEmpty()) QFAIL(qPrintable(d));
                    if (!t.same(rust.residualSumOfSquares(*cs), cpp.residualSumOfSquares(*cs))) QFAIL("residual sum of squares differs");
                }
                if (const QString d = diffSums(detail::LatticeAccumulatorAccess::get(rust), cpp.sums(), t); !d.isEmpty()) QFAIL(qPrintable(d));
            }
        }
        qInfo().noquote() << "accumulator:" << t.summary();
    }

    void spatialGridAgrees() {
        Rng rng(77);
        FloatTally t;
        std::size_t queries = 0;
        for (int run = 0; run < 60; ++run) {
            std::vector<Point2D> pts;
            const int n = static_cast<int>(rng.below(400));
            const int kind = run % 4;
            for (int k = 0; k < n; ++k) {
                if (kind == 0) pts.push_back({rng.uniform() * 500.0, rng.uniform() * 300.0});  // 均勻
                else if (kind == 1) pts.push_back({(k % 20) * 12.0 + rng.symmetric(1.0), (k / 20) * 9.0 + rng.symmetric(1.0)});  // 格子
                else if (kind == 2) pts.push_back({k * 3.0, 100.0});  // 共線
                else pts.push_back({(k % 2) * 1.0e6 + rng.uniform() * 50.0, rng.uniform() * 50.0});  // 兩群相距很遠
                if (k > 0 && rng.uniform() < 0.05) pts.push_back(pts.back());  // 重疊的點
            }
            const double cell = run % 3 == 0 ? 0.0 : (run % 3 == 1 ? 7.5 : -1.0);
            const SpatialGrid rust(pts, cell);
            const legacy::SpatialGrid cpp(pts, cell);
            if (!t.same(rust.cellSize(), cpp.cellSize())) QFAIL("cell size differs");
            std::vector<SpatialGrid::Neighbor> r, c;
            for (std::size_t self = 0; self <= pts.size(); ++self) {  // pts.size() 本身：超出範圍
                const std::size_t k = rng.below(9);
                const double minDistance = rng.uniform() < 0.3 ? 0.0 : 1e-9;
                rust.nearest(self, k, minDistance, r);
                cpp.nearest(self, k, minDistance, c);
                ++queries;
                QCOMPARE(r.size(), c.size());
                for (std::size_t q = 0; q < r.size(); ++q) {
                    QCOMPARE(r[q].index, c[q].index);
                    if (!t.same(r[q].distance, c[q].distance)) QFAIL("neighbour distance differs");
                }
            }
        }
        qInfo().noquote() << queries << "nearest() queries;" << t.summary();
    }

    void clusteringAgrees() {
        Rng rng(5);
        FloatTally t;
        for (int run = 0; run < 300; ++run) {
            std::vector<Point2D> v;
            const int n = static_cast<int>(rng.below(500));
            const int centers = 1 + static_cast<int>(rng.below(5));
            std::vector<Point2D> c;
            for (int k = 0; k < centers; ++k) c.push_back({rng.symmetric(60.0), rng.symmetric(60.0)});
            for (int k = 0; k < n; ++k) {
                const double roll = rng.uniform();
                if (roll < 0.01) v.push_back({std::numeric_limits<double>::quiet_NaN(), 1.0});
                else if (roll < 0.02) v.push_back({1.0e15, 2.0});  // 超出投票的範圍
                else if (roll < 0.15) v.push_back({rng.symmetric(80.0), rng.symmetric(80.0)});
                else {
                    const Point2D& m = c[rng.below(c.size())];
                    v.push_back({m.x + rng.symmetric(1.5), m.y + rng.symmetric(1.5)});
                }
            }
            const double scale = run % 10 == 0 ? 0.0 : 5.0 + rng.uniform() * 40.0;
            const double parameter = 0.05 + rng.uniform() * 0.3;
            for (const bool greedy : {false, true}) {
                const VectorClusters r = greedy ? GreedyClustering(parameter).cluster(v, scale) : GridVoteClustering(parameter).cluster(v, scale);
                const VectorClusters cc =
                    greedy ? legacy::GreedyClustering(parameter).cluster(v, scale) : legacy::GridVoteClustering(parameter).cluster(v, scale);
                QCOMPARE(r.labels, cc.labels);
                QCOMPARE(r.clusters.size(), cc.clusters.size());
                for (std::size_t k = 0; k < r.clusters.size(); ++k) {
                    QCOMPARE(r.clusters[k].members, cc.clusters[k].members);
                    if (const QString d = diffPoint("cluster center", r.clusters[k].center, cc.clusters[k].center, t); !d.isEmpty()) QFAIL(qPrintable(d));
                }
            }
        }
        qInfo().noquote() << "clustering:" << t.summary();
    }

    void cellComponentsAgree() {
        Rng rng(11);
        for (int run = 0; run < 500; ++run) {
            std::vector<LatticeIndex> cells;
            const int n = static_cast<int>(rng.below(120));
            const int spread = run % 4 == 0 ? 2000000 : 12 + static_cast<int>(rng.below(40));  // 很大的範圍：雜湊表那一條路
            for (int k = 0; k < n; ++k) {
                const LatticeIndex cell{static_cast<int>(rng.below(static_cast<std::size_t>(spread))) - spread / 3,
                                        static_cast<int>(rng.below(static_cast<std::size_t>(spread / 2 + 1))) - spread / 5};
                // 每格最多一次（估計器的候選每格只有一個點；重複的格子在兩邊的排序可能不同，見 cell_components.rs）。
                if (std::find(cells.begin(), cells.end(), cell) == cells.end()) cells.push_back(cell);
            }
            const int link = 1 + static_cast<int>(rng.below(4));
            for (const bool twoD : {false, true}) QCOMPARE(detail::largestComponent(cells, link, twoD), legacy::largestComponent(cells, link, twoD));
        }
    }

    void basisFinderAndAcceptanceRulesAgree() {
        FloatTally t;
        std::size_t checked = 0;
        const GridVoteClustering rustClusters;
        const legacy::GridVoteClustering cppClusters;
        std::vector<LatticeCase> cases = extraCases();
        const std::vector<LatticeCase> sweep = sweepCases();
        for (std::size_t k = 0; k < sweep.size(); k += 7) cases.push_back(sweep[k]);  // 掃描的七分之一
        for (const LatticeCase& lc : cases) {
            const std::vector<Point2D> pts = finitePoints(lc.points);
            if (pts.size() < 2) continue;
            const detail::BasisFinder rf(lc.options, rustClusters, pts, 1e-6);
            const legacy::BasisFinder cf(lc.options, cppClusters, pts, 1e-6);
            const std::vector<Point2D> rd = rf.directions();
            const std::vector<Point2D> cd = cf.directions();
            if (rd.size() != cd.size()) QFAIL(qPrintable(lc.name + ": direction count differs"));
            for (std::size_t k = 0; k < rd.size(); ++k) {
                if (const QString d = diffPoint("direction", rd[k], cd[k], t); !d.isEmpty()) QFAIL(qPrintable(lc.name + ": " + d));
            }
            for (std::size_t k = 0; k < std::min<std::size_t>(cd.size(), 3); ++k) {
                const std::optional<Point2D> rv = rf.secondDirection(cd[k], cd);
                const std::optional<Point2D> cv = cf.secondDirection(cd[k], cd);
                if (rv.has_value() != cv.has_value()) QFAIL(qPrintable(lc.name + ": second direction found on one side only"));
                if (cv) {
                    if (const QString d = diffPoint("v", *rv, *cv, t); !d.isEmpty()) QFAIL(qPrintable(lc.name + ": " + d));
                }
            }

            // 接受規則：用 C++ 擬合出來的晶格當候選（原樣、改成一維、只留一部分成員），兩邊各判一次。
            const LatticeFit fit = legacy::fitLattices(lc.options, cppClusters, pts, 4);
            for (const LatticeModel& m : fit.models) {
                if (detail::holesAreBacked(m) != legacy::holesAreBacked(m)) QFAIL(qPrintable(lc.name + ": holesAreBacked differs"));
                if (!t.same(detail::consistency(m, pts), legacy::consistency(m, pts))) QFAIL(qPrintable(lc.name + ": consistency differs"));
                for (int variant = 0; variant < 3; ++variant) {
                    detail::LatticeCandidate c;
                    c.basis = m.basis;
                    c.tolerance = m.tolerancePx;
                    for (std::size_t q = 0; q < m.inliers.size(); ++q) {
                        if (variant == 2 && q % 3 == 1) continue;
                        c.members.push_back(m.inliers[q]);
                        c.cells.push_back(m.cells[q]);
                    }
                    if (variant == 1) {
                        c.basis.hasV = false;
                        for (LatticeIndex& cell : c.cells) cell.j = 0;
                    }
                    ++checked;
                    if (detail::twoDimensionalIsValid(c, pts, lc.options) != legacy::twoDimensionalIsValid(c, pts, lc.options))
                        QFAIL(qPrintable(lc.name + ": twoDimensionalIsValid differs"));
                    if (detail::enoughEvidence(c, pts, lc.options) != legacy::enoughEvidence(c, pts, lc.options))
                        QFAIL(qPrintable(lc.name + ": enoughEvidence differs"));
                }
            }
        }
        qInfo().noquote() << cases.size() << "point sets," << checked << "candidates;" << t.summary();
        QVERIFY(checked > 500);
    }

    // 估計：tst_lattice_scenarios 的 1 029 種＋其他情境，fitAll 與 fit 的結果逐欄比對。
    void estimatorAgrees() {
        FloatTally t;
        std::vector<LatticeCase> cases = sweepCases();
        const std::vector<LatticeCase> extra = extraCases();
        cases.insert(cases.end(), extra.begin(), extra.end());
        std::size_t lattices = 0, none = 0;
        for (const LatticeCase& lc : cases) {
            const LatticeEstimator rust(lc.options, rustClustering(lc.greedy));
            const std::shared_ptr<const IVectorClustering> cpp = cppClustering(lc.greedy);
            const LatticeFit r = rust.fitAll(lc.points);
            const LatticeFit c = legacy::fitLattices(lc.options, *cpp, lc.points, std::max<std::size_t>(lc.options.maxLattices, 1));
            if (const QString d = diffFit(r, c, t); !d.isEmpty()) QFAIL(qPrintable(lc.name + " fitAll: " + d));
            const std::optional<LatticeModel> rf = rust.fit(lc.points);
            const LatticeFit cf = legacy::fitLattices(lc.options, *cpp, lc.points, 1);
            if (rf.has_value() != !cf.models.empty()) QFAIL(qPrintable(lc.name + " fit: found on one side only"));
            if (rf) {
                if (const QString d = diffModel(*rf, cf.models.front(), t); !d.isEmpty()) QFAIL(qPrintable(lc.name + " fit: " + d));
            }
            lattices += c.models.size();
            none += c.models.empty() ? 1 : 0;
        }
        qInfo().noquote() << cases.size() << "point sets," << lattices << "lattices," << none << "without a lattice;" << t.summary();
        QVERIFY(lattices > 1000);
        QVERIFY(none > 10);  // 「沒有晶格」的原因也比到了
    }

    // 自訂的分群策略：Rust 的估計器經回呼呼叫 C++ 的策略。呼叫次數與結果都和 C++ 的估計器相同。
    void anInjectedClusteringIsCalledBackTheSameWay() {
        FloatTally t;
        const std::vector<LatticeCase> sweep = sweepCases();
        for (std::size_t k = 0; k < sweep.size(); k += 13) {
            const LatticeCase& lc = sweep[k];
            for (const bool innerIsRust : {false, true}) {
                auto rustSide = std::make_shared<CountingClustering>(innerIsRust ? rustClustering(false) : cppClustering(false));
                auto cppSide = std::make_shared<CountingClustering>(cppClustering(false));
                const LatticeFit r = LatticeEstimator(lc.options, rustSide).fitAll(lc.points);
                const LatticeFit c = legacy::fitLattices(lc.options, *cppSide, lc.points, lc.options.maxLattices);
                if (const QString d = diffFit(r, c, t); !d.isEmpty()) QFAIL(qPrintable(lc.name + ": " + d));
                QCOMPARE(rustSide->calls, cppSide->calls);
                QVERIFY(cppSide->calls >= 1);
            }
        }
        // 間距差 20 倍：找 v 也要分群（第二次呼叫），兩邊一樣。
        GridSpec far;
        far.cols = 10;
        far.rows = 3;
        far.pitchX = 10.0;
        far.pitchY = 200.0;
        auto rustSide = std::make_shared<CountingClustering>(cppClustering(false));
        auto cppSide = std::make_shared<CountingClustering>(cppClustering(false));
        const LatticeFit r = LatticeEstimator(LatticeOptions{}, rustSide).fitAll(makePoints(far));
        const LatticeFit c = legacy::fitLattices(LatticeOptions{}, *cppSide, makePoints(far), LatticeOptions{}.maxLattices);
        if (const QString d = diffFit(r, c, t); !d.isEmpty()) QFAIL(qPrintable(d));
        QCOMPARE(rustSide->calls, cppSide->calls);
        QVERIFY(cppSide->calls >= 2);
    }

    // 策略丟出的例外原樣回到呼叫端（不會穿過 Rust 的堆疊：記在包裝裡，Rust 回傳之後重新丟出）。
    void exceptionsFromAnInjectedClusteringReachTheCaller() {
        const std::vector<Point2D> pts = makePoints(GridSpec{});
        const LatticeEstimator rust(LatticeOptions{}, std::make_shared<ThrowingClustering>());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, rust.fitAll(pts));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, legacy::fitLattices(LatticeOptions{}, ThrowingClustering(), pts, 4));
        // 例外之後同一個估計器照常可用（沒有留下狀態）。
        QVERIFY(LatticeEstimator().fit(pts).has_value());
        // 違反契約的輸出：Rust 的包裝明確丟 std::logic_error（C++ 的實作是未定義行為，不呼叫）。
        QVERIFY_THROWS_EXCEPTION(std::logic_error, LatticeEstimator(LatticeOptions{}, std::make_shared<BrokenClustering>()).fitAll(pts));
    }

    void predictorAgrees() {
        FloatTally t;
        Rng rng(99);
        std::size_t predictions = 0;
        std::vector<LatticeCase> cases = extraCases();
        const std::vector<LatticeCase> sweep = sweepCases();
        for (std::size_t k = 0; k < sweep.size(); k += 5) cases.push_back(sweep[k]);
        const legacy::GridVoteClustering clusters;
        for (const LatticeCase& lc : cases) {
            const LatticeFit fit = legacy::fitLattices(lc.options, clusters, lc.points, 4);
            for (const LatticeModel& m : fit.models) {
                const Rect bounds[] = {{0.0, 0.0, 1200.0, 900.0},
                                       {rng.uniform() * 400.0, rng.uniform() * 400.0, 50.0 + rng.uniform() * 600.0, 50.0 + rng.uniform() * 500.0},
                                       {-1e9, -1e9, 2e9, 2e9},
                                       {10.0, 10.0, 0.0, 5.0}};  // 空的
                for (const Rect& b : bounds) {
                    const LatticePredictor rp(m, b);
                    const legacy::LatticePredictor cp(m, b);
                    const std::size_t maxCount = rng.uniform() < 0.3 ? 1 + rng.below(20) : LatticePredictor::kUnlimited;
                    const int margin = static_cast<int>(rng.below(4));
                    const std::vector<PredictedCell> pairs[][2] = {
                        {rp.cellsInRange(margin, maxCount), cp.cellsInRange(margin, maxCount)},
                        {rp.holes(maxCount), cp.holes(maxCount)},
                        {rp.allInBounds(std::min<std::size_t>(maxCount, 20000)), cp.allInBounds(std::min<std::size_t>(maxCount, 20000))},
                    };
                    for (const auto& pair : pairs) {
                        ++predictions;
                        if (const QString d = diffCells(pair[0], pair[1], t); !d.isEmpty()) QFAIL(qPrintable(lc.name + ": " + d));
                        std::vector<PredictedCell> r = pair[0];
                        std::vector<PredictedCell> c = pair[1];
                        const Point2D anchor = m.basis.at(m.cells.empty() ? LatticeIndex{} : m.cells[rng.below(m.cells.size())]);
                        LatticePredictor::orderNearToFar(r, anchor);
                        legacy::LatticePredictor::orderNearToFar(c, anchor);
                        if (const QString d = diffCells(r, c, t); !d.isEmpty()) QFAIL(qPrintable(lc.name + " (near to far): " + d));
                    }
                }
            }
        }
        qInfo().noquote() << predictions << "predictions;" << t.summary();
        QVERIFY(predictions > 1000);
    }

    // ---- 比對區與自動擷取（R05.3） ----

    void maskCountRunsAndBackAgree() {
        Rng rng(0x5203);
        std::size_t masks = 0, junk = 0;
        for (int k = 0; k < 4000; ++k) {
            const PixelMask m = randomMask(rng, 48, 36);
            ++masks;
            QCOMPARE(m.count(), legacy::maskCount(m));
            const MatchMask r = runsFromMask(m);
            const MatchMask c = legacy::runsFromMask(m);
            if (const QString d = diffRuns("runsFromMask", r, c); !d.isEmpty()) QFAIL(qPrintable(d));
            const int x = static_cast<int>(rng.symmetric(800.0));
            const int y = static_cast<int>(rng.symmetric(800.0));
            if (const QString d = diffMask("maskFromRuns", maskFromRuns(r, x, y), legacy::maskFromRuns(c, x, y)); !d.isEmpty()) QFAIL(qPrintable(d));
            // 不合法的 runs（超出格子、反過來、重疊）：兩邊同樣忽略。
            const MatchMask bad = randomRuns(rng);
            ++junk;
            if (const QString d = diffMask("maskFromRuns (invalid runs)", maskFromRuns(bad, x, y), legacy::maskFromRuns(bad, x, y)); !d.isEmpty())
                QFAIL(qPrintable(d));
        }
        qInfo().noquote() << masks << "masks and" << junk << "run lists: count, runs and masks identical";
    }

    void matchSpecForAgrees() {
        FloatTally t;
        Rng rng(0x5204);
        std::size_t withMask = 0, withRect = 0;
        for (int k = 0; k < 5000; ++k) {
            MatchSettings s;
            s.mode = rng.below(2) == 0 ? MatchMode::Intensity : MatchMode::Edges;
            if (rng.uniform() >= 0.15) {
                MatchRegion region;
                region.centerXUm = rng.symmetric(900.0);
                region.centerYUm = rng.symmetric(900.0);
                region.widthUm = rng.uniform() < 0.05 ? -rng.uniform() * 50.0 : rng.uniform() * 1500.0;
                region.heightUm = rng.uniform() < 0.05 ? 0.0 : rng.uniform() * 1500.0;
                const double roll = rng.uniform();
                if (roll < 0.45) region.mask = legacy::runsFromMask(randomMask(rng, 70, 70));
                else if (roll < 0.60) region.mask = randomRuns(rng);
                s.region = region;
            }
            const Point2D center = rng.uniform() < 0.02 ? Point2D{kNaN, 10.0} : Point2D{rng.uniform() * 5000.0, rng.uniform() * 4000.0};
            const double um = pickUm(rng);
            const MatchSpec r = matchSpecFor(s, center, um);
            const MatchSpec c = legacy::matchSpecFor(s, center, um);
            QCOMPARE(r.mode, c.mode);
            QCOMPARE(r.rect.has_value(), c.rect.has_value());
            if (r.rect) {
                ++withRect;
                const double rv[] = {r.rect->x, r.rect->y, r.rect->w, r.rect->h};
                const double cv[] = {c.rect->x, c.rect->y, c.rect->w, c.rect->h};
                for (std::size_t i = 0; i < 4; ++i) {
                    if (!t.same(rv[i], cv[i])) QFAIL(qPrintable(QStringLiteral("matchSpecFor rect #%1: rust %2 vs cpp %3").arg(i).arg(rv[i], 0, 'g', 17).arg(cv[i], 0, 'g', 17)));
                }
            }
            if (!c.mask.empty()) ++withMask;
            if (const QString d = diffMask("matchSpecFor mask", r.mask, c.mask); !d.isEmpty()) QFAIL(qPrintable(d));
        }
        qInfo().noquote() << withRect << "specs with a rectangle," << withMask << "with a mask;" << t.summary();
        QVERIFY(withMask > 1000);
    }

    void matchSettingsFromPixelsAgrees() {
        FloatTally t;
        Rng rng(0x5205);
        std::size_t regions = 0, masks = 0;
        for (int k = 0; k < 5000; ++k) {
            const Rect regionPx = randomRect(rng, 600.0, 500.0);
            const PixelMask m = randomMask(rng, 60, 50);
            const PixelMask* mask = rng.uniform() < 0.3 ? nullptr : &m;
            const MatchMode mode = rng.below(2) == 0 ? MatchMode::Intensity : MatchMode::Edges;
            const Point2D center{rng.symmetric(400.0) + 300.0, rng.symmetric(400.0) + 250.0};
            const double um = pickUm(rng);
            const MatchSettings r = matchSettingsFromPixels(regionPx, mask, mode, center, um);
            const MatchSettings c = legacy::matchSettingsFromPixels(regionPx, mask, mode, center, um);
            QCOMPARE(r.mode, c.mode);
            QCOMPARE(r.region.has_value(), c.region.has_value());
            if (!r.region) continue;
            ++regions;
            const double rv[] = {r.region->centerXUm, r.region->centerYUm, r.region->widthUm, r.region->heightUm};
            const double cv[] = {c.region->centerXUm, c.region->centerYUm, c.region->widthUm, c.region->heightUm};
            for (std::size_t i = 0; i < 4; ++i) {
                if (!t.same(rv[i], cv[i])) QFAIL(qPrintable(QStringLiteral("matchSettingsFromPixels #%1: rust %2 vs cpp %3").arg(i).arg(rv[i], 0, 'g', 17).arg(cv[i], 0, 'g', 17)));
            }
            if (c.region->mask.width > 0) ++masks;
            if (const QString d = diffRuns("matchSettingsFromPixels mask", r.region->mask, c.region->mask); !d.isEmpty()) QFAIL(qPrintable(d));
        }
        qInfo().noquote() << regions << "regions," << masks << "with a mask;" << t.summary();
        QVERIFY(masks > 1000);
    }

    void pickBrightBlobAgrees() {
        Rng rng(0x5206);
        std::size_t images = 0, found = 0;
        for (int k = 0; k < 2500; ++k) {
            const RandomImage img = randomImage(rng);
            ++images;
            for (int c = 0; c < 3; ++c) {
                const Rect within = randomRect(rng, img.view.width, img.view.height);
                const double roll = rng.uniform();
                const Point2D click = roll < 0.03 ? Point2D{kNaN, 3.0}
                                    : roll < 0.08 ? Point2D{1e7, -1e7}
                                                  : Point2D{rng.symmetric(10.0) + rng.uniform() * img.view.width, rng.symmetric(10.0) + rng.uniform() * img.view.height};
                const std::optional<BlobPick> r = pickBrightBlob(img.view, within, click);
                const std::optional<BlobPick> cp = legacy::pickBrightBlob(img.view, within, click);
                QCOMPARE(r.has_value(), cp.has_value());
                if (!r) continue;
                ++found;
                QCOMPARE(r->threshold8, cp->threshold8);
                if (const QString d = diffMask("pickBrightBlob", r->blob, cp->blob); !d.isEmpty())
                    QFAIL(qPrintable(QStringLiteral("format %1: %2").arg(QString::fromLatin1(toString(img.view.format).data())).arg(d)));
            }
        }
        // 無效的影像：兩邊都找不到。
        ImageView invalid;
        invalid.width = 10;
        invalid.height = 10;
        invalid.strideBytes = 10;
        QVERIFY(!pickBrightBlob(invalid, {0.0, 0.0, 5.0, 5.0}, {1.0, 1.0}) && !legacy::pickBrightBlob(invalid, {0.0, 0.0, 5.0, 5.0}, {1.0, 1.0}));
        qInfo().noquote() << images << "images," << found << "blobs picked: thresholds and masks identical";
        QVERIFY(found > 3000);
    }

    void growMaskAgrees() {
        Rng rng(0x5207);
        std::size_t grown = 0;
        static const int radii[] = {-3, 0, 1, 2, 3, 4, 6, 9, 12};
        for (int k = 0; k < 4000; ++k) {
            const PixelMask blob = randomMask(rng, 40, 30);
            const int radius = radii[rng.below(std::size(radii))];
            const Rect clip = rng.uniform() < 0.5 ? Rect{blob.x - rng.uniform() * 20.0, blob.y - rng.uniform() * 20.0, blob.width + rng.uniform() * 40.0,
                                                         blob.height + rng.uniform() * 40.0}
                                                  : randomRect(rng, 600.0, 600.0);
            const PixelMask r = growMask(blob, radius, clip);
            const PixelMask c = legacy::growMask(blob, radius, clip);
            if (!c.empty()) ++grown;
            if (const QString d = diffMask("growMask", r, c); !d.isEmpty()) QFAIL(qPrintable(d));
        }
        qInfo().noquote() << grown << "of 4000 grown masks are not empty: identical";
        QVERIFY(grown > 1000);
    }

    // ---- 重複性的樣本統計（R05.4） ----

    void repeatabilitySummarizeAgrees() {
        FloatTally t;
        Rng rng(0x5401);
        std::size_t withPt = 0, complete = 0, empty = 0;
        for (int k = 0; k < 200000; ++k) {
            const std::vector<double> values = randomMeasurements(rng);
            const std::size_t expected = rng.below(3) == 0 ? values.size() + rng.below(3) : values.size();
            const std::optional<double> tolerance = randomTolerance(rng);
            const repeatability::Statistics r = repeatability::summarize(values, expected, tolerance);
            const repeatability::Statistics c = legacy::summarize(values, expected, tolerance);
            if (const QString d = diffStatistics(r, c, t); !d.isEmpty()) QFAIL(qPrintable(QStringLiteral("case %1: %2").arg(k).arg(d)));
            if (c.pt) ++withPt;
            if (c.complete()) ++complete;
            if (c.count == 0) ++empty;
        }
        // 空的 span（data() 可以是 null）：兩邊同樣只有 expected。
        const repeatability::Statistics none = repeatability::summarize({}, 4, 200.0);
        if (const QString d = diffStatistics(none, legacy::summarize({}, 4, 200.0), t); !d.isEmpty()) QFAIL(qPrintable(d));
        qInfo().noquote() << "summarize:" << t.summary()
                          << QStringLiteral("(200 000 inputs: %1 with P/T, %2 complete, %3 without a finite value)").arg(withPt).arg(complete).arg(empty);
        QVERIFY(withPt > 20000 && empty > 1000);
    }

    void repeatabilityRealReportsAgree() {
        const QString root = qEnvironmentVariable("AOI_REPEATABILITY_DATA");
        if (root.isEmpty()) QSKIP("Set AOI_REPEATABILITY_DATA to a folder of BOX LEVEL CSV reports (its subfolders are read too).");
        QStringList folders{root};
        for (QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories); it.hasNext();) folders << it.next();
        const repeatability::MetricCatalog catalog = repeatability::defaultMetrics();
        FloatTally t;
        std::size_t datasets = 0, reports = 0, series = 0, values = 0;
        for (const QString& folder : folders) {
            std::map<std::pair<QString, QString>, std::vector<double>> byKey;
            const std::size_t n = readBoxLevel(folder, byKey);
            if (byKey.empty()) continue;
            ++datasets;
            reports += n;
            for (const auto& [key, v] : byKey) {
                const auto metric = catalog.find(key.second.toStdString());
                const std::optional<double> tolerance = metric != catalog.end() ? metric->second.tolerance : std::nullopt;
                const repeatability::Statistics r = repeatability::summarize(v, n, tolerance);
                const repeatability::Statistics c = legacy::summarize(v, n, tolerance);
                if (const QString d = diffStatistics(r, c, t); !d.isEmpty())
                    QFAIL(qPrintable(QStringLiteral("%1 %2/%3: %4").arg(folder, key.first, key.second, d)));
                ++series;
                values += v.size();
            }
        }
        qInfo().noquote() << QStringLiteral("real reports: %1 folders, %2 CSV files, %3 (Box, column) series, %4 values;").arg(datasets).arg(reports).arg(series).arg(values)
                          << t.summary();
        QVERIFY(series > 0);
    }
};

QTEST_APPLESS_MAIN(TstDomainRsDiff)
#include "tst_domain_rs_diff.moc"
