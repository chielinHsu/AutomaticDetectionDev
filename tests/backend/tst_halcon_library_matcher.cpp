// 流程 B 的樣本庫比對器（S08，HalconLibraryMatcher）：契約見 application/ports/ILibraryMatcher.h。和 tst_opencv_library_matcher 同一組情境；
// 另外比較兩種做法（一次 find_shape_models、每個類型一次 find_shape_model）。合成板子上有兩種元件（大的、小的，交錯排）；候選類型的參考影像從另一塊同樣的板子裁下來。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <cmath>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "domain/image_buffer.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#include "infrastructure/backend_halcon/HalconLibraryMatcher.h"
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

class TstHalconLibraryMatcher : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { REQUIRE_HALCON(); }

    // 兩種做法（一次 find_shape_models、每個類型一次 find_shape_model）給同樣的元件：說明說「多個值的 MaxOverlap 和
    // 分開呼叫的效果相同」，這裡實測（ADR-006 預期的做法，S08 確認）。
    void oneCallAndOneCallPerTypeAgree() {
        const TwoKindBoard board = makeTwoKindBoard();
        const Rect region{0.0, 0.0, 1280.0, 480.0};
        const std::vector<ComponentType> types{typeOf("A", true), typeOf("B", false)};
        HalconSearchOptions perType;
        perType.libraryOneCall = false;
        const Result<LibraryMatchResult> one = HalconLibraryMatcher().match(board.image, region, types, params(), CancelToken(), nullptr);
        const Result<LibraryMatchResult> each = HalconLibraryMatcher(perType).match(board.image, region, types, params(), CancelToken(), nullptr);
        QVERIFY(one.ok() && each.ok());
        QCOMPARE(one.value().components.size(), each.value().components.size());
        QCOMPARE(one.value().samplesCompared, each.value().samplesCompared);
        double worst = 0.0;
        for (std::size_t i = 0; i < one.value().components.size(); ++i) {
            const ComponentCandidate& a = one.value().components[i];
            const ComponentCandidate& b = each.value().components[i];
            worst = std::max(worst, distance(a.pose.position(), b.pose.position()));
            QCOMPARE(a.typeScores.size(), b.typeScores.size());
            QCOMPARE(a.typeScores.front().typeId, b.typeScores.front().typeId);
        }
        qInfo("one call vs one call per type: %zu components, max position difference %.4f px", one.value().components.size(), worst);
        QVERIFY2(worst < 0.01, qPrintable(QString::number(worst)));
    }

    void eachComponentGetsTheRightTypeFirst() {
        const TwoKindBoard board = makeTwoKindBoard();
        HalconLibraryMatcher matcher;
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
        HalconLibraryMatcher matcher;
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
        const HalconGray source = toHalconGray(big.referenceImage, PixelRect{0, 0, big.referenceImage.width, big.referenceImage.height}, false);
        big.referenceImage = viewOfHalconImage(source.image.ZoomImageFactor(2.0, 2.0, "bilinear"), 5.0);
        QVERIFY(big.referenceImage.valid());
        HalconLibraryMatcher matcher;
        const Result<LibraryMatchResult> r = matcher.match(board.image, {0.0, 0.0, 640.0, 480.0}, {big}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        QCOMPARE(nearCount(r.value().components, board.kindA, "A2x"), board.kindA.size());
    }

    void emptyOrUnusableCandidatesMeanAnEmptyLibrary() {
        const TwoKindBoard board = makeTwoKindBoard();
        HalconLibraryMatcher matcher;
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
        HalconLibraryMatcher matcher;
        const Result<LibraryMatchResult> r =
            matcher.match(board.image, {0.0, 0.0, 640.0, 480.0}, {typeOf("B", false)}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        QCOMPARE(r.value().samplesCompared, std::size_t{1});
        QVERIFY(r.value().components.empty());
        QVERIFY(r.value().noMatch && r.value().noMatch->reason == NoMatchReason::NoLibraryMatch);
    }

    void badInputAndCancel() {
        const TwoKindBoard board = makeTwoKindBoard();
        HalconLibraryMatcher matcher;
        QCOMPARE_CODE(matcher.match(ImageView{}, {0, 0, 10, 10}, {}, params(), CancelToken(), nullptr).error().code, ErrorCode::InvalidArgument);
        QCOMPARE_CODE(matcher.match(board.image, {5000, 5000, 10, 10}, {}, params(), CancelToken(), nullptr).error().code,
                      ErrorCode::InvalidArgument);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_CODE(matcher.match(board.image, {0, 0, 640, 480}, {typeOf("A", true)}, params(), cancel.token(), nullptr).error().code,
                      ErrorCode::Cancelled);
    }
};

QTEST_GUILESS_MAIN(TstHalconLibraryMatcher)
#include "tst_halcon_library_matcher.moc"
