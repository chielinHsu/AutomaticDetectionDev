// R05.3 的效能量測（label perf；ADR-031 的 before／after）：比對區與自動擷取（domain/match_region、domain/feature_extract）。
//   MR-PICK-*   pickBrightBlob：主體框內 Otsu ＋ 最近亮塊的 8 連通填色（12 bit 的 Gray16，點擊落在亮塊外：要找最近的前景）。
//   MR-GROW-*   growMask：亮塊往外擴（圓盤）。
//   MR-SPEC-*   matchSpecFor：類型上的遮罩取樣到像素格（校正值和建立時不同：最近鄰重新取樣）。
//   MR-SETTINGS matchSettingsFromPixels：像素的遮罩 → runs。
//   MR-RUNS-*   runsFromMask ＋ maskFromRuns 來回。
//   MR-COUNT    PixelMask::count，一萬個小遮罩（逐次呼叫的成本：rust 建置每次跨 FFI）。
// 全部是資訊性指標（沒有預算）。方法和 perf_lattice 相同：熱機 5 次、正式量 30 次，回報中位數與 p95；
// 結果寫到 perf/results/match_region_<時間>.json（tools/perf-compare.mjs 的來源 match_region）。
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "../ui/MeasurementHygiene.h"
#include "LatticeTestSupport.h"
#include "domain/feature_extract.h"
#include "domain/image_view.h"
#include "domain/match_region.h"

using namespace aoi;
using namespace aoi::test;
using Clock = std::chrono::steady_clock;

namespace {

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
    return v[std::min(v.size(), std::max<std::size_t>(1, rank)) - 1];
}

volatile double g_sink = 0.0;

std::vector<double> timeRuns(const std::function<void()>& fn, int warmup, int runs) {
    for (int k = 0; k < warmup; ++k) fn();
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(runs));
    for (int k = 0; k < runs; ++k) {
        const auto t0 = Clock::now();
        fn();
        out.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
    }
    return out;
}

// 12 bit 的 Gray16 板子：暗底（雜訊）上排 pitch 間距的亮塊（w × h 的圓角矩形），像密排的端子。
struct Board16 {
    int width = 0;
    int height = 0;
    std::vector<std::uint16_t> pixels;

    ImageView view() const {
        ImageView v;
        v.data = reinterpret_cast<const std::uint8_t*>(pixels.data());
        v.width = width;
        v.height = height;
        v.strideBytes = width * 2;
        v.format = PixelFormat::Gray16;
        v.significantBits = 12;
        return v;
    }
};

Board16 makeBoard(int width, int height, int pitch, int blobW, int blobH, std::uint64_t seed) {
    Rng rng(seed);
    Board16 b;
    b.width = width;
    b.height = height;
    b.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int cx = x % pitch - (pitch - blobW) / 2;
            const int cy = y % pitch - (pitch - blobH) / 2;
            const bool corner = (cx < 2 || cx >= blobW - 2) && (cy < 2 || cy >= blobH - 2);
            const bool inBlob = cx >= 0 && cy >= 0 && cx < blobW && cy < blobH && !corner;
            const double base = inBlob ? 3200.0 : 500.0;
            b.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                static_cast<std::uint16_t>(std::clamp(base + rng.symmetric(300.0), 0.0, 4095.0));
        }
    }
    return b;
}

// 隨機的二值遮罩（亮塊狀：幾個橢圓的聯集）。
PixelMask blobMask(int width, int height, int ellipses, std::uint64_t seed) {
    Rng rng(seed);
    PixelMask m;
    m.width = width;
    m.height = height;
    m.bits.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    for (int e = 0; e < ellipses; ++e) {
        const double cx = rng.uniform() * width, cy = rng.uniform() * height;
        const double rx = 2.0 + rng.uniform() * width / 4.0, ry = 2.0 + rng.uniform() * height / 4.0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const double dx = (x - cx) / rx, dy = (y - cy) / ry;
                if (dx * dx + dy * dy <= 1.0) m.bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = 1;
            }
        }
    }
    return m;
}

}  // namespace

class PerfMatchRegion : public QObject {
    Q_OBJECT

    struct Metric {
        QString id;
        QString label;
        std::vector<double> samples;
        QString note;
    };
    std::vector<Metric> metrics_;
    QString hygiene_;

    void add(const QString& id, const QString& label, std::vector<double> samples, const QString& note = {}) {
        qInfo().noquote() << QStringLiteral("%1  %2: median %3 ms, p95 %4 ms (n=%5)%6")
                                 .arg(id, label)
                                 .arg(percentile(samples, 0.5), 0, 'g', 4)
                                 .arg(percentile(samples, 0.95), 0, 'g', 4)
                                 .arg(samples.size())
                                 .arg(note.isEmpty() ? QString() : QStringLiteral("  [%1]").arg(note));
        metrics_.push_back({id, label, std::move(samples), note});
    }

private slots:
    void initTestCase() { hygiene_ = applyMeasurementHygiene(); }

    void pickBrightBlob_() {
        const Board16 board = makeBoard(1200, 1200, 148, 33, 19, 31);
        const ImageView image = board.view();
        struct Case {
            const char* id;
            double size;
        } cases[] = {{"MR-PICK-148", 148.0}, {"MR-PICK-600", 600.0}};
        for (const Case& c : cases) {
            // 點在亮塊的左上外側：起點要靠「最近的前景像素」找。
            const Rect within{148.0, 148.0, c.size, c.size};
            const Point2D click{148.0 + 50.0, 148.0 + 50.0};
            std::size_t area = 0;
            int threshold = -1;
            const std::vector<double> ms = timeRuns(
                [&] {
                    const std::optional<BlobPick> pick = pickBrightBlob(image, within, click);
                    area = pick ? pick->blob.bits.size() : 0;
                    threshold = pick ? pick->threshold8 : -1;
                    g_sink = g_sink + static_cast<double>(area);
                },
                5, 30);
            QVERIFY(threshold >= 0);
            add(QString::fromLatin1(c.id), QStringLiteral("pickBrightBlob, %1x%1 px body (12-bit Gray16, click off the blob)").arg(c.size),
                ms, QStringLiteral("threshold %1, blob box %2 px").arg(threshold).arg(area));
        }
    }

    void growMask_() {
        const PixelMask small = blobMask(33, 19, 3, 7);
        const PixelMask large = blobMask(200, 160, 6, 8);
        struct Case {
            const char* id;
            const PixelMask* blob;
            int radius;
        } cases[] = {{"MR-GROW-33x19-r4", &small, 4}, {"MR-GROW-200x160-r12", &large, 12}};
        for (const Case& c : cases) {
            PixelMask blob = *c.blob;
            blob.x = 100;
            blob.y = 100;
            std::size_t bits = 0;
            const std::vector<double> ms = timeRuns(
                [&] {
                    const PixelMask grown = growMask(blob, c.radius, {0.0, 0.0, 1000.0, 1000.0});
                    bits = grown.bits.size();
                    g_sink = g_sink + static_cast<double>(bits);
                },
                5, 30);
            add(QString::fromLatin1(c.id), QStringLiteral("growMask, %1x%2 blob, radius %3").arg(blob.width).arg(blob.height).arg(c.radius), ms,
                QStringLiteral("%1 px out").arg(bits));
        }
    }

    void matchSpecAndSettings() {
        const PixelMask pixels = blobMask(148, 148, 5, 9);
        PixelMask placed = pixels;
        placed.x = 426;
        placed.y = 426;
        const MatchSettings settings = matchSettingsFromPixels({}, &placed, MatchMode::Intensity, {500.0, 500.0}, 6.08);
        QVERIFY(settings.region && !settings.region->mask.empty());
        std::size_t bits = 0;
        // 同一個校正值（遮罩照原樣）與不同的校正值（重新取樣成 1.6 倍大）。
        add(QStringLiteral("MR-SPEC-148"), QStringLiteral("matchSpecFor, 148x148 mask, same um/px"),
            timeRuns(
                [&] {
                    bits = matchSpecFor(settings, {500.0, 500.0}, 6.08).mask.bits.size();
                    g_sink = g_sink + static_cast<double>(bits);
                },
                5, 30));
        std::vector<double> resample = timeRuns(
            [&] {
                bits = matchSpecFor(settings, {500.0, 500.0}, 3.8).mask.bits.size();
                g_sink = g_sink + static_cast<double>(bits);
            },
            5, 30);
        add(QStringLiteral("MR-SPEC-148-resample"), QStringLiteral("matchSpecFor, 148x148 mask resampled to about 237x237 px"), std::move(resample),
            QStringLiteral("%1 px out").arg(bits));
        add(QStringLiteral("MR-SETTINGS-148"), QStringLiteral("matchSettingsFromPixels, 148x148 mask"),
            timeRuns(
                [&] {
                    const MatchSettings s = matchSettingsFromPixels({}, &placed, MatchMode::Edges, {500.0, 500.0}, 6.08);
                    g_sink = g_sink + static_cast<double>(s.region->mask.runs.size());
                },
                5, 30),
            QStringLiteral("%1 runs").arg(settings.region->mask.runs.size()));
    }

    void runsRoundTrip() {
        const PixelMask mask = blobMask(600, 600, 12, 10);
        std::size_t runs = 0;
        std::vector<double> ms = timeRuns(
            [&] {
                const MatchMask r = runsFromMask(mask);
                runs = r.runs.size();
                const PixelMask back = maskFromRuns(r, 0, 0);
                g_sink = g_sink + static_cast<double>(back.bits.size() + runs);
            },
            5, 30);
        // 備註在量完之後才組（引數的求值順序沒有規定）。
        add(QStringLiteral("MR-RUNS-600"), QStringLiteral("runsFromMask + maskFromRuns, 600x600 mask"), std::move(ms), QStringLiteral("%1 runs").arg(runs));
    }

    void countPerCall() {
        std::vector<PixelMask> masks;
        for (int k = 0; k < 10000; ++k) masks.push_back(blobMask(8, 6, 1, 100 + static_cast<std::uint64_t>(k)));
        std::size_t total = 0;
        std::vector<double> ms = timeRuns(
            [&] {
                total = 0;
                for (const PixelMask& m : masks) total += m.count();
                g_sink = g_sink + static_cast<double>(total);
            },
            5, 30);
        add(QStringLiteral("MR-COUNT-10000"), QStringLiteral("PixelMask::count, 10000 masks of 8x6 (one call each)"), std::move(ms),
            QStringLiteral("%1 px set").arg(total));
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics_) {
            QJsonObject o;
            o["id"] = m.id;
            o["label"] = m.label;
            o["unit"] = "ms";
            o["n"] = static_cast<int>(m.samples.size());
            o["median"] = percentile(m.samples, 0.5);
            o["p95"] = percentile(m.samples, 0.95);
            o["max"] = m.samples.empty() ? 0.0 : *std::max_element(m.samples.begin(), m.samples.end());
            if (!m.note.isEmpty()) o["note"] = m.note;
            rows.append(o);
        }
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "R05.3";
        root["domainImpl"] = AOI_DOMAIN_IMPL_NAME;  // ADR-031：cpp 或 rust（before／after）
        root["note"] = "Match region and blob extraction (domain, standard library only). Numbers compare only on this machine.";
        root["hygiene"] = hygiene_;
        root["metrics"] = rows;
        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/match_region_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) +
                             QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_GUILESS_MAIN(PerfMatchRegion)
#include "perf_match_region.moc"
