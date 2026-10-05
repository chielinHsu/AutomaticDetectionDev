#pragma once

#include <string_view>

// application 層：port。實作在 presentation（S04 的 Presenter 把它轉成 queued signal 回 UI 執行緒）。
namespace aoi {

// use case 的階段。流程 A：Template → Seed → Fit → Verify。流程 B 用 Match（每比對一個樣本回報一次）。
enum class Stage { Template, Seed, Fit, Verify, Match };

constexpr std::string_view toString(Stage s) noexcept {
    switch (s) {
        case Stage::Template: return "template";
        case Stage::Seed: return "seed";
        case Stage::Fit: return "fit";
        case Stage::Verify: return "verify";
        case Stage::Match: return "match";
    }
    return "unknown";
}

struct ProgressEvent {
    Stage stage = Stage::Template;
    double fraction = 0.0;  // 此階段完成的比例，0..1
};

// 契約：report 會在 worker 執行緒被呼叫，實作要能從任何執行緒呼叫，而且要很快（不阻塞 worker）。不丟例外。
class IProgressSink {
public:
    virtual ~IProgressSink() = default;
    virtual void report(const ProgressEvent& event) noexcept = 0;
};

}  // namespace aoi
