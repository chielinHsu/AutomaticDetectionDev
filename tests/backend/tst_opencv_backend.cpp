// OpenCV back end（S07）才有的事：金字塔的座標換算、小分數圖的 NCC、範本的金字塔與角度格點、影像的複製次數（ADR-014）、
// 轉 8 bit 時的有效位元數、工廠與執行緒數。port 的契約與合成板子上的準確度在 S09 的契約測試（tests/contract/），
// 對每個 back end 跑同一組；S07 這裡原本的那一份（契約的草稿）搬過去了。
#include <QtTest>
#include <algorithm>
#include <cmath>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "application/CancelToken.h"
#include "domain/image_copy.h"
#include "domain/image_depth.h"
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
