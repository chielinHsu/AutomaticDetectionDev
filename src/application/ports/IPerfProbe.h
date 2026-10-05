#pragma once

#include <chrono>
#include <cstdint>

// application 層：port。實作在 infrastructure/perf。
namespace aoi {

// 契約：
//  - enabled() 很便宜（一次原子讀取），可以在熱路徑呼叫。關閉時 record() 不應該被呼叫。
//  - record 的 name 必須活得比 probe 久（字串字面值）：probe 只存指標，不複製字串。
//  - 可以從任何執行緒呼叫。不丟例外。
class IPerfProbe {
public:
    virtual ~IPerfProbe() = default;
    virtual bool enabled() const noexcept = 0;
    virtual void record(const char* name, std::int64_t durationNs) noexcept = 0;
};

// 量一段程式的時間。probe 是空指標或關閉時，不讀時鐘（建構與解構都只是幾個指令）。
// 「開或關」只在建構時決定一次，所以中途切換不會記到半段的時間。
//   { ScopedTimer t(probe, "uc.sample.seed"); ...要量的程式...; }
class ScopedTimer {
public:
    ScopedTimer(IPerfProbe* probe, const char* name) noexcept
        : probe_(probe != nullptr && probe->enabled() ? probe : nullptr), name_(name) {
        if (probe_ != nullptr) start_ = Clock::now();
    }
    ~ScopedTimer() {
        if (probe_ != nullptr) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_);
            probe_->record(name_, elapsed.count());
        }
    }
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    using Clock = std::chrono::steady_clock;
    IPerfProbe* probe_;
    const char* name_;
    Clock::time_point start_{};
};

}  // namespace aoi
