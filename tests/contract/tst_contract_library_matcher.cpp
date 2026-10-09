// 契約測試（S09）：流程 B 的樣本庫比對器（ILibraryMatcher，application/ports/ILibraryMatcher.h），對每個有實作的 back end
// 跑同一組：辨識正確率（每個元件的第一個類型是對的）、候選排序、位姿誤差、樣本庫大小 N 的行為、區域、空的與不能用的候選、
// 錯誤與取消。S01 的修訂要求涵蓋這些（ADR-006）。兩個 adapter 的「合併成元件」規則各寫一份（不能共用程式碼，S08），
// 所以最後一個測試在同一塊板子上比兩邊：結果要一致。
// S07 的 tst_opencv_library_matcher 併進這裡；HALCON 才有的（一次 find_shape_models 與每個類型一次）留在 tst_halcon_library_matcher。
//
// 合成板子上有兩種元件，左半 A（主體 48×20、電極亮）、右半 B（主體 24×24、電極暗）；候選類型的參考影像從另一塊同樣的板子裁下來。
// 假 back end 沒有 ILibraryMatcher（ContractTolerances.h 的 hasLibraryMatcher），不在這裡。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

#include "BackendTestSupport.h"
#include "ContractSupport.h"
#include "QtTestPrinters.h"
#include "application/ports/ILibraryMatcher.h"
#include "domain/image_buffer.h"
#include "synthetic_board.h"
#ifdef AOI_WITH_OPENCV
#include "infrastructure/backend_opencv/OpenCvLibraryMatcher.h"
#endif
#ifdef AOI_WITH_HALCON
#include "infrastructure/backend_halcon/HalconLibraryMatcher.h"
#endif

using namespace aoi;
using namespace aoi::contract;
using namespace aoi::test;

namespace {

// ILibraryMatcher 的工廠：BackendRegistry 的 Backend 還沒有這個欄位（流程 B 的 UI 沒有接，S07 的決定），所以這裡直接建。
std::unique_ptr<ILibraryMatcher> makeMatcher([[maybe_unused]] const std::string& id) {  // 兩個都沒編進來時用不到 id
#ifdef AOI_WITH_OPENCV
    if (id == "opencv") return std::make_unique<infra::OpenCvLibraryMatcher>();
#endif
#ifdef AOI_WITH_HALCON
    if (id == "halcon") return std::make_unique<infra::HalconLibraryMatcher>();
#endif
    return {};
}

void addMatcherRows() {
    QTest::addColumn<QString>("backend");
    for (const std::string& id : backendIds(Rows::All)) {
        if (profileOf(id).hasLibraryMatcher) QTest::newRow(id.c_str()) << QString::fromStdString(id);
    }
}

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

const TwoKindBoard& twoKindBoard() {
    static const TwoKindBoard board = [] {
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
    }();
    return board;
}

ComponentType typeFromView(const std::string& id, const ImageView& reference) {
    ComponentType t;
    t.id = id;
    t.referenceImage = reference;
    return t;
}

// 一個類型：參考影像是那種元件（主體＋每邊 4 像素），校正值 umPerPixel。
ComponentType typeOf(const std::string& id, bool a, double umPerPixel = 10.0) {
    const SyntheticBoard board = makeSyntheticBoard(kindSpec(a));
    const Rect box = sampleBoxAround(board);
    ImageView reference = subView(board.image, static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w),
                                  static_cast<int>(box.h));
    reference.umPerPixel = umPerPixel;
    return typeFromView(id, reference);
}

// 每個像素複製成 2×2（看起來大一倍、校正值減半）。
ImageView doubled(const ImageView& source, double umPerPixel) {
    ImageBuffer out(source.width * 2, source.height * 2, PixelFormat::Gray8);
    for (int y = 0; y < out.height(); ++y) {
        const std::uint8_t* src = source.data + static_cast<std::size_t>(y / 2) * static_cast<std::size_t>(source.strideBytes);
        std::uint8_t* dst = out.row(y);
        for (int x = 0; x < out.width(); ++x) dst[x] = src[x / 2];
    }
    return out.view(umPerPixel);
}

// 轉 90°（轉置後左右翻）：同樣的元件，方向不對。
ImageView rotated90(const ImageView& source) {
    ImageBuffer out(source.height, source.width, PixelFormat::Gray8);
    for (int y = 0; y < out.height(); ++y) {
        for (int x = 0; x < out.width(); ++x) {
            out.row(y)[x] = source.data[static_cast<std::size_t>(source.height - 1 - x) * static_cast<std::size_t>(source.strideBytes) +
                                        static_cast<std::size_t>(y)];
        }
    }
    return out.view(source.umPerPixel);
}

// 不在板子上的類型（樣本庫大小 N 的測試用）：方向不對的 A、B，暗的方塊、亮的細條。
std::vector<ComponentType> distractors(std::size_t count) {
    std::vector<ComponentType> out;
    const ComponentType a = typeOf("A", true);
    const ComponentType b = typeOf("B", false);
    out.push_back(typeFromView("A90", rotated90(a.referenceImage)));
    out.push_back(typeFromView("B90", rotated90(b.referenceImage)));
    for (int k = 0; out.size() < count; ++k) {
        SyntheticBoardSpec s = kindSpec(true);
        const bool square = k % 2 == 0;
        s.bodyWidthPx = square ? 30.0 + 2.0 * k : 70.0 + 2.0 * k;
        s.bodyHeightPx = square ? 30.0 + 2.0 * k : 12.0;
        s.bodyLevel = square ? 25 : 250;
        s.padLevel = square ? 35 : 250;
        const SyntheticBoard board = makeSyntheticBoard(s);
        const Rect box = sampleBoxAround(board);
        ImageView reference = subView(board.image, static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w),
                                      static_cast<int>(box.h));
        reference.umPerPixel = 10.0;
        out.push_back(typeFromView("D" + std::to_string(k), reference));
    }
    out.resize(count);
    return out;
}

SearchParams params() {
    SearchParams p;
    p.minScore = 0.75;
    p.angleMinDeg = -5.0;
    p.angleMaxDeg = 5.0;
    return p;
}

// 每個標準答案：最近的元件（≤ 2 px 算找到）的第一個類型是 typeId。回傳找到幾個、最大的位置誤差。
struct Recognition {
    std::size_t right = 0;
    double worstPx = 0.0;
};
Recognition recognize(const std::vector<ComponentCandidate>& found, const std::vector<Pose2D>& truth, const std::string& typeId) {
    Recognition r;
    for (const Pose2D& t : truth) {
        const ComponentCandidate* nearest = nullptr;
        double best = 1e9;
        for (const ComponentCandidate& c : found) {
            const double d = distance(c.pose.position(), t.position());
            if (d < best) {
                best = d;
                nearest = &c;
            }
        }
        if (nearest == nullptr || best > 2.0 || nearest->typeScores.empty() || nearest->typeScores.front().typeId != typeId) continue;
        ++r.right;
        r.worstPx = std::max(r.worstPx, best);
    }
    return r;
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

// 每個測試的開頭：這一列的 back end 能用（授權）才建它的比對器。
#define CONTRACT_MATCHER(name)                                                                                            \
    CONTRACT_BACKEND(name##Engine);                                                                                       \
    const std::unique_ptr<ILibraryMatcher> name##Owner = makeMatcher(name##Engine.backend.id);                            \
    QVERIFY2(name##Owner, "a back end with hasLibraryMatcher has an ILibraryMatcher");                                   \
    ILibraryMatcher& name = *name##Owner;                                                                                 \
    const LibraryTolerances name##Tol = name##EngineProfile.library

class TstContractLibraryMatcher : public QObject {
    Q_OBJECT

private slots:
    // 辨識正確率、排序、分數範圍、位姿誤差、進度。
    void eachComponentGetsTheRightTypeFirst_data() { addMatcherRows(); }
    void eachComponentGetsTheRightTypeFirst() {
        CONTRACT_MATCHER(matcher);
        const TwoKindBoard& board = twoKindBoard();
        CountingProgress progress;
        const Result<LibraryMatchResult> r =
            matcher.match(board.image, {0.0, 0.0, 1280.0, 480.0}, {typeOf("A", true), typeOf("B", false)}, params(), CancelToken(), &progress);
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const LibraryMatchResult& m = r.value();
        QCOMPARE(m.samplesCompared, std::size_t{2});
        QVERIFY(!m.noMatch);
        const Recognition a = recognize(m.components, board.kindA, "A");
        const Recognition b = recognize(m.components, board.kindB, "B");
        qInfo("%s: A %zu/%zu (worst %.3f px), B %zu/%zu (worst %.3f px), %zu components", matcherEngine.backend.id.c_str(), a.right,
              board.kindA.size(), a.worstPx, b.right, board.kindB.size(), b.worstPx, m.components.size());
        QCOMPARE(a.right, board.kindA.size());
        QCOMPARE(b.right, board.kindB.size());
        QCOMPARE(m.components.size(), board.kindA.size() + board.kindB.size());
        QVERIFY2(a.worstPx <= matcherTol.positionPx && b.worstPx <= matcherTol.positionPx, qPrintable(QString::number(std::max(a.worstPx, b.worstPx))));
        for (const ComponentCandidate& c : m.components) {
            QVERIFY(!c.typeScores.empty());
            for (std::size_t i = 1; i < c.typeScores.size(); ++i) QVERIFY(c.typeScores[i - 1].score >= c.typeScores[i].score);
            for (const TypeScore& s : c.typeScores) QVERIFY(s.score >= params().minScore && s.score <= 1.0);
        }
        QCOMPARE(progress.count.load(), 2);
        QCOMPARE(progress.last, 1.0);
    }

    // 樣本庫大小 N：多出來的類型（不在板子上的）不改變辨識結果；比對了 N 個、進度回報 N 次。
    void theLibrarySizeDoesNotChangeTheAnswer_data() {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<int>("n");
        for (const std::string& id : backendIds(Rows::All)) {
            if (!profileOf(id).hasLibraryMatcher) continue;
            for (const int n : {2, 6, 12}) QTest::addRow("%s N=%d", id.c_str(), n) << QString::fromStdString(id) << n;
        }
    }
    void theLibrarySizeDoesNotChangeTheAnswer() {
        CONTRACT_MATCHER(matcher);
        QFETCH(int, n);
        const TwoKindBoard& board = twoKindBoard();
        std::vector<ComponentType> types{typeOf("A", true), typeOf("B", false)};
        for (ComponentType& d : distractors(static_cast<std::size_t>(n) - 2)) types.push_back(std::move(d));
        CountingProgress progress;
        const Result<LibraryMatchResult> r = matcher.match(board.image, {0.0, 0.0, 1280.0, 480.0}, types, params(), CancelToken(), &progress);
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QCOMPARE(r.value().samplesCompared, static_cast<std::size_t>(n));
        QCOMPARE(progress.count.load(), n);
        QCOMPARE(recognize(r.value().components, board.kindA, "A").right, board.kindA.size());
        QCOMPARE(recognize(r.value().components, board.kindB, "B").right, board.kindB.size());
        std::size_t distractorFirst = 0;
        for (const ComponentCandidate& c : r.value().components) {
            if (!c.typeScores.empty() && c.typeScores.front().typeId != "A" && c.typeScores.front().typeId != "B") ++distractorFirst;
        }
        qInfo("%s N=%d: %zu components, %zu with a type that is not on the board first", matcherEngine.backend.id.c_str(), n,
              r.value().components.size(), distractorFirst);
        QCOMPARE(distractorFirst, std::size_t{0});
    }

    void onlyTheRegionIsScanned_data() { addMatcherRows(); }
    void onlyTheRegionIsScanned() {
        CONTRACT_MATCHER(matcher);
        const TwoKindBoard& board = twoKindBoard();
        const Rect leftHalf{0.0, 0.0, 640.0, 480.0};
        const Result<LibraryMatchResult> r =
            matcher.match(board.image, leftHalf, {typeOf("A", true), typeOf("B", false)}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        for (const ComponentCandidate& c : r.value().components) QVERIFY(leftHalf.contains(c.pose.position()));
        QCOMPARE(recognize(r.value().components, board.kindA, "A").right, board.kindA.size());
    }

    // 參考影像的校正值不同（5 µm／像素，看起來大一倍）：先縮放成板子的比例再比。
    void aReferenceAtAnotherScaleIsResized_data() { addMatcherRows(); }
    void aReferenceAtAnotherScaleIsResized() {
        CONTRACT_MATCHER(matcher);
        const TwoKindBoard& board = twoKindBoard();
        const ComponentType big = typeFromView("A2x", doubled(typeOf("A", true).referenceImage, 5.0));
        QVERIFY(big.referenceImage.valid());
        const Result<LibraryMatchResult> r = matcher.match(board.image, {0.0, 0.0, 640.0, 480.0}, {big}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        QCOMPARE(recognize(r.value().components, board.kindA, "A2x").right, board.kindA.size());
    }

    // 沒有候選、或候選都不能用（沒有參考影像、比區域大）：樣本庫是空的（不是錯誤）。
    void emptyOrUnusableCandidatesMeanAnEmptyLibrary_data() { addMatcherRows(); }
    void emptyOrUnusableCandidatesMeanAnEmptyLibrary() {
        CONTRACT_MATCHER(matcher);
        const TwoKindBoard& board = twoKindBoard();
        const Result<LibraryMatchResult> none = matcher.match(board.image, {0, 0, 640, 480}, {}, params(), CancelToken(), nullptr);
        QVERIFY(none.ok());
        QVERIFY(none.value().noMatch && none.value().noMatch->reason == NoMatchReason::LibraryEmpty);
        ComponentType noImage;
        noImage.id = "x";
        const Result<LibraryMatchResult> unusable =
            matcher.match(board.image, {0, 0, 20, 20}, {noImage, typeOf("A", true)}, params(), CancelToken(), nullptr);
        QVERIFY(unusable.ok());
        QCOMPARE(unusable.value().samplesCompared, std::size_t{0});
        QVERIFY(unusable.value().noMatch->reason == NoMatchReason::LibraryEmpty);
    }

    // 區域裡沒有那種元件：比對過了、沒有結果（NoLibraryMatch），不是錯誤。
    void aTypeThatIsNotThereGivesNoLibraryMatch_data() { addMatcherRows(); }
    void aTypeThatIsNotThereGivesNoLibraryMatch() {
        CONTRACT_MATCHER(matcher);
        const Result<LibraryMatchResult> r =
            matcher.match(twoKindBoard().image, {0.0, 0.0, 640.0, 480.0}, {typeOf("B", false)}, params(), CancelToken(), nullptr);
        QVERIFY(r.ok());
        QCOMPARE(r.value().samplesCompared, std::size_t{1});
        QVERIFY(r.value().components.empty());
        QVERIFY(r.value().noMatch && r.value().noMatch->reason == NoMatchReason::NoLibraryMatch);
    }

    void badInputAndCancel_data() { addMatcherRows(); }
    void badInputAndCancel() {
        CONTRACT_MATCHER(matcher);
        const TwoKindBoard& board = twoKindBoard();
        QCOMPARE_ERROR(matcher.match(ImageView{}, {0, 0, 10, 10}, {}, params(), CancelToken(), nullptr), ErrorCode::InvalidArgument);
        QCOMPARE_ERROR(matcher.match(board.image, {5000, 5000, 10, 10}, {}, params(), CancelToken(), nullptr), ErrorCode::InvalidArgument);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_ERROR(matcher.match(board.image, {0, 0, 640, 480}, {typeOf("A", true)}, params(), cancel.token(), nullptr), ErrorCode::Cancelled);
    }

    // 兩個 adapter 各自實作「合併成元件」：同一塊板子、同一組類型，結果要一致（元件數、每個元件的第一個類型、位置）。
    void bothBackEndsAgree() {
        std::vector<std::string> usable;
        for (const std::string& id : backendIds(Rows::All)) {
            if (profileOf(id).hasLibraryMatcher && engineFor(id).engine) usable.push_back(id);
        }
        if (usable.size() < 2) QSKIP("needs two usable back ends with a library matcher (OpenCV and HALCON)");
        const TwoKindBoard& board = twoKindBoard();
        const std::vector<ComponentType> types{typeOf("A", true), typeOf("B", false)};
        const Rect region{0.0, 0.0, 1280.0, 480.0};
        const Result<LibraryMatchResult> first = makeMatcher(usable[0])->match(board.image, region, types, params(), CancelToken(), nullptr);
        const Result<LibraryMatchResult> second = makeMatcher(usable[1])->match(board.image, region, types, params(), CancelToken(), nullptr);
        QVERIFY(first.ok() && second.ok());
        QCOMPARE(first.value().components.size(), second.value().components.size());
        double worst = 0.0;
        for (const ComponentCandidate& a : first.value().components) {
            const ComponentCandidate* nearest = nullptr;
            double best = 1e9;
            for (const ComponentCandidate& b : second.value().components) {
                const double d = distance(a.pose.position(), b.pose.position());
                if (d < best) {
                    best = d;
                    nearest = &b;
                }
            }
            QVERIFY(nearest != nullptr && best <= 2.0);
            QCOMPARE(nearest->typeScores.front().typeId, a.typeScores.front().typeId);
            worst = std::max(worst, best);
        }
        qInfo("%s vs %s: %zu components, max position difference %.3f px", usable[0].c_str(), usable[1].c_str(),
              first.value().components.size(), worst);
        QVERIFY2(worst <= profileOf(usable[0]).library.positionPx, qPrintable(QString::number(worst)));
    }
};

QTEST_GUILESS_MAIN(TstContractLibraryMatcher)
#include "tst_contract_library_matcher.moc"
