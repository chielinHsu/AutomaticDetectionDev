#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "application/backend/BackendRegistry.h"
#include "application/ports/IPatternSearcher.h"
#include "application/ports/ITemplateBuilder.h"
#include "infrastructure/backend_fake/FakeGrid.h"

// infrastructure/backend_fake：只用標準庫。設計說明見 FakeGrid.h。
namespace aoi::fake {

inline constexpr std::string_view kBackendId = "fake";

// 範本：假 back end 的「比對模型」。它帶著這個樣本對應的整個合成世界（有零件的格子與它們的位姿、分數），
// 搜尋器只讀它，不看影像。除了這兩個類別，沒有人看得懂裡面。
class FakeTemplate final : public ITemplate {
public:
    FakeTemplate(Size2D size, std::vector<FakeCell> parts, std::optional<NoMatchReason> problem)
        : size_(size), parts_(std::move(parts)), problem_(problem) {}

    Size2D size() const noexcept override { return size_; }
    std::string_view backendId() const noexcept override { return kBackendId; }
    std::optional<NoMatchReason> sampleProblem() const noexcept override { return problem_; }
    const std::vector<FakeCell>& parts() const noexcept { return parts_; }

private:
    Size2D size_;
    std::vector<FakeCell> parts_;  // 只含有零件的格子（Strong、Weak）
    std::optional<NoMatchReason> problem_;
};

class FakeTemplateBuilder final : public ITemplateBuilder {
public:
    explicit FakeTemplateBuilder(FakeSpec spec = {}) : spec_(std::move(spec)) {}
    std::string_view backendId() const noexcept override { return kBackendId; }
    Result<TemplatePtr> build(const ImageView& image, const Rect& sample, const SearchParams& params,
                              const CancelToken& cancel) override;

private:
    FakeSpec spec_;  // 建立之後不再改，所以可以被多個執行緒同時讀
};

class FakeSearcher final : public IPatternSearcher {
public:
    explicit FakeSearcher(FakeSpec spec = {}) : spec_(std::move(spec)) {}
    std::string_view backendId() const noexcept override { return kBackendId; }
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken& cancel) override;

private:
    FakeSpec spec_;
};

// 一組配套的假 adapter。組裝根以它註冊到 BackendRegistry（id ＝ "fake"）。
Backend makeFakeBackend(const FakeSpec& spec = {});

}  // namespace aoi::fake
