// 流程 B 的樣本庫比對器（S08，HalconLibraryMatcher）才有的事：兩種做法（一次 find_shape_models、每個類型一次 find_shape_model）的結果相同。
// 合成板子上有兩種元件（大的、小的，交錯排）；候選類型的參考影像從另一塊同樣的板子裁下來。ILibraryMatcher 的契約（辨識正確率、排序、
// 位姿、樣本庫大小、區域、錯誤與取消）在 S09 的契約測試（tests/contract/tst_contract_library_matcher），對每個 back end 跑同一組。
#include <QtTest>
#include <algorithm>
#include <cmath>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "domain/image_buffer.h"
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
};

QTEST_GUILESS_MAIN(TstHalconLibraryMatcher)
#include "tst_halcon_library_matcher.moc"
