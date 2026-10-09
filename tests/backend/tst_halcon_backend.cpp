// HALCON back end（S08）才有的事：授權的判斷與快取、座標與角度的換算公式（ADR-010 在 S08 以前「未驗證」的部分）與它的必要性、
// 運算子能不能中斷、shape model 的範本、影像的複製次數與 nativeGray16（ADR-014）、轉 8 bit 的有效位元數與彩色係數、工廠與執行緒數。
// port 的契約與合成板子上的準確度在 S09 的契約測試（tests/contract/），對每個 back end 跑同一組；S08 這裡原本的那一份搬過去了。
// 這台機器沒有 HALCON 的授權或 DLL 時，整組測試跳過並說原因。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "application/CancelToken.h"
#include "domain/image_buffer.h"
#include "domain/image_copy.h"
#include "domain/image_depth.h"
#include "infrastructure/backend_halcon/HalconBackend.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#include "infrastructure/backend_halcon/HalconPatternSearcher.h"
#include "infrastructure/backend_halcon/HalconPose.h"
#include "infrastructure/backend_halcon/HalconRuntime.h"
#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"
#include "synthetic_board.h"

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

TemplatePtr buildFrom(const SyntheticBoard& board, const SearchParams& p, std::size_t index = 0, HalconSearchOptions options = {}) {
    HalconTemplateBuilder builder(std::move(options));
    Result<TemplatePtr> t = builder.build(board.image, sampleBoxAround(board, index), p, CancelToken());
    if (!t.ok()) qWarning("build failed: %s", t.error().message.c_str());
    return t.ok() ? t.value() : TemplatePtr();
}

std::vector<Match> findAll(const ImageView& image, const ITemplate& t, const SearchParams& p,
                           const SearchRegion& region = SearchRegion::wholeImage(), HalconSearchOptions options = {}) {
    HalconPatternSearcher searcher(std::move(options));
    Result<std::vector<Match>> r = searcher.find(image, t, region, p, CancelToken());
    if (!r.ok()) qWarning("find failed: %s", r.error().message.c_str());
    return r.ok() ? r.value() : std::vector<Match>{};
}

std::vector<Match> findAll(const SyntheticBoard& board, const ITemplate& t, const SearchParams& p,
                           const SearchRegion& region = SearchRegion::wholeImage()) {
    return findAll(board.image, t, p, region);
}

// 一個授權檔的文字（格式照這台機器的 license--DEVELOP-.dat；簽章是假的）。
constexpr const char* kDevelopmentLicense =
    "##################################################################################\n"
    "# Example Inc.  (Id: 'CM:3-1111111')                                              #\n"
    "##################################################################################\n"
    "LICENSE MVTec_HDevelop 24.11 MODULES=fc123bdom ID=CM:3-1111111 SESSIONS=1 \\\r\n"
    "        INFO=\"HALCON 24.11 Steady SDK, Free U\" SIGNATURE=\"0000 1111 \\\r\n"
    "        2222 3333\"\r\n"
    "LICENSE MVTec_HALCON 24.11 MODULES=fc123bdom ID=CM:3-1111111 SESSIONS=1 \\\r\n"
    "        INFO=\"HALCON 24.11 Steady SDK, Free U\" SIGNATURE=\"4444 5555\"\r\n";
constexpr const char* kRuntimeLicense =
    "LICENSE MVTec_HALCON 24.11 MODULES=fc123bdom ID=CM:3-2222222 SESSIONS=1 \\\n"
    "        INFO=\"HALCON 24.11 Steady RTL (Bundle)\" SIGNATURE=\"6666\"\n";

HalconLicenseInfo licensedRuntime() {
    HalconLicenseInfo info;
    info.valid = true;
    info.kind = "runtime";
    info.hostId = "3-2222222";
    info.modules = "Foundation | Matching";
    info.shapeMatchingLicensed = true;
    info.hostIds = {"546ceb63fb23", "3-2222222"};
    info.files = {parseHalconLicenseFile("C:/x/license.dat", kRuntimeLicense),
                  parseHalconLicenseFile("C:/x/license--DEVELOP-.dat", kDevelopmentLicense)};
    return info;
}

}  // namespace

class TstHalconBackend : public QObject {
    Q_OBJECT

private slots:
    // ---- 授權的判斷（純函式，不呼叫 HALCON：沒有 HALCON 的電腦也照跑）----
    void licenseFilesAreParsed() {
        const HalconLicenseFile dev = parseHalconLicenseFile("dev.dat", kDevelopmentLicense);
        QCOMPARE(dev.features.size(), std::size_t{2});
        QCOMPARE(dev.features[0], std::string("MVTec_HDevelop"));
        QVERIFY(dev.development());
        QCOMPARE(dev.ids.front(), std::string("CM:3-1111111"));
        QCOMPARE(dev.info, std::string("HALCON 24.11 Steady SDK, Free U"));
        const HalconLicenseFile rt = parseHalconLicenseFile("rt.dat", kRuntimeLicense);
        QVERIFY(!rt.development());
        QCOMPARE(rt.ids, std::vector<std::string>{"CM:3-2222222"});
        QVERIFY(sameHalconHostId("CM:3-2222222", "3-2222222"));
        QVERIFY(sameHalconHostId("cm:3-2222222", "CM:3-2222222"));
        QVERIFY(!sameHalconHostId("3-2222222", "3-1111111"));
        QVERIFY(!sameHalconHostId("", ""));
    }

    // 使用者 2026-10-07：dongle 分 runtime（RTL）與 development（SDK）兩種，授權錯了要提醒。
    void theRuntimeLicenseWithoutTheSdkDongleIsANotice() {
        const HalconLicenseVerdict v = judgeHalconLicense(licensedRuntime());
        QVERIFY(v.usable);
        QCOMPARE(v.notices.size(), std::size_t{1});
        QVERIFY(v.notices[0].kind == HalconLicenseNoticeKind::SdkDongleMissing);
        QCOMPARE(v.notices[0].missingId, std::string("CM:3-1111111"));
        QCOMPARE(v.notices[0].inUseId, std::string("3-2222222"));
        QVERIFY(v.notices[0].message.find("license--DEVELOP-.dat") != std::string::npos);

        HalconLicenseInfo attached = licensedRuntime();
        attached.hostIds.push_back("3-1111111");  // SDK 的 dongle 也插著（HALCON 還是挑了 runtime）：不提醒
        QVERIFY(judgeHalconLicense(attached).notices.empty());
        HalconLicenseInfo development = licensedRuntime();
        development.kind = "development";
        QVERIFY(judgeHalconLicense(development).notices.empty());
        HalconLicenseInfo deployed = licensedRuntime();
        deployed.files.pop_back();  // 只有 runtime 授權（部署的電腦）：不提醒
        QVERIFY(judgeHalconLicense(deployed).notices.empty());
    }

    void noLicenseOrNoMatchingModuleIsUnavailableWithTheReason() {
        HalconLicenseInfo none = licensedRuntime();
        none.valid = false;
        none.errorCode = 2036;
        none.errorText = "could not find license file";
        const HalconLicenseVerdict v = judgeHalconLicense(none);
        QVERIFY(!v.usable);
        QVERIFY2(v.reason.find("2036") != std::string::npos &&
                     v.reason.find("license--DEVELOP-.dat needs CM:3-1111111 (development)") != std::string::npos &&
                     v.reason.find("license.dat needs CM:3-2222222 (runtime)") != std::string::npos,
                 v.reason.c_str());
        // 讀不到 dongle 時（S08 實測：錯誤 2300），HALCON 列不出這台電腦的 ID：原因要說怎麼查，不是只寫「沒有」。
        HalconLicenseInfo unplugged = none;
        unplugged.hostIds.clear();
        QVERIFY(judgeHalconLicense(unplugged).reason.find("hhostid.exe") != std::string::npos);
        HalconLicenseInfo noMatching = licensedRuntime();
        noMatching.shapeMatchingLicensed = false;
        QVERIFY(!judgeHalconLicense(noMatching).usable);
        QVERIFY(judgeHalconLicense(noMatching).reason.find("shape-based matching") != std::string::npos);
        HalconLicenseInfo noDll;
        noDll.runtimeProblem = "halconcpp.dll is missing";
        QCOMPARE(judgeHalconLicense(noDll).reason, std::string("halconcpp.dll is missing"));
    }

    // ---- 授權的快取（2026-10-09）：能用才記住；不能用的下一次重新檢查（插上 dongle 之後不必重新啟動程式）。假的探測，不呼叫 HALCON ----
    void aFailedLicenseIsCheckedAgainTheNextTime() {
        HalconLicenseInfo unplugged = licensedRuntime();
        unplugged.valid = false;
        unplugged.errorCode = 2300;
        unplugged.errorText = "Dongle not attached, or can't read dongle";
        std::vector<bool> rechecks;
        bool attached = false;
        HalconLicenseCache cache([&](bool recheck) {
            rechecks.push_back(recheck);
            return attached ? licensedRuntime() : unplugged;
        });
        QVERIFY(!judgeHalconLicense(cache.get()).usable);  // 第一次：沒插
        QVERIFY(!judgeHalconLicense(cache.get()).usable);  // 還是沒插：再檢查一次，不是回舊的結果
        attached = true;                                    // 使用者插上 dongle
        QVERIFY(judgeHalconLicense(cache.get()).usable);    // 下一次問就拿到
        QCOMPARE(cache.probes(), 3);
        // 第一次是一般的探測；之後的都要 HALCON 立刻重新檢查（is_license_valid）
        QCOMPARE(rechecks, (std::vector<bool>{false, true, true}));
    }

    void aUsableLicenseIsRememberedAndNotProbedAgain() {
        int calls = 0;
        HalconLicenseCache cache([&](bool) {
            ++calls;
            return licensedRuntime();
        });
        for (int i = 0; i < 5; ++i) QVERIFY(judgeHalconLicense(cache.get()).usable);
        QCOMPARE(calls, 1);  // 第一次約 1.5 秒（HALCON 初始化）；之後回記住的那一份
        QCOMPARE(cache.get().hostId, std::string("3-2222222"));
    }

    void callersAtTheSameTimeShareOneProbe() {
        std::atomic<int> calls{0};
        HalconLicenseCache cache([&](bool) {
            ++calls;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // 探測很慢（真的約 1.5 秒）
            return licensedRuntime();
        });
        std::vector<std::thread> callers;
        std::atomic<int> usable{0};
        for (int i = 0; i < 4; ++i)
            callers.emplace_back([&] {
                if (judgeHalconLicense(cache.get()).usable) ++usable;
            });
        for (std::thread& t : callers) t.join();
        QCOMPARE(usable.load(), 4);
        QCOMPARE(calls.load(), 1);  // HALCON 的初始化不要並行做兩次
    }

    // ---- 座標換算（HalconPose.h）：純函式 ----
    void poseConversionMatchesTheDocumentedFormula() {
        // 角度 0：單純的 ＋0.5（HALCON 的像素中心在整數，ADR-010 在 ＋0.5）。
        const Pose2D p0 = poseFromHalcon(114.5, 129.5, 0.0);
        QCOMPARE(p0.x, 130.0);
        QCOMPARE(p0.y, 115.0);
        QCOMPARE(p0.angleDeg, 0.0);
        // 角度 90°：R·(0.5, 0.5) ＝ (−0.5, 0.5)（row, column），所以參考點在 (Row − 1, Column)（HALCON）→ (Column ＋ 0.5, Row − 0.5)。
        const Pose2D p90 = poseFromHalcon(10.0, 20.0, degreesToRadians(90.0));
        QVERIFY2(std::abs(p90.x - 20.5) < 1e-12 && std::abs(p90.y - 9.5) < 1e-12, qPrintable(QStringLiteral("%1 %2").arg(p90.x).arg(p90.y)));
        QVERIFY(std::abs(p90.angleDeg - 90.0) < 1e-12);
        // 子影像的原點
        const Pose2D shifted = poseFromHalcon(0.0, 0.0, 0.0, 100.0, 50.0);
        QCOMPARE(shifted.x, 100.5);
        QCOMPARE(shifted.y, 50.5);
        const HalconRect1 r = halconRect1(10, 20, 30, 40);
        QCOMPARE(r.row1, 20.0);
        QCOMPARE(r.column1, 10.0);
        QCOMPARE(r.row2, 59.0);
        QCOMPARE(r.column2, 39.0);
    }

    // ---- 以下要 HALCON 的程式庫與授權（每個測試開頭 REQUIRE_HALCON；上面的純函式測試在沒有 HALCON 的電腦也照跑）----
    void theRuntimeIsLoadableAndLicensed() {
        REQUIRE_HALCON();
        const HalconLicenseInfo& info = halconLicense();
        qInfo("HALCON %s: %s license, host %s, edition %s, modules %s, check %.0f ms", halconSdkVersion().c_str(), info.kind.c_str(),
              info.hostId.c_str(), info.edition.c_str(), info.modules.c_str(), info.checkMs);
        for (const HalconLicenseNotice& n : judgeHalconLicense(info).notices) qInfo("notice: %s", n.message.c_str());
        QVERIFY(info.valid);
        QVERIFY(info.shapeMatchingLicensed);
        QVERIFY(!info.hostIds.empty());
    }

    // 取消靠 interrupt_operator：find_shape_model(s) 一定要支援 'cancel'（S08 用 get_operator_info 實際查到）。
    // HALCON 改版拿掉這個能力時，這個測試先失敗（取消會退化成「等搜尋做完」）。
    void findShapeModelIsInterruptible() {
        REQUIRE_HALCON();
        for (const char* op : {"find_shape_model", "find_shape_models"}) {
            HalconCpp::HTuple mode;
            HalconCpp::GetOperatorInfo(op, "interrupt_mode", &mode);
            QVERIFY2(mode.Length() > 0 && std::string(mode[0].S().Text()).find("cancel") != std::string::npos, op);
            HalconCpp::HTuple parallel;
            HalconCpp::GetOperatorInfo(op, "parallelization", &parallel);
            QCOMPARE(std::string(parallel[0].S().Text()), std::string("no_restriction"));
        }
    }

    // ---- 範本建立器 ----
    void theTemplateHasTheSampleSizeAndAModel() {
        REQUIRE_HALCON();
        const SyntheticBoard board = makeSyntheticBoard(cleanSpec());
        HalconTemplateBuilder builder;
        QVERIFY(builder.backendId() == "halcon");
        const Result<TemplatePtr> t = builder.build(board.image, {100.4, 98.6, 63.7, 32.2}, params(), CancelToken());
        QVERIFY2(t.ok(), t.ok() ? "" : t.error().message.c_str());
        QCOMPARE(t.value()->size().w, 64.0);  // 四捨五入到像素邊界：100..164、99..131（和 OpenCV 版相同）
        QCOMPARE(t.value()->size().h, 32.0);
        QVERIFY(t.value()->backendId() == "halcon");
        QVERIFY(!t.value()->sampleProblem());
        const auto& ht = dynamic_cast<const HalconTemplate&>(*t.value());
        QVERIFY(ht.hasModel());
        QVERIFY(ht.numLevels() >= 1);
        QCOMPARE(ht.angleMinDeg(), -5.0);
        QCOMPARE(ht.angleMaxDeg(), 5.0);
        qInfo("model levels: %d", ht.numLevels());
    }

    // 寬的角度範圍（±30°）：這裡量得出位姿換算的旋轉修正（30° 時差 0.37 像素）。
    // 沒有修正的（naivePoseFromHalcon）是「依角度變化的系統性偏移」：平均誤差在大角度明顯變大；修正過的平均誤差要小，
    // 而且和角度無關。證明 HalconPose.h 的公式是對的、而且必要。最大誤差的上限和角度正負號的測試同一把尺（0.35 像素）。
    void theRotatedPoseCorrectionIsNeeded() {
        REQUIRE_HALCON();
        const SyntheticBoard straight = makeSyntheticBoard(cleanSpec());
        const TemplatePtr t = buildFrom(straight, params(0.7, 30.0));
        const auto& ht = dynamic_cast<const HalconTemplate&>(*t);
        double worstCorrected = 0.0;
        double worstNaive = 0.0;
        double worstAngle = 0.0;
        double sumCorrected = 0.0;
        double sumNaive = 0.0;
        int boards = 0;
        for (int k = -12; k <= 12; ++k) {
            SyntheticBoardSpec s = cleanSpec();
            s.rows = 3;
            s.cols = 4;
            s.tiltDeg = 2.5 * k;
            const SyntheticBoard tilted = makeSyntheticBoard(s);
            // 直接呼叫 HALCON（不經過搜尋器）拿原始的 Row、Column、Angle，兩種換算各算一次。
            HalconCpp::HImage image;
            image.GenImage1Extern("byte", tilted.image.width, tilted.image.height, const_cast<std::uint8_t*>(tilted.image.data), nullptr);
            HalconCpp::HTuple rows, cols, angles, scores;
            ht.model().FindShapeModel(image, degreesToRadians(-30.0), degreesToRadians(60.0), 0.7, 0, 0.5, "least_squares", 0, 0.9,
                                      &rows, &cols, &angles, &scores);
            std::vector<Pose2D> corrected;
            std::vector<Pose2D> naive;
            for (Hlong i = 0; i < rows.Length(); ++i) {
                corrected.push_back(poseFromHalcon(rows[i].D(), cols[i].D(), angles[i].D()));
                naive.push_back(naivePoseFromHalcon(rows[i].D(), cols[i].D(), angles[i].D()));
            }
            const Accuracy a = compareWithTruth(tilted, corrected, s.tiltDeg);
            const Accuracy b = compareWithTruth(tilted, naive, s.tiltDeg);
            QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(QString::number(s.tiltDeg) + ": " + a.describe()));
            worstCorrected = std::max(worstCorrected, a.maxErrorPx);
            worstNaive = std::max(worstNaive, b.maxErrorPx);
            worstAngle = std::max(worstAngle, a.maxAngleErrorDeg);
            if (std::abs(s.tiltDeg) >= 20.0) {  // 修正量 ≥ 0.2 像素的角度
                sumCorrected += a.meanErrorPx;
                sumNaive += b.meanErrorPx;
                ++boards;
            }
        }
        const double meanCorrected = sumCorrected / boards;
        const double meanNaive = sumNaive / boards;
        qInfo("-30..30 deg: corrected max %.3f px, naive max %.3f px; at |tilt| >= 20: corrected mean %.3f px, naive mean %.3f px; angle max %.3f deg",
              worstCorrected, worstNaive, meanCorrected, meanNaive, worstAngle);
        QVERIFY2(worstCorrected < 0.35, qPrintable(QString::number(worstCorrected)));
        QVERIFY2(meanCorrected < 0.1 && meanNaive > 2.0 * meanCorrected + 0.1,
                 qPrintable(QStringLiteral("%1 vs %2").arg(meanCorrected).arg(meanNaive)));
        QVERIFY2(worstAngle < 0.3, qPrintable(QString::number(worstAngle)));
    }

    // ---- 影像複製（ADR-014 規則 6）----
    void gray8IsSearchedWithoutCopiesAndGray16WithOne() {
        REQUIRE_HALCON();
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

        // nativeGray16：uint2 零複製，結果和 8 bit 的路徑一樣找得到（分數看梯度方向，和亮度的倍數無關）。
        HalconSearchOptions native;
        native.nativeGray16 = true;
        const TemplatePtr tn = buildFrom(deep, params(), 0, native);
        before = imageCopyCount();
        const std::vector<Match> found = findAll(deep.image, *tn, params(), SearchRegion::wholeImage(), native);
        QCOMPARE((imageCopyCount() - before).copies, std::uint64_t{0});
        const Accuracy a = compareWithTruth(deep, posesOf(found), 0.0);
        qInfo("native uint2: %s", qPrintable(a.describe()));
        QVERIFY2(a.matched == a.present && a.falsePositives == 0 && a.maxErrorPx < 0.25, qPrintable(a.describe()));
    }

    // ---- 有效位元數（S07：配方或使用者的覆寫）：轉 8 bit 時用 view 的位元數 ----
    void theGrayConversionUsesTheSignificantBits() {
        REQUIRE_HALCON();
        SyntheticBoardSpec s = cleanSpec(PixelFormat::Gray16);
        s.grayBits = 12;
        s.backgroundLevel = 15;
        s.bodyLevel = 35;
        s.padLevel = 60;
        const SyntheticBoard board = makeSyntheticBoard(s);
        ImageView guessed = board.image;
        guessed.significantBits = detectSignificantBits(guessed);
        QCOMPARE(guessed.significantBits, 10);
        ImageView told = board.image;
        told.significantBits = 12;
        const HalconGray fromGuess = toHalconGray(guessed, PixelRect{0, 0, 4, 4}, false);
        const HalconGray fromTruth = toHalconGray(told, PixelRect{0, 0, 4, 4}, false);
        // 底色 15（8 bit）存成 12 bit 是 241；推測 10 bit → 241 × 255 ／ 1023 ≈ 60（亮了 4 倍），12 bit → 15。和 OpenCV 版相同。
        QCOMPARE(static_cast<int>(fromTruth.image.GetGrayval(0, 0)[0].L()), 15);
        QCOMPARE(static_cast<int>(fromGuess.image.GetGrayval(0, 0)[0].L()), 60);
        for (const ImageView& view : {guessed, told}) {
            HalconTemplateBuilder builder;
            const Result<TemplatePtr> t = builder.build(view, sampleBoxAround(board), params(), CancelToken());
            QVERIFY(t.ok() && !t.value()->sampleProblem());
            const Accuracy a = compareWithTruth(board, posesOf(findAll(view, *t.value(), params())), 0.0);
            QVERIFY2(a.matched == a.present && a.falsePositives == 0, qPrintable(a.describe()));
        }
    }

    // 彩色轉灰階和 OpenCV 的 cvtColor 同一組定點係數（兩個 back end 拿到同樣的灰階值）。
    void colorToGrayUsesTheOpenCvCoefficients() {
        REQUIRE_HALCON();
        ImageBuffer buffer(3, 1, PixelFormat::Bgrx32);
        const std::uint8_t bgrx[] = {255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 255, 0};  // 藍、綠、紅
        std::copy(std::begin(bgrx), std::end(bgrx), buffer.row(0));
        const HalconGray g = toHalconGray(buffer.view(), PixelRect{0, 0, 3, 1}, false);
        QCOMPARE(static_cast<int>(g.image.GetGrayval(0, 0)[0].L()), 29);   // (1868·255 ＋ 8192) >> 14
        QCOMPARE(static_cast<int>(g.image.GetGrayval(0, 1)[0].L()), 150);  // 9617·255
        QCOMPARE(static_cast<int>(g.image.GetGrayval(0, 2)[0].L()), 76);   // 4899·255
    }

    // ---- 工廠 ----
    void theFactoryBuildsAMatchingPairAndSetsTheThreads() {
        REQUIRE_HALCON();
        Result<Backend> b = makeHalconBackend();
        QVERIFY2(b.ok(), b.ok() ? "" : b.error().message.c_str());
        QCOMPARE(b.value().id, std::string("halcon"));
        QVERIFY(b.value().templateBuilder && b.value().searcher);
        QVERIFY(b.value().templateBuilder->backendId() == b.value().searcher->backendId());
        QCOMPARE(setHalconThreads(2), 2);
        QCOMPARE(setHalconThreads(0), 2);  // 0 ＝ 不改
        QVERIFY(setHalconThreads(static_cast<int>(std::thread::hardware_concurrency())) >= 1);
    }
};

QTEST_GUILESS_MAIN(TstHalconBackend)
#include "tst_halcon_backend.moc"
