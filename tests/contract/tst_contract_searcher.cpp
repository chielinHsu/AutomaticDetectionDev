// 契約測試（S09）：ITemplateBuilder 與 IPatternSearcher 的契約（application/ports 的標頭），對組裝根註冊的每一個 back end
// 跑同一組（Liskov）。每個測試函式一個 back end 一列；沒編進程式或沒有授權的那一列 SKIPPED 並說原因。
//
// 兩種測試：
//  - 契約（每個 back end，含假的）：錯誤碼、分數範圍與排序、分數下限、最多幾個、角度範圍、搜尋區域、空結果、取消、可重入。
//  - 座標與角度的慣例、合成板子上的準確度（會讀像素的 back end）：容差在 ContractTolerances.h。
// S03 的 tst_fake_backend、S07 的 tst_opencv_backend、S08 的 tst_halcon_backend 是這份的草稿；共同的部分搬到這裡，
// 各自只留 back end 才有的事（金字塔、授權、位姿公式…）。
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

#include "BackendTestSupport.h"
#include "ContractSupport.h"
#include "QtTestPrinters.h"
#include "application/CancelToken.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::contract;
using namespace aoi::test;
using Clock = std::chrono::steady_clock;

namespace {

SyntheticBoardSpec cleanSpec(PixelFormat format = PixelFormat::Gray8) {
    SyntheticBoardSpec s;
    s.format = format;
    s.width = 1024;
    s.height = 768;
    s.rows = 6;
    s.cols = 10;
    s.seed = 3;
    return s;
}

const SyntheticBoard& cleanBoard() {
    static const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
    return board;
}

SearchParams params(double minScore = 0.7, double angle = 5.0) {
    SearchParams p;
    p.minScore = minScore;
    p.angleMinDeg = -angle;
    p.angleMaxDeg = angle;
    return p;
}

TemplatePtr buildFrom(const SearchEngine& e, const ImageView& image, const Rect& sample, const SearchParams& p) {
    Result<TemplatePtr> t = e.backend.templateBuilder->build(image, sample, p, CancelToken());
    if (!t.ok()) qWarning("%s: build failed: %s", e.backend.id.c_str(), t.error().message.c_str());
    return t.ok() ? t.value() : TemplatePtr();
}

TemplatePtr buildFrom(const SearchEngine& e, const SyntheticBoard& board, const SearchParams& p, std::size_t index = 0) {
    return buildFrom(e, board.image, sampleBoxAround(board, index), p);
}

Result<std::vector<Match>> search(const SearchEngine& e, const ImageView& image, const ITemplate& t, const SearchParams& p,
                                  const SearchRegion& region = SearchRegion::wholeImage(), const CancelToken& cancel = {}) {
    return e.backend.searcher->find(image, t, region, p, cancel);
}

std::vector<Match> findAll(const SearchEngine& e, const ImageView& image, const ITemplate& t, const SearchParams& p,
                           const SearchRegion& region = SearchRegion::wholeImage()) {
    Result<std::vector<Match>> r = search(e, image, t, p, region);
    if (!r.ok()) qWarning("%s: find failed: %s", e.backend.id.c_str(), r.error().message.c_str());
    return r.ok() ? r.value() : std::vector<Match>{};
}

bool anglesWithin(const std::vector<Match>& m, double lo, double hi) {
    return std::all_of(m.begin(), m.end(), [&](const Match& x) { return x.pose.angleDeg >= lo && x.pose.angleDeg <= hi; });
}

bool scoresWithin(const std::vector<Match>& m, double floor) {
    return std::all_of(m.begin(), m.end(), [&](const Match& x) { return x.score >= floor && x.score <= 1.0; });
}

bool insideSomeWindow(const Match& m, const std::vector<Rect>& windows) {
    return std::any_of(windows.begin(), windows.end(), [&](const Rect& w) { return w.contains(m.pose.position()); });
}

// 別家的範本，連 backendId 都冒用目標的名字：型別檢查要擋下（只比名字的搜尋器會把它當成自己的，未定義行為）。
struct Impostor final : ITemplate {
    explicit Impostor(std::string id) : id_(std::move(id)) {}
    Size2D size() const noexcept override { return {48.0, 28.0}; }
    std::string_view backendId() const noexcept override { return id_; }
    std::string id_;
};

// ---- 比對區（ADR-028）----
// 合成板子的元件：主體 40 × 20、兩端各一個電極（22% 寬），左右、上下都對稱。比對區偏右下（中心離主體中心 (7, 1) px）：
// 搜尋器回報的要是主體中心，不是比對區的中心。比對區要跨過中心、含三條直的邊（左電極｜主體、主體｜右電極、右電極｜底色）：
// 只框右半邊時，它的邊和左半邊的鏡像只差一個平移（兩個電極一樣），shape model 會在每個元件上找到兩次（這個測試的第一版實測，2026-10-15）。
enum class RegionKind { Rect, Mask, Edges };

const SyntheticPart& firstPart(const SyntheticBoard& board) {
    for (const SyntheticPart& p : board.parts) {
        if (p.present) return p;
    }
    return board.parts.front();
}

MatchSpec regionSpec(const SyntheticBoard& board, RegionKind kind) {
    const Pose2D c = firstPart(board).pose;
    MatchSpec spec;
    const Rect rect{c.x - 14.0, c.y - 12.0, 42.0, 26.0};
    spec.rect = rect;
    if (kind == RegionKind::Edges) spec.mode = MatchMode::Edges;
    if (kind == RegionKind::Mask) {
        // 外接矩形 42 × 26，主體中間挖一個 8 × 10 的洞（填滿約 93%：一定走遮罩的路，不會被當成矩形）。
        PixelMask m;
        m.x = static_cast<int>(std::lround(rect.x));
        m.y = static_cast<int>(std::lround(rect.y));
        m.width = 42;
        m.height = 26;
        m.bits.assign(static_cast<std::size_t>(m.width) * m.height, 1);
        for (int y = 8; y < 18; ++y)
            for (int x = 14; x < 22; ++x) m.bits[static_cast<std::size_t>(y) * m.width + x] = 0;
        spec.rect = m.bounds();
        spec.mask = std::move(m);
    }
    return spec;
}

double regionOffset(const SyntheticBoard& board, const MatchSpec& spec) {
    return distance(firstPart(board).pose.position(), spec.rect->center());
}

TemplatePtr buildRegion(const SearchEngine& e, const SyntheticBoard& board, const MatchSpec& spec, const SearchParams& p) {
    Result<TemplatePtr> t = e.backend.templateBuilder->buildFromSample(board.image, SampleSpec{sampleBoxAround(board, 0), spec}, p, CancelToken());
    if (!t.ok()) qWarning("%s: buildFromSample failed: %s", e.backend.id.c_str(), t.error().message.c_str());
    return t.ok() ? t.value() : TemplatePtr();
}

void addRegionRows() {
    QTest::addColumn<QString>("backend");
    QTest::addColumn<int>("kind");
    for (const std::string& id : backendIds(Rows::Vision)) {
        QTest::addRow("%s rect", id.c_str()) << QString::fromStdString(id) << static_cast<int>(RegionKind::Rect);
        QTest::addRow("%s mask", id.c_str()) << QString::fromStdString(id) << static_cast<int>(RegionKind::Mask);
        QTest::addRow("%s edges", id.c_str()) << QString::fromStdString(id) << static_cast<int>(RegionKind::Edges);
    }
}

double budgetLimit(const QString& id) {
    QFile f(QStringLiteral(AOI_SOURCE_DIR "/perf/budgets.json"));
    if (!f.open(QIODevice::ReadOnly)) return -1.0;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).object().value("budgets").toArray()) {
        if (v.toObject().value("id").toString() == id) return v.toObject().value("limit").toDouble(-1.0);
    }
    return -1.0;
}

QString describe(const std::string& id, const Accuracy& a) { return QString::fromStdString(id) + QStringLiteral(": ") + a.describe(); }

}  // namespace

class TstContractSearcher : public QObject {
    Q_OBJECT

private slots:
    // ---- 誰參與契約 ----
    // 組裝根註冊三個 back end（依序 fake、opencv、halcon）；沒編進程式的也要登記成「不可用」並附原因（這樣這份測試的那一列
    // 才會 SKIPPED，而不是根本沒有那一列）。
    void theRegistryListsEveryBackEnd() {
        std::vector<std::string> ids;
        for (const BackendInfo& info : registry().list()) {
            ids.push_back(info.id);
            const ContractEngine& e = engineFor(info.id);
            qInfo("%-6s %s", info.id.c_str(),
                  e.engine ? "runs the contract" : (e.skipReason.empty() ? e.failure.c_str() : ("SKIPPED: " + e.skipReason).c_str()));
            if (!info.available) QVERIFY2(!info.unavailable.message.empty(), info.id.c_str());
        }
        QCOMPARE(ids, (std::vector<std::string>{"fake", "opencv", "halcon"}));
        QVERIFY(engineFor("fake").engine);  // 假的永遠在
#ifdef AOI_WITH_OPENCV
        QVERIFY2(engineFor("opencv").engine, engineFor("opencv").failure.c_str());
#else
        QVERIFY(!engineFor("opencv").skipReason.empty());
#endif
#ifndef AOI_WITH_HALCON
        QVERIFY(!engineFor("halcon").skipReason.empty());
#endif
    }

    // 取消的容差和 I4 的預算是同一個數字（ContractTolerances.h），預算改了這裡要跟著改。
    void theCancelLatencyIsTheI4Budget() { QCOMPARE(kCancelLatencyMs, budgetLimit(QStringLiteral("I4"))); }

    // ---- ITemplateBuilder ----
    void theTemplateHasTheSampleSizeAndItsBackEnd_data() { addBackendRows(Rows::All); }
    void theTemplateHasTheSampleSizeAndItsBackEnd() {
        CONTRACT_BACKEND(be);
        const Rect sample{100.4, 98.6, 63.7, 32.2};  // 蓋住第一個元件；不是整數像素
        const TemplatePtr t = buildFrom(be, cleanBoard().image, sample, params());
        QVERIFY(t);
        // 範本的大小就是每個候選的主體框（use case 用它）：可以對齊到像素邊界，差不到一個像素。
        QVERIFY2(std::abs(t->size().w - sample.w) <= 1.0 && std::abs(t->size().h - sample.h) <= 1.0,
                 qPrintable(QStringLiteral("%1 x %2").arg(t->size().w).arg(t->size().h)));
        QVERIFY(t->backendId() == be.backend.id);
        QVERIFY(be.backend.templateBuilder->backendId() == be.backend.id);
        QVERIFY(be.backend.searcher->backendId() == be.backend.id);
        QVERIFY(!t->sampleProblem().has_value());
    }

    void theBuilderRefusesBadInput_data() { addBackendRows(Rows::All); }
    void theBuilderRefusesBadInput() {
        CONTRACT_BACKEND(be);
        ITemplateBuilder& builder = *be.backend.templateBuilder;
        const ImageView image = cleanBoard().image;
        const Rect sample = sampleBoxAround(cleanBoard());
        QCOMPARE_ERROR(builder.build(ImageView{}, sample, params(), CancelToken()), ErrorCode::InvalidArgument);
        QCOMPARE_ERROR(builder.build(image, {5000, 5000, 20, 20}, params(), CancelToken()), ErrorCode::InvalidArgument);
        QCOMPARE_ERROR(builder.build(image, {10, 10, NAN, 20}, params(), CancelToken()), ErrorCode::InvalidArgument);
        QCOMPARE_ERROR(builder.build(image, {10, 10, 20, -5}, params(), CancelToken()), ErrorCode::InvalidArgument);
        SearchParams backwards = params();
        std::swap(backwards.angleMinDeg, backwards.angleMaxDeg);
        QCOMPARE_ERROR(builder.build(image, sample, backwards, CancelToken()), ErrorCode::InvalidArgument);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_ERROR(builder.build(image, sample, params(), cancel.token()), ErrorCode::Cancelled);
    }

    // 平的樣本（只有底色）：建得出來，但範本知道自己沒用（LowContrast）；用它搜尋是成功、空的。
    void aFlatSampleIsLowContrast_data() { addBackendRows(Rows::Vision); }
    void aFlatSampleIsLowContrast() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard().image, {900.0, 650.0, 40.0, 30.0}, params());  // 右下角沒有元件
        QVERIFY(t);
        QVERIFY(t->sampleProblem() == NoMatchReason::LowContrast);
        const Result<std::vector<Match>> r = search(be, cleanBoard().image, *t, params());
        QVERIFY(r.ok());
        QVERIFY(r.value().empty());
    }

    // ---- IPatternSearcher：錯誤碼 ----
    void theSearcherRefusesBadInput_data() { addBackendRows(Rows::All); }
    void theSearcherRefusesBadInput() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        QVERIFY(t);
        const ImageView image = cleanBoard().image;
        QCOMPARE_ERROR(search(be, ImageView{}, *t, params()), ErrorCode::InvalidArgument);
        for (const double bad : {1.5, -0.1, std::numeric_limits<double>::quiet_NaN()}) {
            QCOMPARE_ERROR(search(be, image, *t, params(bad)), ErrorCode::InvalidArgument);
        }
        SearchParams backwards = params();
        std::swap(backwards.angleMinDeg, backwards.angleMaxDeg);
        QCOMPARE_ERROR(search(be, image, *t, backwards), ErrorCode::InvalidArgument);
        SearchParams nanAngle = params();
        nanAngle.angleMaxDeg = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE_ERROR(search(be, image, *t, nanAngle), ErrorCode::InvalidArgument);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_ERROR(search(be, image, *t, params(), SearchRegion::wholeImage(), cancel.token()), ErrorCode::Cancelled);
    }

    void aForeignTemplateIsRefused_data() { addBackendRows(Rows::All); }
    void aForeignTemplateIsRefused() {
        CONTRACT_BACKEND(be);
        const ImageView image = cleanBoard().image;
        const Impostor impostor(be.backend.id);
        QCOMPARE_ERROR(search(be, image, impostor, params()), ErrorCode::BackendMismatch);
        // 另一個（能用的）back end 建的範本。
        int others = 0;
        for (const std::string& id : backendIds(Rows::All)) {
            if (id == be.backend.id || !engineFor(id).engine) continue;
            const TemplatePtr foreign = buildFrom(*engineFor(id).engine, cleanBoard(), params());
            QVERIFY(foreign);
            QCOMPARE_ERROR(search(be, image, *foreign, params()), ErrorCode::BackendMismatch);
            ++others;
        }
        qInfo("templates from %d other back ends refused", others);
    }

    // ---- IPatternSearcher：結果的形狀 ----
    // 分數在 [minScore, 1]、依分數由高到低（同分依 y、再依 x）、角度在範圍內、中心在影像內。
    void matchesAreScoredSortedAndInsideTheImage_data() { addBackendRows(Rows::All); }
    void matchesAreScoredSortedAndInsideTheImage() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params(0.5));
        const std::vector<Match> found = findAll(be, cleanBoard().image, *t, params(0.5));
        QVERIFY(!found.empty());
        QVERIFY(scoresWithin(found, 0.5));
        QVERIFY(sortedByContract(found));
        QVERIFY(anglesWithin(found, -5.0, 5.0));
        const Rect image{0.0, 0.0, 1024.0, 768.0};
        for (const Match& m : found) QVERIFY(image.contains(m.pose.position()));
        if (beProfile.readsPixels) QCOMPARE(found.size(), cleanBoard().presentCount());
    }

    // 分數下限是硬的：每個匹配 ≥ minScore；下限越高，結果只會越少。
    void minScoreIsAHardFloor_data() { addBackendRows(Rows::All); }
    void minScoreIsAHardFloor() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params(0.3));
        std::size_t previous = std::numeric_limits<std::size_t>::max();
        for (const double floor : {0.3, 0.6, 0.8, 0.95, 1.0}) {
            const std::vector<Match> found = findAll(be, cleanBoard().image, *t, params(floor));
            QVERIFY2(scoresWithin(found, floor), qPrintable(QString::number(floor)));
            QVERIFY2(found.size() <= previous, qPrintable(QString::number(floor)));
            previous = found.size();
        }
    }

    // 最多 maxMatches 個，而且是分數最高的那幾個（和不限制時的前幾個同分）。
    void maxMatchesKeepsTheBestOnes_data() { addBackendRows(Rows::All); }
    void maxMatchesKeepsTheBestOnes() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params(0.5));
        const std::vector<Match> all = findAll(be, cleanBoard().image, *t, params(0.5));
        QVERIFY(all.size() > 7);
        SearchParams p = params(0.5);
        p.maxMatches = 7;
        const std::vector<Match> few = findAll(be, cleanBoard().image, *t, p);
        QCOMPARE(few.size(), std::size_t{7});
        QVERIFY(sortedByContract(few));
        for (std::size_t i = 0; i < few.size(); ++i) {
            QVERIFY2(std::abs(few[i].score - all[i].score) < 1e-9, qPrintable(QStringLiteral("%1: %2 vs %3").arg(i).arg(few[i].score).arg(all[i].score)));
        }
    }

    // 搜尋的角度範圍比範本窄：角度一定在範圍內。比範本寬：契約沒有規定要拒絕還是只回範圍內的——兩種都接受（ADR-004：
    // OpenCV、HALCON 回 InvalidArgument，假的照樣過濾；use case 永遠用同一組參數建範本與搜尋，看不到這個差別）。
    void theAngleRangeIsRespected_data() { addBackendRows(Rows::All); }
    void theAngleRangeIsRespected() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params(0.5, 5.0));
        SearchParams narrow = params(0.5);
        narrow.angleMinDeg = 0.0;
        narrow.angleMaxDeg = 2.0;
        QVERIFY(anglesWithin(findAll(be, cleanBoard().image, *t, narrow), 0.0, 2.0));
        const Result<std::vector<Match>> wider = search(be, cleanBoard().image, *t, params(0.5, 30.0));
        if (wider.ok()) QVERIFY(anglesWithin(wider.value(), -30.0, 30.0));
        else QCOMPARE_ERROR(wider, ErrorCode::InvalidArgument);
        qInfo("%s: a search range wider than the template's: %s", be.backend.id.c_str(), wider.ok() ? "filtered" : "InvalidArgument");
    }

    // ---- IPatternSearcher：搜尋區域 ----
    // 視窗搜尋只回中心在某個視窗內的匹配。會讀像素的：每個視窗（含 4×4 的小視窗）找到它的元件；元件中心在視窗外
    // 1.5 像素時，即使搜尋時看得到它，也不可以回傳（S08 的變異 M5b）。
    void aWindowSearchOnlyReturnsCentersInsideTheWindows_data() { addBackendRows(Rows::All); }
    void aWindowSearchOnlyReturnsCentersInsideTheWindows() {
        CONTRACT_BACKEND(be);
        const SyntheticBoard& board = cleanBoard();
        const TemplatePtr t = buildFrom(be, board, params());
        std::vector<Rect> windows;
        for (std::size_t k : {std::size_t{5}, std::size_t{17}, std::size_t{33}}) {
            windows.push_back(rectFromCenter(board.parts[k].pose.position(), {60.0, 30.0}));
        }
        std::vector<Rect> tiny;
        for (std::size_t k : {std::size_t{5}, std::size_t{17}}) tiny.push_back(rectFromCenter(board.parts[k].pose.position(), {4.0, 4.0}));
        std::vector<Rect> beside;
        for (std::size_t k : {std::size_t{5}, std::size_t{17}}) {
            const Point2D c = board.parts[k].pose.position();
            beside.push_back(Rect{c.x + 1.5, c.y - 10.0, 20.0, 20.0});  // 中心在左邊界的左邊 1.5 像素
        }
        for (const std::vector<Rect>* set : {&windows, &tiny, &beside}) {
            for (const Match& m : findAll(be, board.image, *t, params(0.3), SearchRegion::inWindows(*set))) {
                QVERIFY2(insideSomeWindow(m, *set), qPrintable(QStringLiteral("(%1, %2)").arg(m.pose.x).arg(m.pose.y)));
            }
        }
        if (beProfile.readsPixels) {
            QCOMPARE(findAll(be, board.image, *t, params(0.3), SearchRegion::inWindows(windows)).size(), windows.size());
            QCOMPARE(findAll(be, board.image, *t, params(0.3), SearchRegion::inWindows(tiny)).size(), tiny.size());
            QVERIFY(findAll(be, board.image, *t, params(0.3), SearchRegion::inWindows(beside)).empty());
        }
    }

    // 同一個元件，兩種區域（整張影像、視窗）給的是同一個位姿。
    void windowAndWholeImageAgree_data() { addBackendRows(Rows::Vision); }
    void windowAndWholeImageAgree() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const std::vector<Match> whole = findAll(be, cleanBoard().image, *t, params());
        std::vector<Rect> windows;
        for (const Match& m : whole) windows.push_back(rectFromCenter(m.pose.position(), {20.0, 20.0}));
        const std::vector<Match> windowed = findAll(be, cleanBoard().image, *t, params(), SearchRegion::inWindows(windows));
        QCOMPARE(windowed.size(), whole.size());
        double worst = 0.0;
        for (const Match& m : windowed) {
            double best = std::numeric_limits<double>::max();
            for (const Match& w : whole) best = std::min(best, distance(w.pose.position(), m.pose.position()));
            worst = std::max(worst, best);
        }
        qInfo("%s: window vs whole image: max %.4f px", be.backend.id.c_str(), worst);
        QVERIFY2(worst <= beProfile.searcher.windowVsWholePx, qPrintable(QString::number(worst)));
    }

    // ---- 空結果：成功、空的，不是錯誤 ----
    void noWindowsMeansNothingAndNoError_data() { addBackendRows(Rows::All); }
    void noWindowsMeansNothingAndNoError() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const Result<std::vector<Match>> r = search(be, cleanBoard().image, *t, params(0.0), SearchRegion::inWindows({}));
        QVERIFY(r.ok());
        QVERIFY(r.value().empty());
    }

    // 影像上真的沒有東西（每一格都缺件，只有底色）：成功、空的。
    void anEmptyBoardGivesAnEmptyResult_data() { addBackendRows(Rows::Vision); }
    void anEmptyBoardGivesAnEmptyResult() {
        CONTRACT_BACKEND(be);
        SyntheticBoardSpec s = cleanSpec();
        s.missingFraction = 1.0;
        const SyntheticBoard empty = makeSyntheticBoard(s);
        QCOMPARE(empty.presentCount(), std::size_t{0});
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const Result<std::vector<Match>> r = search(be, empty.image, *t, params());
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(r.value().empty());
    }

    // ---- 座標與角度的慣例（ADR-010）----
    // 原點：每個元件的中心和標準答案差不到 cleanPositionPx（差半個像素的慣例一定超過）。
    void aCleanBoardIsFoundCompletelyAndPrecisely_data() { addBackendRows(Rows::Vision); }
    void aCleanBoardIsFoundCompletelyAndPrecisely() {
        CONTRACT_BACKEND(be);
        const SearcherTolerances& tol = beProfile.searcher;
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const std::vector<Match> found = findAll(be, cleanBoard().image, *t, params());
        const Accuracy a = compareWithTruth(cleanBoard(), posesOf(found), 0.0);
        qInfo("%s", qPrintable(describe(be.backend.id, a)));
        QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        QVERIFY2(a.maxErrorPx < tol.cleanPositionPx, qPrintable(a.describe()));
        QVERIFY2(a.maxAngleErrorDeg < tol.cleanAngleDeg, qPrintable(a.describe()));
        QVERIFY(scoresWithin(found, tol.cleanMinScore));
    }

    // 子影像（原點在 (160, 128)、stride 比一列大）：找到的位置加上子影像的原點，就是整張影像上的位置。
    // 子影像的邊界都在兩排元件中間，沒有被切一半的元件。
    void aSubViewHasItsOwnOrigin_data() { addBackendRows(Rows::Vision); }
    void aSubViewHasItsOwnOrigin() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const std::vector<Match> whole = findAll(be, cleanBoard().image, *t, params());
        const Rect frame{160.0, 128.0, 560.0, 192.0};
        const ImageView part = subView(cleanBoard().image, 160, 128, 560, 192);
        QVERIFY(part.valid() && !part.packed());
        const std::vector<Match> found = findAll(be, part, *t, params());
        const auto expected = static_cast<std::size_t>(
            std::count_if(whole.begin(), whole.end(), [&](const Match& m) { return frame.contains(m.pose.position()); }));
        QCOMPARE(expected, std::size_t{21});  // 3 列 × 7 欄
        QCOMPARE(found.size(), expected);
        double worst = 0.0;
        for (const Match& m : found) {
            const Point2D shifted{m.pose.x + frame.x, m.pose.y + frame.y};
            double best = std::numeric_limits<double>::max();
            for (const Match& w : whole) best = std::min(best, distance(w.pose.position(), shifted));
            worst = std::max(worst, best);
        }
        qInfo("%s: sub-view vs whole image: max %.4f px", be.backend.id.c_str(), worst);
        QVERIFY2(worst <= beProfile.searcher.subViewPx, qPrintable(QString::number(worst)));
    }

    // 角度的正負號（螢幕上逆時針為正）：範本從不傾斜的板子建，在整個傾斜的板子上找。
    void theAngleSignFollowsTheConvention_data() {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<double>("tilt");
        for (const std::string& id : backendIds(Rows::Vision)) {
            for (const double tilt : {3.0, -3.0, 4.5}) {
                QTest::addRow("%s %+.1f", id.c_str(), tilt) << QString::fromStdString(id) << tilt;
            }
        }
    }
    void theAngleSignFollowsTheConvention() {
        CONTRACT_BACKEND(be);
        QFETCH(double, tilt);
        const SearcherTolerances& tol = beProfile.searcher;
        SyntheticBoardSpec s = cleanSpec();
        s.tiltDeg = tilt;
        const SyntheticBoard tilted = makeSyntheticBoard(s);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const Accuracy a = compareWithTruth(tilted, posesOf(findAll(be, tilted.image, *t, params())), tilt);
        qInfo("%s tilt %+.1f: %s", be.backend.id.c_str(), tilt, qPrintable(a.describe()));
        QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        QVERIFY2(a.maxErrorPx < tol.tiltedPositionPx, qPrintable(a.describe()));
        QVERIFY2(a.maxAngleErrorDeg < tol.tiltedAngleDeg, qPrintable(a.describe()));
    }

    // 角度掃一遍：範圍內每一個角度都找得回來（範本的角度格點不一定剛好有那個角度）。準確度用「範本角落的位移」衡量。
    void everyAngleInTheRangeIsRecovered_data() {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<double>("range");
        QTest::addColumn<double>("step");
        for (const std::string& id : backendIds(Rows::Vision)) {
            QTest::addRow("%s +-5", id.c_str()) << QString::fromStdString(id) << 5.0 << 0.5;
            QTest::addRow("%s +-30", id.c_str()) << QString::fromStdString(id) << 30.0 << 2.5;
        }
    }
    void everyAngleInTheRangeIsRecovered() {
        CONTRACT_BACKEND(be);
        QFETCH(double, range);
        QFETCH(double, step);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params(0.7, range));
        QVERIFY(t);
        const double halfDiagonal = 0.5 * std::hypot(t->size().w, t->size().h);
        double worstAngle = 0.0;
        double worstPx = 0.0;
        const int steps = static_cast<int>(std::lround(range / step));
        for (int k = -steps; k <= steps; ++k) {
            SyntheticBoardSpec s = cleanSpec();
            s.rows = 3;
            s.cols = 4;
            s.tiltDeg = step * k;
            const SyntheticBoard tilted = makeSyntheticBoard(s);
            const Accuracy a = compareWithTruth(tilted, posesOf(findAll(be, tilted.image, *t, params(0.7, range))), s.tiltDeg);
            const QString what = QString::number(s.tiltDeg) + QStringLiteral(": ") + a.describe();
            QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(what));
            QVERIFY2(degreesToRadians(a.maxAngleErrorDeg) * halfDiagonal <= beProfile.searcher.cornerShiftPx, qPrintable(what));
            worstAngle = std::max(worstAngle, a.maxAngleErrorDeg);
            worstPx = std::max(worstPx, a.maxErrorPx);
        }
        qInfo("%s +-%.0f deg: worst angle %.3f deg (corner %.3f px), worst position %.3f px", be.backend.id.c_str(), range, worstAngle,
              degreesToRadians(worstAngle) * halfDiagonal, worstPx);
    }

    // 三種正規格式（Gray8、12 bit 的 Gray16、Bgrx32）畫的是同一塊板子：找到的位姿與分數一樣。
    void allCanonicalFormatsGiveTheSameMatches_data() { addBackendRows(Rows::Vision); }
    void allCanonicalFormatsGiveTheSameMatches() {
        CONTRACT_BACKEND(be);
        const std::vector<Match> reference = findAll(be, cleanBoard().image, *buildFrom(be, cleanBoard(), params()), params());
        QVERIFY(!reference.empty());
        for (const PixelFormat f : {PixelFormat::Gray16, PixelFormat::Bgrx32}) {
            const SyntheticBoard board = makeSyntheticBoard(cleanSpec(f));
            const std::vector<Match> found = findAll(be, board.image, *buildFrom(be, board, params()), params());
            QCOMPARE(found.size(), reference.size());
            for (std::size_t i = 0; i < found.size(); ++i) {
                QVERIFY(distance(found[i].pose.position(), reference[i].pose.position()) < 1e-6);
                QVERIFY(std::abs(found[i].score - reference[i].score) < 1e-6);
            }
        }
    }

    // ---- 取消 ----
    // 搜尋進行中升起取消：在 kCancelLatencyMs（＝ I4）之內回 Cancelled。大的板子（12 MP）、±180°，不會在取消之前做完。
    // Debug 的時間沒有意義（沒最佳化、OpenCV 是 Debug 版），只印出來；Release、Profile 才把上限當成失敗條件。
    void cancellingDuringASearchStopsItInTime_data() { addBackendRows(Rows::Vision); }
    void cancellingDuringASearchStopsItInTime() {
        CONTRACT_BACKEND(be);
        SyntheticBoardSpec s = cleanSpec();
        s.width = 4000;
        s.height = 3000;
        s.rows = 30;
        s.cols = 40;
        const SyntheticBoard board = makeSyntheticBoard(s);
        const SearchParams wide = params(0.5, 180.0);
        const TemplatePtr t = buildFrom(be, board, wide);
        QVERIFY(t);
        CancelSource cancel;
        std::atomic<std::int64_t> cancelledAt{0};
        std::thread canceller([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            cancelledAt = Clock::now().time_since_epoch().count();
            cancel.cancel();
        });
        const Result<std::vector<Match>> r = search(be, board.image, *t, wide, SearchRegion::wholeImage(), cancel.token());
        const auto returned = Clock::now().time_since_epoch().count();
        canceller.join();
        QVERIFY2(!r.ok(), "the search finished before the cancel: make the board bigger");
        QCOMPARE_CODE(r.error().code, ErrorCode::Cancelled);
        const double latencyMs = std::chrono::duration<double, std::milli>(Clock::duration(returned - cancelledAt.load())).count();
        qInfo("%s: cancel to return %.1f ms (limit %.0f ms)", be.backend.id.c_str(), latencyMs, kCancelLatencyMs);
#ifdef AOI_PERF_ENFORCE
        QVERIFY2(latencyMs <= kCancelLatencyMs, qPrintable(QString::number(latencyMs)));
#endif
    }

    // ---- 比對區（ADR-028）----
    // 整框的規格（沒有比對區、灰階）＝ build(rect)：同一個範本，搜尋結果逐位元相同（向下相容的哨兵）。
    void aDefaultSpecMatchesTheRectBuild_data() { addBackendRows(Rows::All); }
    void aDefaultSpecMatchesTheRectBuild() {
        CONTRACT_BACKEND(be);
        const TemplatePtr a = buildFrom(be, cleanBoard(), params());
        Result<TemplatePtr> b = be.backend.templateBuilder->buildFromSample(cleanBoard().image, SampleSpec{sampleBoxAround(cleanBoard(), 0), {}},
                                                                              params(), CancelToken());
        QVERIFY(a && b.ok());
        QCOMPARE(b.value()->size().w, a->size().w);
        QCOMPARE(b.value()->size().h, a->size().h);
        const std::vector<Match> x = findAll(be, cleanBoard().image, *a, params());
        const std::vector<Match> y = findAll(be, cleanBoard().image, *b.value(), params());
        QVERIFY(!x.empty());
        QCOMPARE(y.size(), x.size());
        for (std::size_t i = 0; i < x.size(); ++i) {
            QCOMPARE(y[i].pose.x, x[i].pose.x);
            QCOMPARE(y[i].pose.y, x[i].pose.y);
            QCOMPARE(y[i].pose.angleDeg, x[i].pose.angleDeg);
            QCOMPARE(y[i].score, x[i].score);
        }
    }

    // 比對區和影像沒有交集：InvalidArgument（每個 back end，含不看影像的假 back end）。
    void aMatchRegionOutsideTheImageIsRefused_data() { addBackendRows(Rows::All); }
    void aMatchRegionOutsideTheImageIsRefused() {
        CONTRACT_BACKEND(be);
        MatchSpec spec;
        spec.rect = Rect{-500.0, -500.0, 20.0, 20.0};
        QCOMPARE_ERROR(be.backend.templateBuilder->buildFromSample(cleanBoard().image, SampleSpec{sampleBoxAround(cleanBoard(), 0), spec}, params(),
                                                                  CancelToken()),
                       ErrorCode::InvalidArgument);
    }

    // 範本只用比對區（矩形、遮罩、邊緣）：回報的是主體中心，範本大小仍是主體框。上限 ＝ 乾淨板子的上限 ＋ 角度誤差經過
    // 原點位移的槓桿（|o|·cleanAngleDeg，弧度）：比對區不在中心時，角度差一點，主體中心就差 |o| 倍。
    void aMatchRegionReportsTheBodyCenter_data() { addRegionRows(); }
    void aMatchRegionReportsTheBodyCenter() {
        CONTRACT_BACKEND(be);
        QFETCH(int, kind);
        const SearcherTolerances& tol = beProfile.searcher;
        const MatchSpec spec = regionSpec(cleanBoard(), static_cast<RegionKind>(kind));
        const TemplatePtr t = buildRegion(be, cleanBoard(), spec, params());
        QVERIFY(t);
        const Rect body = sampleBoxAround(cleanBoard(), 0);
        QVERIFY2(std::abs(t->size().w - body.w) <= 1.0 && std::abs(t->size().h - body.h) <= 1.0, "the template size is the body box");
        const std::vector<Match> found = findAll(be, cleanBoard().image, *t, params());
        const Accuracy a = compareWithTruth(cleanBoard(), posesOf(found), 0.0);
        const double limit = tol.cleanPositionPx + regionOffset(cleanBoard(), spec) * degreesToRadians(tol.cleanAngleDeg);
        qInfo("%s: %s (limit %.3f px)", be.backend.id.c_str(), qPrintable(a.describe()), limit);
        QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        QVERIFY2(a.maxErrorPx < limit, qPrintable(a.describe()));
        QVERIFY2(a.maxAngleErrorDeg < tol.cleanAngleDeg, qPrintable(a.describe()));
    }

    // 比對區不在中心、板子傾斜：位移跟著角度轉（rotate(o, θ)）。比對區比主體框小，角度的精度跟著範本變小（和
    // everyAngleInTheRangeIsRecovered 同一把尺：角度誤差 × 比對區的半對角線 ≤ cornerShiftPx）；位置的上限 ＝ 傾斜板子的上限
    // ＋ |o| × 這個角度上限（弧度）——角度差一點，主體中心就差 |o| 倍。
    void anOffsetRegionFollowsTheTilt_data() {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<double>("tilt");
        for (const std::string& id : backendIds(Rows::Vision)) {
            for (const double tilt : {4.5, -4.5, 30.0}) QTest::addRow("%s %+.1f", id.c_str(), tilt) << QString::fromStdString(id) << tilt;
        }
    }
    void anOffsetRegionFollowsTheTilt() {
        CONTRACT_BACKEND(be);
        QFETCH(double, tilt);
        const SearcherTolerances& tol = beProfile.searcher;
        const double range = std::abs(tilt) > 5.0 ? 30.0 : 5.0;
        const MatchSpec spec = regionSpec(cleanBoard(), RegionKind::Rect);
        const TemplatePtr t = buildRegion(be, cleanBoard(), spec, params(0.7, range));
        QVERIFY(t);
        SyntheticBoardSpec s = cleanSpec();
        s.tiltDeg = tilt;
        const SyntheticBoard tilted = makeSyntheticBoard(s);
        const Accuracy a = compareWithTruth(tilted, posesOf(findAll(be, tilted.image, *t, params(0.7, range))), tilt);
        const double halfDiagonal = 0.5 * std::hypot(spec.rect->w, spec.rect->h);
        const double angleLimitRad = std::max(degreesToRadians(tol.tiltedAngleDeg), tol.cornerShiftPx / halfDiagonal);
        const double limit = tol.tiltedPositionPx + regionOffset(cleanBoard(), spec) * angleLimitRad;
        qInfo("%s tilt %+.1f: %s (limit %.3f px, %.3f deg)", be.backend.id.c_str(), tilt, qPrintable(a.describe()), limit,
              radiansToDegrees(angleLimitRad));
        QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        QVERIFY2(a.maxErrorPx < limit, qPrintable(a.describe()));
        QVERIFY2(degreesToRadians(a.maxAngleErrorDeg) <= angleLimitRad, qPrintable(a.describe()));
    }

    // 視窗（以主體中心為準）與整張影像給同一個位姿：OpenCV 的邊距、HALCON 的參考點視窗（平移、放大）都要算對。
    void windowAndWholeImageAgreeWithAnOffset_data() { addRegionRows(); }
    void windowAndWholeImageAgreeWithAnOffset() {
        CONTRACT_BACKEND(be);
        QFETCH(int, kind);
        const SearchParams p = params(0.7, 30.0);
        const TemplatePtr t = buildRegion(be, cleanBoard(), regionSpec(cleanBoard(), static_cast<RegionKind>(kind)), p);
        QVERIFY(t);
        SyntheticBoardSpec s = cleanSpec();
        s.tiltDeg = 20.0;
        const SyntheticBoard tilted = makeSyntheticBoard(s);
        const std::vector<Match> whole = findAll(be, tilted.image, *t, p);
        QVERIFY(!whole.empty());
        std::vector<Rect> windows;
        for (const Match& m : whole) windows.push_back(rectFromCenter(m.pose.position(), {20.0, 20.0}));
        const std::vector<Match> windowed = findAll(be, tilted.image, *t, p, SearchRegion::inWindows(windows));
        QCOMPARE(windowed.size(), whole.size());
        double worst = 0.0;
        for (const Match& m : windowed) {
            double best = std::numeric_limits<double>::max();
            for (const Match& w : whole) best = std::min(best, distance(w.pose.position(), m.pose.position()));
            worst = std::max(worst, best);
        }
        qInfo("%s: window vs whole image with a match region: max %.4f px", be.backend.id.c_str(), worst);
        QVERIFY2(worst <= beProfile.searcher.windowVsWholePx, qPrintable(QString::number(worst)));
    }

    // 遮罩的範本一樣可以被幾個 worker 同時用（範本建好之後不變）。
    void severalThreadsShareAMaskedTemplate_data() { addBackendRows(Rows::Vision); }
    void severalThreadsShareAMaskedTemplate() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildRegion(be, cleanBoard(), regionSpec(cleanBoard(), RegionKind::Mask), params());
        QVERIFY(t);
        const std::vector<Match> alone = findAll(be, cleanBoard().image, *t, params());
        QVERIFY(!alone.empty());
        std::atomic<int> same{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&] {
                for (int k = 0; k < 3; ++k) {
                    const Result<std::vector<Match>> r = search(be, cleanBoard().image, *t, params());
                    if (r.ok() && r.value().size() == alone.size() &&
                        std::equal(r.value().begin(), r.value().end(), alone.begin(), [](const Match& a, const Match& b) {
                            return a.score == b.score && a.pose.x == b.pose.x && a.pose.y == b.pose.y && a.pose.angleDeg == b.pose.angleDeg;
                        })) {
                        ++same;
                    }
                }
            });
        }
        for (std::thread& th : threads) th.join();
        QCOMPARE(same.load(), 12);
    }

    // ---- 可重入：多個 worker 同時用同一個搜尋器與範本，結果和單獨跑一樣 ----
    void severalThreadsGetTheSameResult_data() { addBackendRows(Rows::All); }
    void severalThreadsGetTheSameResult() {
        CONTRACT_BACKEND(be);
        const TemplatePtr t = buildFrom(be, cleanBoard(), params());
        const std::vector<Match> alone = findAll(be, cleanBoard().image, *t, params());
        QVERIFY(!alone.empty());
        std::atomic<int> same{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&] {
                for (int k = 0; k < 5; ++k) {
                    const Result<std::vector<Match>> r = search(be, cleanBoard().image, *t, params());
                    if (r.ok() && r.value().size() == alone.size() &&
                        std::equal(r.value().begin(), r.value().end(), alone.begin(), [](const Match& a, const Match& b) {
                            return a.score == b.score && a.pose.x == b.pose.x && a.pose.y == b.pose.y && a.pose.angleDeg == b.pose.angleDeg;
                        })) {
                        ++same;
                    }
                }
            });
        }
        for (std::thread& th : threads) th.join();
        QCOMPARE(same.load(), 20);
    }
};

QTEST_GUILESS_MAIN(TstContractSearcher)
#include "tst_contract_searcher.moc"
