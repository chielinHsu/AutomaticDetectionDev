// 真實影像（S08）：使用者提供的去識別化板子影像（不入版控）。環境變數 AOI_REAL_IMAGE_DIR 指到影像的資料夾才執行，
// 沒有就跳過（QSKIP）。效能量測：只在 scripts/dev/test.ps1 -Perf 時跑（AiInspTool/tests/tests.pro 的 AOI_PERF_TESTS）。
//
// 和 perf_opencv_real 同一張影像、同一個樣本框，HALCON 與 OpenCV 的 use case 各跑一次並排：找到幾個（依來源）、
// 種子分數的分布、晶格、各階段的時間；另外畫一張縮小的疊圖（<建置目錄>/real-<檔名>-halcon-overlay.png）給人看。
// 讀檔用 OpenCV 的影像來源（這台機器的 Qt 沒有 TIFF；HALCON 的影像來源沒有做，見 docs/steps/S08-halcon/spec.md），
// 所以只在 OpenCV 也編進來時建置（AiInspTool/tests/tests.pro）。
//
// 樣本框預設是 0_0_HA.tif 的一根針腳（影像像素 x 442、y 444、寬 80、高 30）；AOI_REAL_SAMPLE="x,y,w,h" 可以換。
#include <QDir>
#include <QFileInfo>
#include <QtTest>
#include <algorithm>
#include <chrono>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "QtTestPrinters.h"
#include "app/CompositionRoot.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/image_depth.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"
#include "BackendTestSupport.h"

using namespace aoi;

namespace {

Rect sampleRect() {
    const QString text = qEnvironmentVariable("AOI_REAL_SAMPLE");
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() == 4) return {parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble(), parts[3].toDouble()};
    return {442.0, 444.0, 80.0, 30.0};
}

void writeOverlay(const ImageView& image, const SampleProposal& proposal, const QString& path) {
    const infra::GrayRoi gray = infra::toGray8(image, cv::Rect(0, 0, image.width, image.height));
    cv::Mat color;
    cv::cvtColor(gray.mat, color, cv::COLOR_GRAY2BGR);
    for (const Candidate& c : proposal.candidates) {
        // 洋紅：樣本；紅：推算的缺格（Predicted）；藍：視窗驗證找到的；種子依分數：綠 ≥ 0.85、黃 0.75..0.85、橘 < 0.75（同 perf_opencv_real）。
        const cv::Scalar ink = c.source == CandidateSource::Sample      ? cv::Scalar(255, 0, 255)
                               : c.source == CandidateSource::Predicted ? cv::Scalar(0, 0, 255)
                               : c.source == CandidateSource::Verified  ? cv::Scalar(255, 160, 0)
                               : c.score >= 0.85                        ? cv::Scalar(0, 220, 0)
                               : c.score >= 0.75                        ? cv::Scalar(0, 230, 255)
                                                                        : cv::Scalar(0, 128, 255);
        const cv::RotatedRect r(cv::Point2f(static_cast<float>(c.pose.x - 0.5), static_cast<float>(c.pose.y - 0.5)),
                                cv::Size2f(static_cast<float>(proposal.partSize.w), static_cast<float>(proposal.partSize.h)),
                                static_cast<float>(-c.pose.angleDeg));
        cv::Point2f corners[4];
        r.points(corners);
        for (int i = 0; i < 4; ++i) cv::line(color, corners[i], corners[(i + 1) % 4], ink, 3);
    }
    cv::Mat small;
    cv::resize(color, small, cv::Size(), 0.25, 0.25, cv::INTER_AREA);
    cv::imwrite(path.toStdString(), small);
}

}  // namespace

class PerfHalconReal : public QObject {
    Q_OBJECT

private:
    QDir dir_;
    app::CoreServices halcon_;
    app::CoreServices opencv_;

    void report(const char* backend, const QString& file, app::CoreServices& core, const ImageView& view) {
        SampleRequest request;
        request.image = view;
        request.sample = sampleRect();
        SampleProposal last;
        std::vector<double> totals;
        for (int run = 0; run < 3; ++run) {
            const Result<SampleProposal> r = core.engine->generate->run(request, CancelToken());
            QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
            last = r.value();
            totals.push_back(last.timings.totalMs);
        }
        std::sort(totals.begin(), totals.end());
        std::size_t bySource[4] = {0, 0, 0, 0};
        for (const Candidate& c : last.candidates) ++bySource[static_cast<int>(c.source)];
        int histogram[7] = {0, 0, 0, 0, 0, 0, 0};  // 0.70, 0.75, …, 0.95..1.00
        for (const Candidate& c : last.candidates) {
            if (c.source == CandidateSource::Seed) ++histogram[std::clamp(static_cast<int>((c.score - 0.70) / 0.05), 0, 6)];
        }
        qInfo("  [%s] sample problem: %s | candidates %zu: sample %zu, seed %zu, verified %zu, predicted %zu", backend,
              last.sampleProblem ? "yes" : "no", last.candidates.size(), bySource[0], bySource[1], bySource[2], bySource[3]);
        qInfo("  [%s] seed scores: 0.70-0.75 %d | 0.75-0.80 %d | 0.80-0.85 %d | 0.85-0.90 %d | 0.90-0.95 %d | 0.95-1.00 %d", backend,
              histogram[0], histogram[1], histogram[2], histogram[3], histogram[4], histogram[5] + histogram[6]);
        qInfo("  [%s] lattice: found %d, lattices %zu, u (%.2f, %.2f), v (%.2f, %.2f), rms %.3f px, coverage %.3f, confidence %.3f, holes %zu",
              backend, last.grid.found, last.grid.lattices, last.grid.u.x, last.grid.u.y, last.grid.v.x, last.grid.v.y, last.grid.rmsPx,
              last.grid.coverage, last.grid.confidence, last.grid.holes);
        qInfo("  [%s] time (last run): template %.1f, seed %.1f, fit %.1f, verify %.1f, total %.1f ms | total over 3 runs: min %.1f, max %.1f ms",
              backend, last.timings.templateMs, last.timings.seedMs, last.timings.fitMs, last.timings.verifyMs, last.timings.totalMs,
              totals.front(), totals.back());
        writeOverlay(view, last, QDir::current().filePath(QStringLiteral("real-%1-%2-overlay.png").arg(QFileInfo(file).baseName(), QString::fromLatin1(backend))));
    }

private slots:
    void initTestCase() {
        const QString d = qEnvironmentVariable("AOI_REAL_IMAGE_DIR");
        if (d.isEmpty()) QSKIP("AOI_REAL_IMAGE_DIR is not set: the user's real images are not in the repository.");
        REQUIRE_HALCON();
        dir_ = QDir(d);
        QVERIFY2(dir_.exists(), qPrintable(d));
        Result<app::CoreServices> h = app::composeCore(app::makeBackendRegistry(), "halcon", false, app::ExecutorKind::StdThreads);
        QVERIFY2(h.ok(), h.ok() ? "" : h.error().message.c_str());
        halcon_ = std::move(h.value());
        Result<app::CoreServices> c = app::composeCore(app::makeBackendRegistry(), "opencv", false, app::ExecutorKind::StdThreads);
        QVERIFY(c.ok());
        opencv_ = std::move(c.value());
    }

    void realBoard_data() {
        QTest::addColumn<QString>("file");
        QTest::newRow("HA (high angle light)") << QStringLiteral("0_0_HA.tif");
        QTest::newRow("LA (low angle light)") << QStringLiteral("0_0_LA.tif");
    }

    void realBoard() {
        QFETCH(QString, file);
        const QString path = dir_.filePath(file);
        if (!QFileInfo::exists(path)) QSKIP(qPrintable(QStringLiteral("%1 is not there").arg(path)));
        infra::OpenCvImageSource source;
        const Result<ImageView> image = source.load(path.toStdString());
        QVERIFY2(image.ok(), image.ok() ? "" : image.error().message.c_str());
        const ImageView& view = image.value();
        qInfo("%s: %d x %d %s, %d bit (detected)", qPrintable(file), view.width, view.height, std::string(toString(view.format)).c_str(),
              effectiveBits(view));
        report("halcon", file, halcon_, view);
        report("opencv", file, opencv_, view);
    }
};

QTEST_GUILESS_MAIN(PerfHalconReal)
#include "perf_halcon_real.moc"
