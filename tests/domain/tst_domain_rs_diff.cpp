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
#include <QtTest>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/ffi/aoi_domain_rs.h"
#include "domain/lattice/legacy/non_max_suppression_legacy.h"
#include "domain/lattice/non_max_suppression.h"

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
};

QTEST_APPLESS_MAIN(TstDomainRsDiff)
#include "tst_domain_rs_diff.moc"
