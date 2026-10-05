#pragma once

#include <QtTest>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "application/ports/IPatternSearcher.h"
#include "application/ports/ITemplateBuilder.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "infrastructure/backend_fake/FakeBackend.h"
#include "infrastructure/backend_fake/FakeGrid.h"

// 核心測試共用的輔助。核心不看影像內容（假 back end），但 use case 會檢查樣本的對比，所以影像要有紋理。
namespace aoi::test {

// 合成的灰階影像：8 像素的棋盤加斜向漸層，任何 8×8 以上的區域亮度標準差都遠大於 use case 的 minContrast。
class TestImage {
public:
    TestImage(int width, int height, bool flat = false)
        : width_(width), height_(height), pixels_(std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height))) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int checker = ((x / 8 + y / 8) & 1) * 90;
                const int ramp = (x + y) % 60;
                (*pixels_)[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                    flat ? std::uint8_t{128} : static_cast<std::uint8_t>(40 + checker + ramp);
            }
        }
    }

    ImageView view() const {
        ImageView v;
        v.data = pixels_->data();
        v.width = width_;
        v.height = height_;
        v.strideBytes = width_;
        v.format = PixelFormat::Gray8;
        v.owner = pixels_;
        return v;
    }

private:
    int width_;
    int height_;
    std::shared_ptr<std::vector<std::uint8_t>> pixels_;
};

inline SampleRequest makeRequest(const TestImage& image, Rect sample) {
    SampleRequest request;
    request.image = image.view();
    request.sample = sample;
    return request;
}

// 轉手給真的假 back end，但記下被呼叫了幾次（驗證「樣本有問題就不搜尋」之類的事）。
class CountingBuilder final : public ITemplateBuilder {
public:
    explicit CountingBuilder(fake::FakeSpec spec = {}) : inner_(std::move(spec)) {}
    std::string_view backendId() const noexcept override { return inner_.backendId(); }
    Result<TemplatePtr> build(const ImageView& image, const Rect& sample, const SearchParams& params,
                              const CancelToken& cancel) override {
        ++calls;
        return inner_.build(image, sample, params, cancel);
    }
    std::atomic<int> calls{0};

private:
    fake::FakeTemplateBuilder inner_;
};

class CountingSearcher final : public IPatternSearcher {
public:
    explicit CountingSearcher(fake::FakeSpec spec = {}) : inner_(std::move(spec)) {}
    std::string_view backendId() const noexcept override { return inner_.backendId(); }
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken& cancel) override {
        ++calls;
        if (region.kind == SearchRegion::Kind::Windows) ++windowCalls;
        return inner_.find(image, tmpl, region, params, cancel);
    }
    std::atomic<int> calls{0};
    std::atomic<int> windowCalls{0};

private:
    fake::FakeSearcher inner_;
};

inline bool approxEqual(double a, double b, double tolerance = 1e-9) { return std::abs(a - b) <= tolerance; }

inline std::size_t countSource(const std::vector<Candidate>& candidates, CandidateSource source) {
    std::size_t n = 0;
    for (const Candidate& c : candidates) n += c.source == source ? 1u : 0u;
    return n;
}

}  // namespace aoi::test
