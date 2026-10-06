// 流程 B 的樣本庫比對器（S07，OpenCvLibraryMatcher）：契約見 application/ports/ILibraryMatcher.h。
// 合成板子上有兩種元件（大的、小的，交錯排）；候選類型的參考影像從另一塊同樣的板子裁下來。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <opencv2/imgproc.hpp>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "domain/image_buffer.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"
#include "infrastructure/backend_opencv/OpenCvLibraryMatcher.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

// 兩種元件：A（主體 48×20、電極亮）與 B（主體 24×24、電極暗），各畫在自己的板子上，再拼成一張：左半 A、右半 B。
struct TwoKindBoard {
    ImageBuffer buffer;
    ImageView image;
    std::vector<Pose2D> kindA;
    std::vector<Pose2D> kindB;
};

SyntheticBoardSpec kindSpec(bool a) {
    SyntheticBoardSpec s;
    s.width = 640;
    s.height = 480;
    s.rows = 6;
    s.cols = 5;
    s.pitchXPx = 100.0;
    s.pitchYPx = 70.0;
    s.originXPx = 90.0;
    s.originYPx = 70.0;
    s.bodyWidthPx = a ? 48.0 : 24.0;
    s.bodyHeightPx = a ? 20.0 : 24.0;
    s.bodyLevel = a ? 150 : 120;
    s.padLevel = a ? 225 : 40;
    s.seed = a ? 3 : 4;
    return s;
}

TwoKindBoard makeTwoKindBoard() {
    const SyntheticBoard a = makeSyntheticBoard(kindSpec(true));
    const SyntheticBoard b = makeSyntheticBoard(kindSpec(false));
    TwoKindBoard out;
    out.buffer = ImageBuffer(1280, 480, PixelFormat::Gray8);
    for (int y = 0; y < 480; ++y) {
        std::copy(a.image.data + y * a.image.strideBytes, a.image.data + y * a.image.strideBytes + 640, out.buffer.row(y));
        std::copy(b.image.data + y * b.image.strideBytes, b.image.data + y * b.image.strideBytes + 640, out.buffer.row(y) + 640);
    }
    out.image = out.buffer.view(10.0);  // 10 µm／像素
    for (const SyntheticPart& p : a.parts) out.kindA.push_back(p.pose);
    for (const SyntheticPart& p : b.parts) out.kindB.push_back({p.pose.x + 640.0, p.pose.y, p.pose.angleDeg});
    return out;
}

// 一個類型：參考影像是那種元件（主體＋每邊 4 像素），校正值 umPerPixel。
ComponentType typeOf(const std::string& id, bool a, double umPerPixel = 10.0) {
    const SyntheticBoard board = makeSyntheticBoard(kindSpec(a));
    const Rect box = sampleBoxAround(board);
    ComponentType t;
    t.id = id;
    t.referenceImage = subView(board.image, static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w),
                               static_cast<int>(box.h));
    t.referenceImage.umPerPixel = umPerPixel;
    return t;
}

SearchParams params() {
    SearchParams p;
    p.minScore = 0.75;
    p.angleMinDeg = -5.0;
    p.angleMaxDeg = 5.0;
    return p;
}

std::size_t nearCount(const std::vector<ComponentCandidate>& found, const std::vector<Pose2D>& truth, const std::string& typeId) {
    std::size_t n = 0;
    for (const Pose2D& t : truth) {
        for (const ComponentCandidate& c : found) {
            if (distance(c.pose.position(), t.position()) <= 2.0 && !c.typeScores.empty() && c.typeScores.front().typeId == typeId) {
                ++n;
                break;
            }
        }
    }
    return n;
}

class CountingProgress final : public IProgressSink {
public:
    void report(const ProgressEvent& e) noexcept override {
        if (e.stage == Stage::Match) {
            ++count;
            last = e.fraction;
        }
    }
    std::atomic<int> count{0};
    double last = 0.0;
};

}  // namespace

class TstOpenCvLibraryMatcher : public QObject {
    Q_OBJECT

private slots:
    void eachComponentGetsTheRightTypeFirst() {
        const TwoKindBoard board = makeTwoKindBoard();
        OpenCvLibraryMatcher matcher;
        CountingProgress progress;
        const Rect region{0.0, 0.0, 1280.0, 480.0};
        const Result<LibraryMatchResult> r =
            matcher.match(board.image, region, {typeOf("A", true), typeOf("B", false)}, params(), CancelToken(), &progress);
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const LibraryMatchResult& m = r.value();
        QCOMPARE(m.samplesCompared, std::size_t{2});
        QVERIFY(!m.noMatch);
        QCOMPARE(nearCount(m.components, board.kindA, "A"), board.kindA.size());
        QCOMPARE(nearCount(m.components, board.kindB, "B"), board.kindB.size());
        for (const ComponentCandidate& c : m.components) {
            QVERIFY(!c.typeScores.empty());
            for (std::size_t i = 1; i < c.typeScores.size(); ++i) QVERIFY(c.typeScores[i - 1].score >= c.typeScores[i].score);
            for (const TypeScore& s : c.typeScores) QVERIFY(s.score >= params().minScore && s.score <= 1.0);
        }
        QCOMPARE(progress.count.load(), 2);
        QCOMPARE(progress.last, 1.0);
    }

    void onlyTheRegionIsScanned() {
        const TwoKindBoard board = makeTwoKindBoard();
        OpenCvLibraryMatcher matcher;
        const Rect leftHalf{0.0, 0.0, 640.0, 480.0};
        const Result<LibraryMatchResult> r =
            matcher.match(board.image, leftHalf, {typeOf("A", true), typeOf("B", false)}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        for (const ComponentCandidate& c : r.value().components) QVERIFY(leftHalf.contains(c.pose.position()));
        QCOMPARE(nearCount(r.value().components, board.kindA, "A"), board.kindA.size());
    }

    // 參考影像的校正值不同（5 µm／像素，看起來大一倍）：先縮放成板子的比例再比。
    void aReferenceAtAnotherScaleIsResized() {
        const TwoKindBoard board = makeTwoKindBoard();
        ComponentType big = typeOf("A2x", true, 5.0);
        cv::Mat doubled;
        const MatView source = borrowAsMat(big.referenceImage);
        cv::resize(source.mat, doubled, cv::Size(), 2.0, 2.0, cv::INTER_LINEAR);
        big.referenceImage = fromMat(doubled, 5.0);
        OpenCvLibraryMatcher matcher;
        const Result<LibraryMatchResult> r = matcher.match(board.image, {0.0, 0.0, 640.0, 480.0}, {big}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        QCOMPARE(nearCount(r.value().components, board.kindA, "A2x"), board.kindA.size());
    }

    void emptyOrUnusableCandidatesMeanAnEmptyLibrary() {
        const TwoKindBoard board = makeTwoKindBoard();
        OpenCvLibraryMatcher matcher;
        const Result<LibraryMatchResult> none = matcher.match(board.image, {0, 0, 640, 480}, {}, params(), CancelToken(), nullptr);
        QVERIFY(none.ok());
        QVERIFY(none.value().noMatch && none.value().noMatch->reason == NoMatchReason::LibraryEmpty);
        ComponentType noImage;
        noImage.id = "x";
        const ComponentType tooBig = typeOf("A", true);
        const Result<LibraryMatchResult> unusable =
            matcher.match(board.image, {0, 0, 20, 20}, {noImage, tooBig}, params(), CancelToken(), nullptr);
        QVERIFY(unusable.ok());
        QCOMPARE(unusable.value().samplesCompared, std::size_t{0});  // 沒有參考影像、比區域大：都沒有比對
        QVERIFY(unusable.value().noMatch->reason == NoMatchReason::LibraryEmpty);
    }

    void aTypeThatIsNotThereGivesNoLibraryMatch() {
        const TwoKindBoard board = makeTwoKindBoard();
        OpenCvLibraryMatcher matcher;
        const Result<LibraryMatchResult> r =
            matcher.match(board.image, {0.0, 0.0, 640.0, 480.0}, {typeOf("B", false)}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        QCOMPARE(r.value().samplesCompared, std::size_t{1});
        QVERIFY(r.value().components.empty());
        QVERIFY(r.value().noMatch && r.value().noMatch->reason == NoMatchReason::NoLibraryMatch);
    }

    void badInputAndCancel() {
        const TwoKindBoard board = makeTwoKindBoard();
        OpenCvLibraryMatcher matcher;
        QCOMPARE_CODE(matcher.match(ImageView{}, {0, 0, 10, 10}, {}, params(), CancelToken(), nullptr).error().code, ErrorCode::InvalidArgument);
        QCOMPARE_CODE(matcher.match(board.image, {5000, 5000, 10, 10}, {}, params(), CancelToken(), nullptr).error().code,
                      ErrorCode::InvalidArgument);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_CODE(matcher.match(board.image, {0, 0, 640, 480}, {typeOf("A", true)}, params(), cancel.token(), nullptr).error().code,
                      ErrorCode::Cancelled);
    }
};

QTEST_GUILESS_MAIN(TstOpenCvLibraryMatcher)
#include "tst_opencv_library_matcher.moc"
