#include "infrastructure/perf/PerfProbe.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>

namespace aoi::infra {

namespace {

std::uint32_t currentThreadNumber() {
    static std::atomic<std::uint32_t> next{1};
    thread_local const std::uint32_t mine = next.fetch_add(1, std::memory_order_relaxed);
    return mine;
}

std::string jsonString(std::string_view s) {
    std::string out = "\"";
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    out += '"';
    return out;
}

std::string number(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

// 最近排名法（nearest-rank）：p 是 0..1。values 要已排序、非空。
double percentile(const std::vector<double>& sorted, double p) {
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(sorted.size())));
    return sorted[std::min(sorted.size(), std::max<std::size_t>(1, rank)) - 1];
}

}  // namespace

PerfProbe::PerfProbe(std::size_t capacity, bool enabled)
    : enabled_(enabled), slots_(std::max<std::size_t>(1, capacity)), epoch_(std::chrono::steady_clock::now()) {}

void PerfProbe::record(const char* name, std::int64_t durationNs) noexcept {
    const auto endNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - epoch_).count();
    const std::uint32_t thread = currentThreadNumber();
    std::lock_guard lock(mutex_);
    slots_[next_] = Record{name != nullptr ? name : "", endNs, durationNs, thread};
    next_ = (next_ + 1) % slots_.size();
    if (count_ < slots_.size()) {
        ++count_;
    } else {
        ++dropped_;
    }
}

void PerfProbe::clear() {
    std::lock_guard lock(mutex_);
    next_ = 0;
    count_ = 0;
    dropped_ = 0;
}

std::size_t PerfProbe::recorded() const {
    std::lock_guard lock(mutex_);
    return count_;
}

std::size_t PerfProbe::dropped() const {
    std::lock_guard lock(mutex_);
    return dropped_;
}

std::vector<PerfProbe::Record> PerfProbe::snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<Record> out;
    out.reserve(count_);
    const std::size_t oldest = count_ < slots_.size() ? 0 : next_;
    for (std::size_t k = 0; k < count_; ++k) out.push_back(slots_[(oldest + k) % slots_.size()]);
    return out;
}

std::string PerfProbe::toJson(bool includeRecords) const {
    const std::vector<Record> records = snapshot();
    const std::size_t droppedCount = dropped();

    std::map<std::string, std::vector<double>> byName;  // name → 各次的毫秒
    for (const Record& r : records) byName[r.name].push_back(static_cast<double>(r.durationNs) / 1e6);

    std::string json = "{\n  \"schema\": 1,\n";
    json += "  \"capacity\": " + std::to_string(capacity()) + ",\n";
    json += "  \"recorded\": " + std::to_string(records.size()) + ",\n";
    json += "  \"dropped\": " + std::to_string(droppedCount) + ",\n";
    json += "  \"summary\": [";
    bool first = true;
    for (auto& [name, values] : byName) {
        std::sort(values.begin(), values.end());
        double total = 0.0;
        for (const double v : values) total += v;
        json += first ? "\n" : ",\n";
        first = false;
        json += "    {\"name\": " + jsonString(name) + ", \"count\": " + std::to_string(values.size()) +
                ", \"minMs\": " + number(values.front()) + ", \"medianMs\": " + number(percentile(values, 0.5)) +
                ", \"p95Ms\": " + number(percentile(values, 0.95)) + ", \"maxMs\": " + number(values.back()) +
                ", \"totalMs\": " + number(total) + "}";
    }
    json += first ? "]" : "\n  ]";
    if (includeRecords) {
        json += ",\n  \"records\": [";
        for (std::size_t k = 0; k < records.size(); ++k) {
            const Record& r = records[k];
            json += k == 0 ? "\n" : ",\n";
            json += "    {\"name\": " + jsonString(r.name) + ", \"thread\": " + std::to_string(r.thread) +
                    ", \"endNs\": " + std::to_string(r.endNs) + ", \"durationNs\": " + std::to_string(r.durationNs) + "}";
        }
        json += records.empty() ? "]" : "\n  ]";
    }
    json += "\n}\n";
    return json;
}

Result<void> PerfProbe::writeJson(std::string_view path, bool includeRecords) const {
    try {
        const std::u8string utf8(reinterpret_cast<const char8_t*>(path.data()), path.size());
        std::ofstream file(std::filesystem::path{utf8}, std::ios::binary | std::ios::trunc);
        if (!file) return makeError(ErrorCode::IoFailed, "Cannot open '" + std::string(path) + "' for writing.");
        const std::string json = toJson(includeRecords);
        file.write(json.data(), static_cast<std::streamsize>(json.size()));
        file.flush();
        if (!file) return makeError(ErrorCode::IoFailed, "Cannot write '" + std::string(path) + "'.");
        return Result<void>::success();
    } catch (const std::exception& e) {
        return makeError(ErrorCode::Internal, e.what());
    }
}

}  // namespace aoi::infra
