// 真實影像的比對區驗收（ADR-026）：使用者的密排端子影像（不入版控）。環境變數 AOI_REAL_MATCH_IMAGE 指到影像檔才執行，
// 沒有就跳過（QSKIP）。效能量測：只在 scripts/dev/test.ps1 -Perf 時跑（tests.pro 的 AOI_PERF_TESTS）。
//
// 問題：CAD 框（148 × 148）裡端子的亮塊只佔約 2%，上下是大塊的亮焊墊；用整個框建範本，比對的幾乎都是焊墊，
// 少一塊焊墊的那欄就找不到、位置也偏。比對區只框端子，應該明顯變好。
//
// 量法：
//  - 標準答案：整張影像的 Otsu 二值化、連通塊；面積與外接矩形和樣本的端子亮塊相近的，就是一根端子（重心）。
//    寫到 <工作資料夾>/real-match-truth.json，疊圖 real-match-truth.png 給人目視確認（數量應該是 1047）。
//  - 每個 back end × 每種比對區（整框、矩形、遮罩、邊緣）：從樣本建範本，整張影像搜尋（minScore 0.5），
//    主體中心減掉樣本的原點位移 ＝ 預測的端子重心，和標準答案在半徑 10 px 內一對一配對。
//    門檻 0.5..0.9 各算一次：召回率、誤報、位置誤差的 p95 與最大。結果寫到 real-match-results.json、疊圖 real-match-<列>.png。
//
// 參數（環境變數，都可以不給）：
//  AOI_REAL_MATCH_CLICK  "x,y"     樣本端子上的一點（預設 2178,2002）
//  AOI_REAL_MATCH_BODY   "dx,dy,w,h" CAD 框相對端子亮塊重心的左上角與大小（預設 −71,−57,148,148）
//  AOI_REAL_MATCH_GROW   遮罩外擴幾 px（預設 4）；矩形比對區 ＝ 亮塊外接矩形外加 12 px
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "QtTestPrinters.h"
#include "app/CompositionRoot.h"
#include "application/backend/BackendRegistry.h"
#include "application/backend/SearchEngine.h"
#include "domain/feature_extract.h"
#include "domain/match_region.h"
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"

using namespace aoi;

namespace {

using Clock = std::chrono::steady_clock;

std::vector<double> numbers(const char* name, std::vector<double> fallback) {
    const QStringList parts = qEnvironmentVariable(name).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() != static_cast<qsizetype>(fallback.size())) return fallback;
    std::vector<double> out;
    for (const QString& p : parts) out.push_back(p.trimmed().toDouble());
    return out;
}

Point2D centroidOf(const PixelMask& m) {
    double sx = 0.0;
    double sy = 0.0;
    double n = 0.0;
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) {
            if (!m.bits[static_cast<std::size_t>(y) * m.width + x]) continue;
            sx += m.x + x + 0.5;
            sy += m.y + y + 0.5;
            n += 1.0;
        }
    }
    return {sx / n, sy / n};
}

struct Truth {
    std::vector<Point2D> centers;
    int threshold = 0;
};

// 整張影像的 Otsu；連通塊的面積與外接矩形和樣本的亮塊相近（0.6..1.6 倍、邊長 ±40%）的算一根端子。
Truth findTerminals(const cv::Mat& gray8, const PixelMask& sampleBlob) {
    Truth t;
    cv::Mat binary;
    t.threshold = static_cast<int>(cv::threshold(gray8, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU));
    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int n = cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);
    const double area = static_cast<double>(sampleBlob.count());
    for (int i = 1; i < n; ++i) {
        const double a = stats.at<int>(i, cv::CC_STAT_AREA);
        const double w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const double h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        if (a < 0.6 * area || a > 1.6 * area) continue;
        if (std::abs(w - sampleBlob.width) > 0.4 * sampleBlob.width || std::abs(h - sampleBlob.height) > 0.4 * sampleBlob.height) continue;
        // OpenCV 的重心：像素中心在整數 → ADR-010 加 0.5。
        t.centers.push_back({centroids.at<double>(i, 0) + 0.5, centroids.at<double>(i, 1) + 0.5});
    }
    return t;
}

struct Score {
    double threshold = 0.0;
    std::size_t matched = 0;
    std::size_t falsePositives = 0;
    double recall = 0.0;
    double p95 = 0.0;
    double maxError = 0.0;
    // 誤差向量（預測 − 標準答案）的平均（系統性的偏差）與扣掉平均之後的 p95、最大（一致性）。標準答案是每根端子的
    // Otsu 重心：端子右邊有一段暗一點的延伸，過不過門檻每根不同，所以重心本身在 x 上會跳幾個像素（樣本那一根：
    // 擷取的亮塊 39 寬、標準答案 31 寬，重心差 3.9 px）。偏差與一致性分開看，才分得出是搜尋不準還是答案不準。
    Point2D bias;
    double residualP95 = 0.0;
    double residualMax = 0.0;
    double residualXP95 = 0.0;  // 只看 x、只看 y（扣掉偏差）
    double residualYP95 = 0.0;
};

double percentile95(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, static_cast<std::size_t>(std::ceil(0.95 * v.size())) - 1)];
}

// 預測（端子重心）與標準答案在 radius 內一對一配對（依距離由近到遠，貪婪）。
Score scoreAt(const std::vector<Point2D>& truth, const std::vector<std::pair<Point2D, double>>& predicted, double threshold, double radius) {
    struct Pair {
        double d;
        std::size_t t;
        std::size_t p;
    };
    std::vector<Pair> pairs;
    std::vector<std::size_t> kept;
    for (std::size_t p = 0; p < predicted.size(); ++p) {
        if (predicted[p].second >= threshold) kept.push_back(p);
    }
    for (const std::size_t p : kept) {
        for (std::size_t t = 0; t < truth.size(); ++t) {
            const double d = distance(truth[t], predicted[p].first);
            if (d <= radius) pairs.push_back({d, t, p});
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.d < b.d; });
    std::vector<bool> usedT(truth.size(), false);
    std::vector<bool> usedP(predicted.size(), false);
    std::vector<double> errors;
    std::vector<Point2D> vectors;
    for (const Pair& q : pairs) {
        if (usedT[q.t] || usedP[q.p]) continue;
        usedT[q.t] = true;
        usedP[q.p] = true;
        errors.push_back(q.d);
        vectors.push_back({predicted[q.p].first.x - truth[q.t].x, predicted[q.p].first.y - truth[q.t].y});
    }
    Score s;
    s.threshold = threshold;
    s.matched = errors.size();
    s.falsePositives = kept.size() - errors.size();
    s.recall = truth.empty() ? 0.0 : static_cast<double>(errors.size()) / static_cast<double>(truth.size());
    std::sort(errors.begin(), errors.end());
    if (!errors.empty()) {
        s.p95 = percentile95(errors);
        s.maxError = errors.back();
        for (const Point2D& v : vectors) {
            s.bias.x += v.x / static_cast<double>(vectors.size());
            s.bias.y += v.y / static_cast<double>(vectors.size());
        }
        std::vector<double> residuals;
        for (const Point2D& v : vectors) residuals.push_back(std::hypot(v.x - s.bias.x, v.y - s.bias.y));
        s.residualP95 = percentile95(residuals);
        s.residualMax = *std::max_element(residuals.begin(), residuals.end());
        std::vector<double> rx;
        std::vector<double> ry;
        for (const Point2D& v : vectors) {
            rx.push_back(std::abs(v.x - s.bias.x));
            ry.push_back(std::abs(v.y - s.bias.y));
        }
        s.residualXP95 = percentile95(rx);
        s.residualYP95 = percentile95(ry);
    }
    return s;
}

}  // namespace

class PerfMatchRegionReal : public QObject {
    Q_OBJECT

private:
    ImageView image_;
    cv::Mat gray8_;
    PixelMask blob_;
    Point2D blobCenter_;
    Rect body_;
    Truth truth_;
    BackendRegistry registry_ = app::makeBackendRegistry();
    std::map<std::string, SearchEnginePtr> engines_;
    QJsonArray results_;
    // 每一列的預測（端子重心、分數）：最後比較兩個 back end 彼此的一致性（和標準答案無關）。
    std::map<QString, std::vector<std::pair<Point2D, double>>> predictions_;

    const SearchEngine* engine(const std::string& id) {
        auto it = engines_.find(id);
        if (it != engines_.end()) return it->second.get();
        Result<SearchEnginePtr> made = makeSearchEngine(registry_, id);
        if (!made) return nullptr;
        return engines_.emplace(id, made.value()).first->second.get();
    }

private slots:
    void initTestCase() {
        const QString path = qEnvironmentVariable("AOI_REAL_MATCH_IMAGE");
        if (path.isEmpty()) QSKIP("AOI_REAL_MATCH_IMAGE is not set: the user's real images are not in the repository.");
        infra::OpenCvImageSource source;
        Result<ImageView> loaded = source.load(path.toStdString());
        QVERIFY2(loaded.ok(), loaded.ok() ? "" : loaded.error().message.c_str());
        image_ = loaded.value();
        gray8_ = infra::toGray8(image_, cv::Rect(0, 0, image_.width, image_.height)).mat.clone();
        qInfo("image %dx%d format %s significantBits %d", image_.width, image_.height, std::string(toString(image_.format)).c_str(), image_.significantBits);

        const std::vector<double> click = numbers("AOI_REAL_MATCH_CLICK", {2178.0, 2002.0});
        const std::optional<BlobPick> pick = pickBrightBlob(image_, {click[0] - 60.0, click[1] - 60.0, 120.0, 120.0}, {click[0], click[1]});
        QVERIFY(pick);
        blob_ = pick->blob;
        blobCenter_ = centroidOf(blob_);
        const std::vector<double> body = numbers("AOI_REAL_MATCH_BODY", {-71.0, -57.0, 148.0, 148.0});
        body_ = {std::round(blobCenter_.x + body[0]), std::round(blobCenter_.y + body[1]), body[2], body[3]};
        qInfo("sample blob %dx%d at (%d,%d), %zu px, centroid (%.2f, %.2f), Otsu %d; body %.0f,%.0f %.0fx%.0f", blob_.width, blob_.height,
              blob_.x, blob_.y, blob_.count(), blobCenter_.x, blobCenter_.y, pick->threshold8, body_.x, body_.y, body_.w, body_.h);

        truth_ = findTerminals(gray8_, blob_);
        qInfo("truth: %zu terminals (global Otsu %d)", truth_.centers.size(), truth_.threshold);
        // 位移的參考點用「標準答案裡樣本那根端子的重心」：和比對時用同一個定義（擷取的亮塊門檻不同、形狀可能多黏一塊，
        // 它的重心當參考會讓每一筆都差同一個向量；第一次量測的 p95 ≈ 3.9 px 幾乎是常數，就是這個偏差）。
        double nearest = 1e300;
        Point2D reference = blobCenter_;
        for (const Point2D& c : truth_.centers) {
            const double d = distance(c, blobCenter_);
            if (d < nearest) {
                nearest = d;
                reference = c;
            }
        }
        qInfo("sample terminal in the truth: (%.2f, %.2f), %.2f px from the picked blob's centroid", reference.x, reference.y, nearest);
        QVERIFY2(nearest < 10.0, "the sample terminal is not in the truth");
        blobCenter_ = reference;
        QJsonArray centers;
        for (const Point2D& c : truth_.centers) centers.append(QJsonArray{c.x, c.y});
        QFile f(QStringLiteral("real-match-truth.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonObject{{"image", path}, {"count", static_cast<int>(truth_.centers.size())}, {"centers", centers}}).toJson());
        cv::Mat color;
        cv::cvtColor(gray8_, color, cv::COLOR_GRAY2BGR);
        for (const Point2D& c : truth_.centers) cv::circle(color, cv::Point(static_cast<int>(c.x), static_cast<int>(c.y)), 12, cv::Scalar(0, 220, 0), 2);
        cv::rectangle(color, cv::Rect(static_cast<int>(body_.x), static_cast<int>(body_.y), static_cast<int>(body_.w), static_cast<int>(body_.h)),
                      cv::Scalar(255, 0, 255), 3);
        cv::Mat small;
        cv::resize(color, small, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
        cv::imwrite("real-match-truth.png", small);
    }

    void search_data() {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<QString>("region");
        for (const char* id : {"opencv", "halcon"}) {
            for (const char* region : {"whole", "rect", "mask", "edges"}) QTest::addRow("%s %s", id, region) << id << region;
        }
    }

    void search() {
        QFETCH(QString, backend);
        QFETCH(QString, region);
        const SearchEngine* e = engine(backend.toStdString());
        if (e == nullptr) QSKIP(qPrintable(backend + QStringLiteral(" is not usable here")));

        const int grow = qEnvironmentVariableIsSet("AOI_REAL_MATCH_GROW") ? qEnvironmentVariableIntValue("AOI_REAL_MATCH_GROW") : 4;
        MatchSpec spec;
        const Rect blobRect = blob_.bounds();
        const Rect tight{blobRect.x - 12.0, blobRect.y - 12.0, blobRect.w + 24.0, blobRect.h + 24.0};
        if (region == QStringLiteral("rect")) {
            spec.rect = tight;
        } else if (region == QStringLiteral("mask")) {
            spec.mask = growMask(blob_, grow, body_);
            spec.rect = spec.mask.bounds();
        } else if (region == QStringLiteral("edges")) {
            spec.rect = tight;
            spec.mode = MatchMode::Edges;
        }

        SearchParams params;
        params.minScore = 0.5;
        params.angleMinDeg = -5.0;
        params.angleMaxDeg = 5.0;
        params.maxMatches = 100000;
        const Clock::time_point t0 = Clock::now();
        Result<TemplatePtr> built = e->backend.templateBuilder->buildFromSample(image_, SampleSpec{body_, spec}, params, CancelToken());
        QVERIFY2(built.ok(), built.ok() ? "" : built.error().message.c_str());
        const double buildMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        const Clock::time_point t1 = Clock::now();
        Result<std::vector<Match>> found = e->backend.searcher->find(image_, *built.value(), SearchRegion::wholeImage(), params, CancelToken());
        QVERIFY2(found.ok(), found.ok() ? "" : found.error().message.c_str());
        const double findMs = std::chrono::duration<double, std::milli>(Clock::now() - t1).count();

        // 主體中心 → 端子重心：減掉樣本上的位移（轉過找到的角度）。
        const Point2D o{body_.center().x - blobCenter_.x, body_.center().y - blobCenter_.y};
        std::vector<std::pair<Point2D, double>> predicted;
        for (const Match& m : found.value()) {
            const Point2D r = rotate(o, m.pose.angleDeg);
            predicted.push_back({{m.pose.x - r.x, m.pose.y - r.y}, m.score});
        }

        predictions_[backend + QLatin1Char(' ') + region] = predicted;
        QJsonArray byThreshold;
        QString line;
        for (const double th : {0.5, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9}) {
            const Score s = scoreAt(truth_.centers, predicted, th, 10.0);
            byThreshold.append(QJsonObject{{"threshold", th}, {"matched", static_cast<int>(s.matched)}, {"falsePositives", static_cast<int>(s.falsePositives)},
                                           {"recall", s.recall}, {"p95Px", s.p95}, {"maxPx", s.maxError}, {"biasX", s.bias.x}, {"biasY", s.bias.y},
                                           {"residualP95Px", s.residualP95}, {"residualMaxPx", s.residualMax},
                                           {"residualXP95Px", s.residualXP95}, {"residualYP95Px", s.residualYP95}});
            line += QStringLiteral("\n  th %1: recall %2% (%3/%4), FP %5, err p95 %6 max %7 px, bias (%8, %9), residual p95 %10 max %11 (x %12, y %13)")
                        .arg(th, 0, 'f', 2)
                        .arg(100.0 * s.recall, 0, 'f', 1)
                        .arg(s.matched)
                        .arg(truth_.centers.size())
                        .arg(s.falsePositives)
                        .arg(s.p95, 0, 'f', 2)
                        .arg(s.maxError, 0, 'f', 2)
                        .arg(s.bias.x, 0, 'f', 2)
                        .arg(s.bias.y, 0, 'f', 2)
                        .arg(s.residualP95, 0, 'f', 2)
                        .arg(s.residualMax, 0, 'f', 2)
                        .arg(s.residualXP95, 0, 'f', 2)
                        .arg(s.residualYP95, 0, 'f', 2);
        }
        qInfo("%s %s: build %.0f ms, find %.0f ms, %zu matches%s", qPrintable(backend), qPrintable(region), buildMs, findMs,
              found.value().size(), qPrintable(line));
        results_.append(QJsonObject{{"backend", backend}, {"region", region}, {"buildMs", buildMs}, {"findMs", findMs},
                                    {"matches", static_cast<int>(found.value().size())}, {"truth", static_cast<int>(truth_.centers.size())},
                                    {"byThreshold", byThreshold}});

        cv::Mat color;
        cv::cvtColor(gray8_, color, cv::COLOR_GRAY2BGR);
        for (const Point2D& c : truth_.centers) cv::circle(color, cv::Point(static_cast<int>(c.x), static_cast<int>(c.y)), 14, cv::Scalar(0, 200, 0), 2);
        for (const auto& [p, score] : predicted) {
            const cv::Scalar ink = score >= 0.8 ? cv::Scalar(255, 160, 0) : score >= 0.7 ? cv::Scalar(0, 230, 255) : cv::Scalar(0, 0, 255);
            cv::drawMarker(color, cv::Point(static_cast<int>(p.x), static_cast<int>(p.y)), ink, cv::MARKER_CROSS, 16, 2);
        }
        cv::Mat small;
        cv::resize(color, small, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
        cv::imwrite(QStringLiteral("real-match-%1-%2.png").arg(backend, region).toStdString(), small);
    }

    // 兩種做法彼此一致到什麼程度（和標準答案無關）：A 的每個預測（分數 ≥ 門檻）找 B 裡最近的（5 px 內），
    // 距離扣掉平均向量之後的 p95 與最大。兩個不同的演算法彼此很一致、卻都離標準答案幾個像素時，不準的是標準答案。
    void agreement_data() {
        QTest::addColumn<QString>("a");
        QTest::addColumn<double>("thA");
        QTest::addColumn<QString>("b");
        QTest::addColumn<double>("thB");
        QTest::newRow("opencv rect vs halcon rect") << QStringLiteral("opencv rect") << 0.8 << QStringLiteral("halcon rect") << 0.7;
        QTest::newRow("opencv edges vs halcon mask") << QStringLiteral("opencv edges") << 0.6 << QStringLiteral("halcon mask") << 0.7;
        QTest::newRow("opencv rect vs opencv edges") << QStringLiteral("opencv rect") << 0.8 << QStringLiteral("opencv edges") << 0.6;
        QTest::newRow("opencv whole vs halcon whole") << QStringLiteral("opencv whole") << 0.8 << QStringLiteral("halcon whole") << 0.75;
    }
    void agreement() {
        QFETCH(QString, a);
        QFETCH(double, thA);
        QFETCH(QString, b);
        QFETCH(double, thB);
        if (!predictions_.count(a) || !predictions_.count(b)) QSKIP("one of the rows did not run");
        std::vector<Point2D> d;
        for (const auto& [p, s] : predictions_[a]) {
            if (s < thA) continue;
            double best = 5.0;
            Point2D at{};
            bool found = false;
            for (const auto& [q, t] : predictions_[b]) {
                if (t < thB) continue;
                const double dist = distance(p, q);
                if (dist < best) {
                    best = dist;
                    at = {q.x - p.x, q.y - p.y};
                    found = true;
                }
            }
            if (found) d.push_back(at);
        }
        QVERIFY(!d.empty());
        Point2D mean{};
        for (const Point2D& v : d) {
            mean.x += v.x / static_cast<double>(d.size());
            mean.y += v.y / static_cast<double>(d.size());
        }
        std::vector<double> r;
        for (const Point2D& v : d) r.push_back(std::hypot(v.x - mean.x, v.y - mean.y));
        qInfo("%s (>= %.2f) vs %s (>= %.2f): %zu pairs, mean offset (%.2f, %.2f), residual p95 %.2f max %.2f px", qPrintable(a), thA,
              qPrintable(b), thB, d.size(), mean.x, mean.y, percentile95(r), *std::max_element(r.begin(), r.end()));
    }

    void cleanupTestCase() {
        if (results_.isEmpty()) return;
        QFile f(QStringLiteral("real-match-results.json"));
        if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(results_).toJson());
        qInfo("results: %s", qPrintable(QDir::current().absoluteFilePath(f.fileName())));
    }
};

QTEST_GUILESS_MAIN(PerfMatchRegionReal)
#include "perf_match_region_real.moc"
