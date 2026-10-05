#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "application/Result.h"
#include "application/ports/IPerfProbe.h"

// infrastructure/perf：只用標準庫。IPerfProbe 的實作：環狀緩衝加 JSON 輸出。
namespace aoi::infra {

// 設計：
//  - 預設是關閉的。關閉時 enabled() 只是一次原子讀取，ScopedTimer 不讀時鐘，record() 不會被呼叫。
//  - 開啟時 record() 取一個互斥鎖、寫一格、前進一格。成本在 perf.md（實測，不是目標值）。
//    各階段是毫秒等級的區間，所以鎖的競爭可以忽略；若將來要量微秒等級的熱路徑，要先量再改成 lock-free。
//  - 環狀緩衝：滿了就覆蓋最舊的，並累計被覆蓋的筆數（dropped），所以長時間執行不會無限成長。
//  - name 只存指標（IPerfProbe 的契約：字串字面值）。
class PerfProbe final : public IPerfProbe {
public:
    struct Record {
        const char* name = "";
        std::int64_t endNs = 0;       // 結束時刻，距離 PerfProbe 建立（steady_clock）
        std::int64_t durationNs = 0;
        std::uint32_t thread = 0;     // 小整數的執行緒編號（依第一次記錄的順序，從 1 起）
    };

    explicit PerfProbe(std::size_t capacity = 65536, bool enabled = false);

    bool enabled() const noexcept override { return enabled_.load(std::memory_order_relaxed); }
    void record(const char* name, std::int64_t durationNs) noexcept override;

    void setEnabled(bool on) noexcept { enabled_.store(on, std::memory_order_relaxed); }
    void clear();

    std::size_t capacity() const noexcept { return slots_.size(); }
    std::size_t recorded() const;  // 目前在緩衝裡的筆數
    std::size_t dropped() const;   // 被覆蓋掉的筆數
    std::vector<Record> snapshot() const;  // 由舊到新

    // JSON：{ schema, capacity, recorded, dropped, summary:[{name,count,minMs,medianMs,p95Ms,maxMs,totalMs}], records:[…] }。
    // includeRecords ＝ false 時只輸出 summary（檔案小很多）。
    std::string toJson(bool includeRecords = true) const;
    Result<void> writeJson(std::string_view path, bool includeRecords = true) const;

private:
    std::atomic<bool> enabled_;
    mutable std::mutex mutex_;
    std::vector<Record> slots_;
    std::size_t next_ = 0;     // 下一次寫入的格子
    std::size_t count_ = 0;    // 緩衝裡的筆數（≤ capacity）
    std::size_t dropped_ = 0;
    std::chrono::steady_clock::time_point epoch_;
};

}  // namespace aoi::infra
