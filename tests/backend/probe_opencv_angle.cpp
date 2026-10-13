// 探測（fix/halcon-accuracy，2026-10-13；不列進 tests.pro）：ADR-004 §5 的 O1——OpenCV 在 B1 低對比板子上的角度。
#include <opencv2/imgproc.hpp>

#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "application/CancelToken.h"
#include "domain/image_buffer.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvTemplate.h"
#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"
#include "infrastructure/backend_opencv/OpenCvBackend.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

inline const char* aoiEnv(const char* name) {
    static std::map<std::string, std::string> cache;
    const QByteArray v = qgetenv(name);
    if (v.isEmpty()) return nullptr;
    return cache.emplace(name, v.toStdString()).first->second.c_str();
}

SyntheticBoardSpec abBoardSpec() {
    SyntheticBoardSpec s;
    s.width = 2592;
    s.height = 1944;
    s.bodyWidthPx = 56.0;
    s.bodyHeightPx = 24.0;
    s.pitchXPx = 84.0;
    s.pitchYPx = 60.0;
    s.originXPx = 120.0;
    s.originYPx = 110.0;
    s.cols = static_cast<int>((s.width - 2 * s.originXPx) / s.pitchXPx) + 1;
    s.rows = static_cast<int>((s.height - 2 * s.originYPx) / s.pitchYPx) + 1;
    s.noiseAmplitude = 4;
    s.blurRadius = 1;
    s.seed = 17;
    return s;
}

struct BoardCase {
    std::string name;
    SyntheticBoardSpec spec;
};

std::vector<BoardCase> boards() {
    std::vector<BoardCase> out;
    const char* only = aoiEnv("AOI_PROBE_BOARDS");
    const std::string filter = only != nullptr ? only : "";
    const auto addCase = [&](const char* name, SyntheticBoardSpec s) {
        if (filter.empty() || filter.find(name) != std::string::npos) out.push_back({name, s});
    };
    SyntheticBoardSpec b1 = abBoardSpec();
    b1.bodyLevel = 72;
    b1.padLevel = 84;
    addCase("B1", b1);
    SyntheticBoardSpec b1nn = b1;
    b1nn.noiseAmplitude = 0;
    addCase("B1nn", b1nn);
    SyntheticBoardSpec b1nb = b1;
    b1nb.blurRadius = 0;
    addCase("B1nb", b1nb);
    addCase("noise4", abBoardSpec());
    SyntheticBoardSpec n12 = abBoardSpec();
    n12.noiseAmplitude = 12;
    n12.blurRadius = 2;
    addCase("noise12", n12);
    SyntheticBoardSpec b2 = abBoardSpec();
    b2.blurRadius = 4;
    addCase("B2", b2);
    SyntheticBoardSpec tilt = abBoardSpec();
    tilt.tiltDeg = 3.0;
    addCase("tilt3", tilt);
    return out;
}

struct Variant {
    std::string name;
    OpenCvSearchOptions options;
    int preSmoothRadius = 0;  // > 0：建範本用的影像先做 3 次方框模糊（半徑 r）；搜尋仍用原圖
    double gaussSigma = 0.0;  // > 0：建範本用的影像先做高斯模糊（σ）；搜尋仍用原圖
};

std::vector<Variant> variants() {
    std::vector<Variant> out;
    const char* only = aoiEnv("AOI_PROBE_VARIANTS");
    const std::string filter = only != nullptr ? only : "";
    const auto addCase = [&](const char* name, Variant v) {
        v.name = name;
        if (filter.empty() || filter.find(name) != std::string::npos) out.push_back(std::move(v));
    };
    addCase("default", {});
    for (const double c : {0.5, 0.25}) {
        Variant v;
        v.options.cornerShiftPx = c;
        addCase(("corner" + std::to_string(c).substr(0, 4)).c_str(), v);
    }
    for (const int r : {1, 2}) {
        Variant v;
        v.preSmoothRadius = r;
        addCase(("preblur" + std::to_string(r)).c_str(), v);
        v.options.cornerShiftPx = 0.25;
        addCase(("preblur" + std::to_string(r) + "corner0.25").c_str(), v);
    }
    for (const double s : {0.5, 0.7, 1.0}) {
        Variant v;
        v.gaussSigma = s;
        addCase(("gauss" + std::to_string(s).substr(0, 3)).c_str(), v);
        v.options.cornerShiftPx = 0.25;
        addCase(("gauss" + std::to_string(s).substr(0, 3) + "corner0.25").c_str(), v);
    }
    return out;
}

// 3 次方框模糊的複本（和 synthetic_board 同一種）。
ImageBuffer smoothed(const ImageView& view, int r) {
    ImageBuffer out(view.width, view.height, PixelFormat::Gray8);
    std::vector<std::uint8_t> gray(static_cast<std::size_t>(view.width) * static_cast<std::size_t>(view.height));
    for (int y = 0; y < view.height; ++y)
        std::memcpy(gray.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.width), view.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.strideBytes), static_cast<std::size_t>(view.width));
    const auto boxLine = [](const std::uint8_t* in, std::uint8_t* o, int n, std::ptrdiff_t step, int radius) {
        const int window = 2 * radius + 1;
        auto at = [&](int i) { return static_cast<int>(in[static_cast<std::ptrdiff_t>(std::clamp(i, 0, n - 1)) * step]); };
        int sum = 0;
        for (int i = -radius; i <= radius; ++i) sum += at(i);
        for (int i = 0; i < n; ++i) {
            o[static_cast<std::ptrdiff_t>(i) * step] = static_cast<std::uint8_t>((sum + window / 2) / window);
            sum += at(i + radius + 1) - at(i - radius);
        }
    };
    std::vector<std::uint8_t> tmp(gray.size());
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < view.height; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(view.width);
            boxLine(gray.data() + row, tmp.data() + row, view.width, 1, r);
        }
        for (int x = 0; x < view.width; ++x) boxLine(tmp.data() + x, gray.data() + x, view.height, view.width, r);
    }
    for (int y = 0; y < view.height; ++y) std::memcpy(out.row(y), gray.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.width), static_cast<std::size_t>(view.width));
    return out;
}

class FixedTemplateBuilder final : public ITemplateBuilder {
public:
    explicit FixedTemplateBuilder(TemplatePtr t) : t_(std::move(t)) {}
    std::string_view backendId() const noexcept override { return t_->backendId(); }
    Result<TemplatePtr> build(const ImageView&, const Rect&, const SearchParams&, const CancelToken&) override { return t_; }
private:
    TemplatePtr t_;
};

QString range(std::vector<double> v) {
    if (v.empty()) return QStringLiteral("-");
    std::sort(v.begin(), v.end());
    const double p95 = v[static_cast<std::size_t>(0.95 * static_cast<double>(v.size() - 1))];
    return QStringLiteral("min %1 med %2 p95 %3 max %4").arg(v.front(), 0, 'f', 3).arg(v[v.size() / 2], 0, 'f', 3).arg(p95, 0, 'f', 3).arg(v.back(), 0, 'f', 3);
}

}  // namespace

class ProbeOpenCvAngle : public QObject {
    Q_OBJECT

private slots:
    void probe() {
        SearchParams params;
        params.minScore = 0.70;
        params.angleMinDeg = -5.0;
        params.angleMaxDeg = 5.0;
        for (const BoardCase& bc : boards()) {
            const SyntheticBoard board = makeSyntheticBoard(bc.spec);
            QVERIFY(board.image.valid());
            const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
            qInfo("==== board %s: noise %d blur %d body %d pad %d tilt %g", bc.name.c_str(), bc.spec.noiseAmplitude, bc.spec.blurRadius,
                  bc.spec.bodyLevel, bc.spec.padLevel, bc.spec.tiltDeg);
            for (const Variant& v : variants()) {
                OpenCvTemplateBuilder builder(v.options);
                OpenCvPatternSearcher searcher(v.options);
                ImageBuffer smoothedImage = v.preSmoothRadius > 0 ? smoothed(board.image, v.preSmoothRadius) : ImageBuffer();
                if (v.gaussSigma > 0.0) {
                    smoothedImage = ImageBuffer(board.image.width, board.image.height, PixelFormat::Gray8);
                    cv::Mat src(board.image.height, board.image.width, CV_8UC1, const_cast<std::uint8_t*>(board.image.data), static_cast<std::size_t>(board.image.strideBytes));
                    cv::Mat dst(smoothedImage.height(), smoothedImage.width(), CV_8UC1, smoothedImage.data(), static_cast<std::size_t>(smoothedImage.strideBytes()));
                    cv::GaussianBlur(src, dst, cv::Size(0, 0), v.gaussSigma, v.gaussSigma, cv::BORDER_REPLICATE);
                }
                const bool pre = v.preSmoothRadius > 0 || v.gaussSigma > 0.0;
                const ImageView templateImage = pre ? smoothedImage.view() : board.image;
                const Result<TemplatePtr> built = builder.build(templateImage, sample, params, CancelToken());
                QVERIFY2(built.ok(), built.ok() ? "" : built.error().message.c_str());
                const auto* t = dynamic_cast<const OpenCvTemplate*>(built.value().get());
                QVERIFY(t != nullptr);
                QString levels;
                for (const OpenCvTemplateLevel& lv : t->levels()) levels += QStringLiteral(" %1x%2 step %3 (%4 angles)").arg(lv.width).arg(lv.height).arg(lv.angleStepDeg, 0, 'f', 3).arg(lv.anglesDeg.size());
                // 一個典型的元件（第 100 個）：第 0 層每個角度的範本放在它的中心，NCC 分數對角度的曲線。
                {
                    const SyntheticPart& part = board.parts[100];
                    const OpenCvTemplateLevel& lv = t->levels()[0];
                    const double offX = sample.center().x - board.parts[middlePresentIndex(board)].pose.x;
                    const double offY = sample.center().y - board.parts[middlePresentIndex(board)].pose.y;
                    const int px = static_cast<int>(std::lround(part.pose.x + offX - lv.width * 0.5));
                    const int py = static_cast<int>(std::lround(part.pose.y + offY - lv.height * 0.5));
                    cv::Mat whole(board.image.height, board.image.width, CV_8UC1, const_cast<std::uint8_t*>(board.image.data), static_cast<std::size_t>(board.image.strideBytes));
                    const cv::Mat patch = whole(cv::Rect(px, py, lv.width, lv.height));
                    QString curve;
                    for (std::size_t k = 0; k < lv.anglesDeg.size(); ++k) {
                        cv::Mat result;
                        cv::matchTemplate(patch, lv.rotated[k], result, cv::TM_CCOEFF_NORMED);
                        curve += QStringLiteral(" %1:%2").arg(lv.anglesDeg[k], 0, 'f', 2).arg(result.at<float>(0, 0), 0, 'f', 4);
                    }
                    qInfo("  [%s] NCC vs angle at part 100:%s", v.name.c_str(), qPrintable(curve));
                }
                FixedTemplateBuilder fixedBuilder(built.value());
                GenerateObjectsFromSample uc(fixedBuilder, searcher, nullptr);
                SampleRequest request;
                request.image = board.image;
                request.sample = sample;
                request.search = params;
                const Result<SampleProposal> r = uc.run(request, CancelToken());
                QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
                const SampleProposal& p = r.value();
                std::vector<double> angles, scores, errors;
                const double offX = sample.center().x - board.parts[middlePresentIndex(board)].pose.x;
                const double offY = sample.center().y - board.parts[middlePresentIndex(board)].pose.y;
                std::size_t over06 = 0, over10 = 0, falsePositives = 0;
                for (const Candidate& c : p.candidates) {
                    if (!c.hasScore()) continue;
                    const double a = std::abs(c.pose.angleDeg - bc.spec.tiltDeg);
                    angles.push_back(a);
                    scores.push_back(c.score);
                    if (a > 0.6) ++over06;
                    if (a > 1.0) ++over10;
                    double best = std::numeric_limits<double>::max();
                    for (const SyntheticPart& part : board.parts) {
                        if (part.present) best = std::min(best, distance(c.pose.position(), {part.pose.x + offX, part.pose.y + offY}));
                    }
                    if (best > 3.0) ++falsePositives;
                    else errors.push_back(best);
                }
                // 角度的直方圖（0.2° 一格）
                std::vector<int> hist(8, 0);
                for (const double a : angles) ++hist[std::min<std::size_t>(7, static_cast<std::size_t>(a / 0.2))];
                QString histText;
                for (std::size_t i = 0; i < hist.size(); ++i) histText += QStringLiteral(" [%1-%2):%3").arg(0.2 * i, 0, 'f', 1).arg(0.2 * (i + 1), 0, 'f', 1).arg(hist[i]);
                qInfo("  [%s] levels:%s | candidates %zu scored %zu false %zu | pos err %s | |angle-tilt| %s; >0.6: %zu, >1.0: %zu | hist%s | score %s | %.1f ms (seed %.1f verify %.1f)",
                      v.name.c_str(), qPrintable(levels), p.candidates.size(), angles.size(), falsePositives, qPrintable(range(errors)), qPrintable(range(angles)), over06, over10,
                      qPrintable(histText), qPrintable(range(scores)), p.timings.totalMs, p.timings.seedMs, p.timings.verifyMs);
            }
        }
    }
};

QTEST_GUILESS_MAIN(ProbeOpenCvAngle)
#include "probe_opencv_angle.moc"
