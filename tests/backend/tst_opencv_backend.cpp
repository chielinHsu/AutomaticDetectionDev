// OpenCV back end（S07）：範本建立器與搜尋器遵守 port 的契約（ITemplateBuilder.h、IPatternSearcher.h），
// 而且在合成板子上找得到、找得準。S09 會把契約的部分泛化成「對每個 back end 跑同一組」；這裡是草稿（S07 計畫的 L 原則）。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <thread>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "application/CancelToken.h"
#include "domain/image_copy.h"
#include "domain/image_depth.h"
#include "infrastructure/backend_fake/FakeBackend.h"
#include "infrastructure/backend_opencv/OpenCvBackend.h"
#include "infrastructure/backend_opencv/OpenCvCorrelation.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"
#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvPyramid.h"
#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"
#include "synthetic_board.h"

#include <opencv2/imgproc.hpp>

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

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

SearchParams params(double minScore = 0.7, double angle = 5.0) {
    SearchParams p;
    p.minScore = minScore;
    p.angleMinDeg = -angle;
    p.angleMaxDeg = angle;
    return p;
}

TemplatePtr buildFrom(const SyntheticBoard& board, const SearchParams& p, std::size_t index = 0) {
    OpenCvTemplateBuilder builder;
    Result<TemplatePtr> t = builder.build(board.image, sampleBoxAround(board, index), p, CancelToken());
    return t.ok() ? t.value() : TemplatePtr();
}

std::vector<Match> findAll(const SyntheticBoard& board, const ITemplate& t, const SearchParams& p,
                           const SearchRegion& region = SearchRegion::wholeImage()) {
    OpenCvPatternSearcher searcher;
    Result<std::vector<Match>> r = searcher.find(board.image, t, region, p, CancelToken());
    if (!r.ok()) qWarning("find failed: %s", r.error().message.c_str());
    return r.ok() ? r.value() : std::vector<Match>{};
}

}  // namespace

class TstOpenCvBackend : public QObject {
    Q_OBJECT

private slots:
    // ---- 座標換算（OpenCvPyramid.h）----
    void pyramidCoordinatesRoundTrip() {
        for (int level = 0; level <= 4; ++level) {
            for (const double x : {0.5, 1.0, 17.25, 1000.0}) {
                QVERIFY(std::abs(fromPyramidLevel(toPyramidLevel(x, level), level) - x) < 1e-12);
            }
        }
        // 第 1 層像素 0 的中心（0.5）以第 0 層像素 0 為中心（0.5）；第 1 層像素 1（1.5）對到第 0 層像素 2（2.5）。
        QCOMPARE(fromPyramidLevel(0.5, 1), 0.5);
        QCOMPARE(fromPyramidLevel(1.5, 1), 2.5);
        QCOMPARE(sideAtPyramidLevel(64, 2), 16);
        QCOMPARE(sideAtPyramidLevel(3, 3), 1);
    }

    // ---- 小區域的 NCC（OpenCvCorrelation.h）和 cv::matchTemplate 是同一個定義 ----
    void directCorrelationMatchesOpenCv() {
        cv::RNG rng(7);
        for (const cv::Size templSize : {cv::Size(16, 8), cv::Size(64, 32), cv::Size(33, 17)}) {
            cv::Mat patch(templSize.height + 6, templSize.width + 9, CV_8U);
            rng.fill(patch, cv::RNG::UNIFORM, 0, 256);
            cv::Mat templ(templSize, CV_8U);
            rng.fill(templ, cv::RNG::UNIFORM, 0, 256);
            patch(cv::Rect(4, 3, templSize.width, templSize.height)).setTo(cv::Scalar(77));  // 一個平的視窗：分數 0
            const double sum = cv::sum(templ)[0];
            const double centered = templ.dot(templ) - sum * sum / static_cast<double>(templ.total());
            cv::Mat mine;
            correlate(patch, integralsOf(patch), templ, sum, centered, mine);
            cv::Mat reference;
            cv::matchTemplate(patch, templ, reference, cv::TM_CCOEFF_NORMED);
            QCOMPARE(mine.size(), reference.size());
            double worst = 0.0;
            cv::minMaxLoc(cv::abs(mine - reference), nullptr, &worst);
            QVERIFY2(worst < 1e-4, qPrintable(QString::number(worst)));
            QCOMPARE(mine.at<float>(3, 4), 0.0f);
        }
        QVERIFY(correlateDirectly({68, 36}, {64, 32}));
        QVERIFY(!correlateDirectly({2592, 1944}, {64, 32}));
    }

    // ---- 範本建立器 ----
    void theTemplateHasTheSampleSizeAndAPyramid() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        OpenCvTemplateBuilder builder;
        QVERIFY(builder.backendId() == "opencv");
        const Result<TemplatePtr> t = builder.build(board.image, {100.4, 98.6, 63.7, 32.2}, params(), CancelToken());
        QVERIFY(t.ok());
        QCOMPARE(t.value()->size().w, 64.0);  // 四捨五入到像素邊界：100..164、99..131（蓋住第一個元件）
        QCOMPARE(t.value()->size().h, 32.0);
        QVERIFY(t.value()->backendId() == "opencv");
        const auto& ot = dynamic_cast<const OpenCvTemplate&>(*t.value());
        QCOMPARE(ot.topLevel(), 2);  // 64×32 → 32×16 → 16×8（短邊 8 ＝ minTopSidePx）
        QCOMPARE(ot.levels()[2].width, 16);
        QCOMPARE(ot.levels()[2].height, 8);
        // 角度：範圍的兩端都在；越上層步進越粗。
        for (const OpenCvTemplateLevel& level : ot.levels()) {
            QCOMPARE(level.anglesDeg.front(), -5.0);
            QCOMPARE(level.anglesDeg.back(), 5.0);
            QCOMPARE(level.rotated.size(), level.anglesDeg.size());
        }
        QVERIFY(ot.levels()[0].anglesDeg.size() > ot.levels()[2].anglesDeg.size());
    }

    void aZeroAngleRangeHasOneAngleAndAFullCircleDoesNotRepeatTheEnds() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        OpenCvTemplateBuilder builder;
        const Result<TemplatePtr> fixed = builder.build(board.image, sampleBoxAround(board), params(0.7, 0.0), CancelToken());
        QVERIFY(fixed.ok());
        for (const auto& level : dynamic_cast<const OpenCvTemplate&>(*fixed.value()).levels()) QCOMPARE(level.anglesDeg.size(), std::size_t{1});
        SearchParams circle = params();
        circle.angleMinDeg = -180.0;
        circle.angleMaxDeg = 180.0;
        const Result<TemplatePtr> full = builder.build(board.image, sampleBoxAround(board), circle, CancelToken());
        QVERIFY(full.ok());
        const auto& ft = dynamic_cast<const OpenCvTemplate&>(*full.value());
        QVERIFY(ft.fullCircle());
        const auto& top = ft.levels().back();
        QVERIFY(top.anglesDeg.back() < 180.0);  // −180 與 180 是同一個角度
        QVERIFY(std::abs(top.angleStepDeg * static_cast<double>(top.anglesDeg.size()) - 360.0) < 1e-9);
    }

    void aFlatSampleIsLowContrast() {
        SyntheticBoardSpec s = cleanSpec();
        s.rows = 1;
        s.cols = 1;
        const SyntheticBoard board = makeSyntheticBoard(s);
        OpenCvTemplateBuilder builder;
        // 板子右下角沒有元件，只有底色。
        const Result<TemplatePtr> t = builder.build(board.image, {900.0, 650.0, 40.0, 30.0}, params(), CancelToken());
        QVERIFY(t.ok());
        QVERIFY(t.value()->sampleProblem() == NoMatchReason::LowContrast);
        // 有問題的範本搜不到東西（成功、空的）。
        QVERIFY(findAll(board, *t.value(), params()).empty());
    }

    void theBuilderRefusesBadInput() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        OpenCvTemplateBuilder builder;
        QCOMPARE_CODE(builder.build(ImageView{}, {10, 10, 20, 20}, params(), CancelToken()).error().code, ErrorCode::InvalidArgument);
        QCOMPARE_CODE(builder.build(board.image, {5000, 5000, 20, 20}, params(), CancelToken()).error().code, ErrorCode::InvalidArgument);
        QCOMPARE_CODE(builder.build(board.image, {10, 10, NAN, 20}, params(), CancelToken()).error().code, ErrorCode::InvalidArgument);
        SearchParams backwards = params();
        std::swap(backwards.angleMinDeg, backwards.angleMaxDeg);
        QCOMPARE_CODE(builder.build(board.image, {10, 10, 20, 20}, backwards, CancelToken()).error().code, ErrorCode::InvalidArgument);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_CODE(builder.build(board.image, sampleBoxAround(board), params(), cancel.token()).error().code, ErrorCode::Cancelled);
    }

    // ---- 搜尋器：找得到、找得準（T6 的單元版本）----
    void aCleanBoardIsFoundCompletelyAndPrecisely() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(board, params());
        QVERIFY(t);
        const std::vector<Match> found = findAll(board, *t, params());
        const Accuracy a = compareWithTruth(board, posesOf(found), 0.0);
        QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        QVERIFY2(a.maxErrorPx < 0.25, qPrintable(a.describe()));
        QVERIFY2(a.maxAngleErrorDeg < 0.3, qPrintable(a.describe()));
        QVERIFY(sortedByContract(found));
        for (const Match& m : found) QVERIFY(m.score >= 0.9 && m.score <= 1.0);
    }

    // 角度的正負號（ADR-010：螢幕上逆時針為正）：範本從不傾斜的板子建，在傾斜 ±3° 的板子上找。
    void theAngleSignFollowsTheConvention_data() {
        QTest::addColumn<double>("tilt");
        QTest::newRow("+3") << 3.0;
        QTest::newRow("-3") << -3.0;
        QTest::newRow("+4.5") << 4.5;
    }
    void theAngleSignFollowsTheConvention() {
        QFETCH(double, tilt);
        const SyntheticBoard straight = makeSyntheticBoard(cleanSpec());
        SyntheticBoardSpec s = cleanSpec();
        s.tiltDeg = tilt;
        const SyntheticBoard tilted = makeSyntheticBoard(s);
        const TemplatePtr t = buildFrom(straight, params());
        const std::vector<Match> found = findAll(tilted, *t, params());
        const Accuracy a = compareWithTruth(tilted, posesOf(found), tilt);
        QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        QVERIFY2(a.maxErrorPx < 0.35, qPrintable(a.describe()));
        QVERIFY2(a.maxAngleErrorDeg < 0.35, qPrintable(a.describe()));
    }

    // 角度掃一遍（−5°..+5°，每 0.5°）：範本的格點不一定剛好有那個角度，頂層的估計也可能偏到另一邊；
    // 精修要靠爬坡與內插找回來（S07 的變異 M11）。角度的準確度以「範本角落的位移」衡量：誤差 × 半對角線 ≤ 0.3 像素
    // （這裡的範本 48×28：0.62°；實測最大約 0.42°，NCC 對角度的曲線在真值兩側不對稱，拋物線的頂點偏向 0°）。
    void everyAngleInTheRangeIsRecovered() {
        const SyntheticBoard straight = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(straight, params());
        const double halfDiagonal = 0.5 * std::hypot(t->size().w, t->size().h);
        double worst = 0.0;
        for (int k = -10; k <= 10; ++k) {
            SyntheticBoardSpec s = cleanSpec();
            s.rows = 3;
            s.cols = 4;
            s.tiltDeg = 0.5 * k;
            const SyntheticBoard tilted = makeSyntheticBoard(s);
            const std::vector<Match> found = findAll(tilted, *t, params());
            const Accuracy a = compareWithTruth(tilted, posesOf(found), s.tiltDeg);
            QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(QString::number(s.tiltDeg) + ": " + a.describe()));
            worst = std::max(worst, a.maxAngleErrorDeg);
            QVERIFY2(degreesToRadians(a.maxAngleErrorDeg) * halfDiagonal <= 0.3, qPrintable(QString::number(s.tiltDeg) + ": " + a.describe()));
        }
        qInfo("worst angle error over -5..5 deg: %.3f deg", worst);
    }

    // 寬的角度範圍（±30°）：頂層的步進粗（6.4°），估計可能差不只一步。
    void aWideAngleRangeIsRecoveredToo() {
        const SyntheticBoard straight = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(straight, params(0.7, 30.0));
        const double halfDiagonal = 0.5 * std::hypot(t->size().w, t->size().h);
        double worst = 0.0;
        for (int k = -12; k <= 12; ++k) {
            SyntheticBoardSpec s = cleanSpec();
            s.rows = 3;
            s.cols = 4;
            s.tiltDeg = 2.5 * k;
            const SyntheticBoard tilted = makeSyntheticBoard(s);
            const std::vector<Match> found = findAll(tilted, *t, params(0.7, 30.0));
            const Accuracy a = compareWithTruth(tilted, posesOf(found), s.tiltDeg);
            QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(QString::number(s.tiltDeg) + ": " + a.describe()));
            worst = std::max(worst, a.maxAngleErrorDeg);
            QVERIFY2(degreesToRadians(a.maxAngleErrorDeg) * halfDiagonal <= 0.3, qPrintable(QString::number(s.tiltDeg) + ": " + a.describe()));
        }
        qInfo("worst angle error over -30..30 deg: %.3f deg", worst);
    }

    // 三種正規格式（Gray8、12 bit 的 Gray16、Bgrx32）畫的是同一塊板子：找到的位姿一樣。
    void allCanonicalFormatsGiveTheSameMatches() {
        const SyntheticBoard gray = makeSyntheticBoard(cleanSpec());
        const std::vector<Match> reference = findAll(gray, *buildFrom(gray, params()), params());
        QVERIFY(!reference.empty());
        for (const PixelFormat f : {PixelFormat::Gray16, PixelFormat::Bgrx32}) {
            const SyntheticBoard board = makeSyntheticBoard(cleanSpec(f));
            const std::vector<Match> found = findAll(board, *buildFrom(board, params()), params());
            QCOMPARE(found.size(), reference.size());
            for (std::size_t i = 0; i < found.size(); ++i) {
                QVERIFY(distance(found[i].pose.position(), reference[i].pose.position()) < 1e-6);
                QVERIFY(std::abs(found[i].score - reference[i].score) < 1e-6);
            }
        }
    }

    // ---- 契約（IPatternSearcher.h）----
    void windowSearchOnlyReturnsCentersInsideTheWindows() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(board, params());
        std::vector<Rect> windows;
        for (std::size_t k : {std::size_t{5}, std::size_t{17}, std::size_t{33}}) {
            windows.push_back(rectFromCenter(board.parts[k].pose.position(), {60.0, 30.0}));
        }
        const std::vector<Match> found = findAll(board, *t, params(0.3), SearchRegion::inWindows(windows));
        QCOMPARE(found.size(), windows.size());
        for (const Match& m : found) {
            QVERIFY(std::any_of(windows.begin(), windows.end(), [&](const Rect& w) { return w.contains(m.pose.position()); }));
        }
        // 視窗外面有分數一樣高的元件，但不在結果裡。
        QVERIFY(findAll(board, *t, params(0.3), SearchRegion::inWindows({})).empty());
    }

    void minScoreMaxMatchesAndAngleRangeAreRespected() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(board, params(0.5, 5.0));
        SearchParams p = params(0.5, 5.0);
        p.maxMatches = 7;
        const std::vector<Match> few = findAll(board, *t, p);
        QCOMPARE(few.size(), std::size_t{7});
        QVERIFY(sortedByContract(few));
        // 範本建了 ±5°，搜尋只要 0..2°：角度一定在範圍內。
        SearchParams narrow = params(0.5, 5.0);
        narrow.angleMinDeg = 0.0;
        narrow.angleMaxDeg = 2.0;
        for (const Match& m : findAll(board, *t, narrow)) QVERIFY(m.pose.angleDeg >= 0.0 && m.pose.angleDeg <= 2.0);
        // 分數下限 1.0：只有完全相同的（樣本自己）才可能，而且分數 ≤ 1。
        for (const Match& m : findAll(board, *t, params(1.0, 5.0))) QVERIFY(m.score >= 1.0 && m.score <= 1.0);
    }

    void theSearcherRefusesBadInputAndForeignTemplates() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(board, params());
        OpenCvPatternSearcher searcher;
        QVERIFY(searcher.backendId() == "opencv");
        QCOMPARE_CODE(searcher.find(ImageView{}, *t, SearchRegion::wholeImage(), params(), CancelToken()).error().code,
                      ErrorCode::InvalidArgument);
        QCOMPARE_CODE(searcher.find(board.image, *t, SearchRegion::wholeImage(), params(1.5), CancelToken()).error().code,
                      ErrorCode::InvalidArgument);
        // 範本只建了 ±5°：要搜 ±30° 是錯的輸入，不是默默只搜一部分。
        QCOMPARE_CODE(searcher.find(board.image, *t, SearchRegion::wholeImage(), params(0.7, 30.0), CancelToken()).error().code,
                      ErrorCode::InvalidArgument);
        fake::FakeTemplateBuilder fakeBuilder;
        const Result<TemplatePtr> foreign = fakeBuilder.build(board.image, sampleBoxAround(board), params(), CancelToken());
        QVERIFY(foreign.ok());
        QCOMPARE_CODE(searcher.find(board.image, *foreign.value(), SearchRegion::wholeImage(), params(), CancelToken()).error().code,
                      ErrorCode::BackendMismatch);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_CODE(searcher.find(board.image, *t, SearchRegion::wholeImage(), params(), cancel.token()).error().code,
                      ErrorCode::Cancelled);
    }

    void cancellingDuringASearchStopsIt() {
        SyntheticBoardSpec s = cleanSpec();
        s.width = 4000;
        s.height = 3000;
        s.rows = 30;
        s.cols = 40;
        const SyntheticBoard board = makeSyntheticBoard(s);
        SearchParams wide = params(0.5, 180.0);
        const TemplatePtr t = buildFrom(board, wide);
        CancelSource cancel;
        std::thread canceller([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            cancel.cancel();
        });
        OpenCvPatternSearcher searcher;
        const Result<std::vector<Match>> r = searcher.find(board.image, *t, SearchRegion::wholeImage(), wide, cancel.token());
        canceller.join();
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::Cancelled);
    }

    // 可重入：兩個執行緒同時用同一個範本、同一個搜尋器，結果和單獨跑一樣。
    void concurrentSearchesGiveTheSameResult() {
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(board, params());
        OpenCvPatternSearcher searcher;
        const std::vector<Match> alone = findAll(board, *t, params());
        std::vector<Match> a;
        std::vector<Match> b;
        std::thread ta([&] { a = searcher.find(board.image, *t, SearchRegion::wholeImage(), params(), CancelToken()).value(); });
        std::thread tb([&] { b = searcher.find(board.image, *t, SearchRegion::wholeImage(), params(), CancelToken()).value(); });
        ta.join();
        tb.join();
        QCOMPARE(a.size(), alone.size());
        QCOMPARE(b.size(), alone.size());
        for (std::size_t i = 0; i < alone.size(); ++i) {
            QVERIFY(distance(a[i].pose.position(), alone[i].pose.position()) == 0.0);
            QVERIFY(distance(b[i].pose.position(), alone[i].pose.position()) == 0.0);
        }
    }

    // ---- 影像複製（ADR-014 規則 6）----
    void gray8IsSearchedWithoutCopiesAndGray16WithOne() {
        const SyntheticBoard gray = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t8 = buildFrom(gray, params());
        ImageCopyCount before = imageCopyCount();
        (void)findAll(gray, *t8, params());
        QCOMPARE((imageCopyCount() - before).copies, std::uint64_t{0});

        const SyntheticBoard deep = makeSyntheticBoard(cleanSpec(PixelFormat::Gray16));
        const TemplatePtr t16 = buildFrom(deep, params());
        before = imageCopyCount();
        (void)findAll(deep, *t16, params());
        const ImageCopyCount d = imageCopyCount() - before;
        QCOMPARE(d.copies, std::uint64_t{1});
        QCOMPARE(d.bytes, std::uint64_t{1024 * 768});
    }

    // ---- 有效位元數（S07：配方或使用者的覆寫）----
    // 暗的 12 bit 影像（最大值 ≤ 1023）被推測成 10 bit。轉 8 bit 給 matchTemplate 時用的是 view 的位元數：
    // 推測的 10 bit 會把亮度放大 4 倍，覆寫成 12 bit 才是相機真正的亮度。NCC 不受亮度的倍數影響，兩種都找得到。
    void theGrayConversionUsesTheSignificantBits() {
        SyntheticBoardSpec s = cleanSpec(PixelFormat::Gray16);
        s.grayBits = 12;
        s.backgroundLevel = 15;
        s.bodyLevel = 35;
        s.padLevel = 60;
        const SyntheticBoard board = makeSyntheticBoard(s);
        ImageView guessed = board.image;
        guessed.significantBits = detectSignificantBits(guessed);
        QCOMPARE(guessed.significantBits, 10);  // 推測錯了：其實是 12 bit
        ImageView told = board.image;
        told.significantBits = 12;

        const cv::Rect pad(0, 0, 4, 4);  // 左上角是底色
        const GrayRoi fromGuess = toGray8(guessed, pad);
        const GrayRoi fromTruth = toGray8(told, pad);
        // 底色 15（8 bit）存成 12 bit 是 241；推測 10 bit → 241 × 255 ／ 1023 ≈ 60（亮了 4 倍），12 bit → 15。
        QCOMPARE(static_cast<int>(fromTruth.mat.at<std::uint8_t>(0, 0)), 15);
        QCOMPARE(static_cast<int>(fromGuess.mat.at<std::uint8_t>(0, 0)), 60);

        for (const ImageView& view : {guessed, told}) {
            OpenCvTemplateBuilder builder;
            const Result<TemplatePtr> t = builder.build(view, sampleBoxAround(board), params(), CancelToken());
            QVERIFY(t.ok() && !t.value()->sampleProblem());
            OpenCvPatternSearcher searcher;
            const Result<std::vector<Match>> r = searcher.find(view, *t.value(), SearchRegion::wholeImage(), params(), CancelToken());
            QVERIFY(r.ok());
            const Accuracy a = compareWithTruth(board, posesOf(r.value()), 0.0);
            QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        }
    }

    // ---- 工廠 ----
    void theFactoryBuildsAMatchingPair() {
        const Backend b = makeOpenCvBackend();
        QCOMPARE(b.id, std::string("opencv"));
        QVERIFY(b.templateBuilder && b.searcher);
        QVERIFY(b.templateBuilder->backendId() == b.searcher->backendId());
        QVERIFY(setOpenCvThreads(2) == 2);
        QVERIFY(setOpenCvThreads(0) == 2);  // 0 ＝ 不改
    }
};

QTEST_GUILESS_MAIN(TstOpenCvBackend)
#include "tst_opencv_backend.moc"
