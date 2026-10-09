#include "application/compare/BackendComparison.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace aoi {

namespace {

struct Side {
    const SampleProposal* proposal = nullptr;
    double threshold = 0.0;
};

ObjectState stateOf(const Candidate& c, double threshold) {
    switch (c.source) {
        case CandidateSource::Sample: return ObjectState::Sample;
        case CandidateSource::Predicted: return ObjectState::Inferred;
        case CandidateSource::Seed:
        case CandidateSource::Verified: break;
    }
    return c.score >= threshold ? ObjectState::Found : ObjectState::Rejected;
}

bool isFound(ObjectState s) { return s == ObjectState::Sample || s == ObjectState::Found; }

CompareHit hitOf(const Candidate& c, double threshold) {
    CompareHit hit;
    hit.present = true;
    hit.pose = c.pose;
    hit.state = stateOf(c, threshold);
    if (c.hasScore()) hit.score = c.score;
    return hit;
}

CompareGap gapOf(const CompareHit& other) {
    if (!other.present) return CompareGap::NotFound;
    if (other.state == ObjectState::Inferred) return CompareGap::InferredOnly;
    return CompareGap::BelowThreshold;
}

CompareSideSummary summarize(const Side& side) {
    CompareSideSummary s;
    const SampleProposal& p = *side.proposal;
    s.candidates = p.candidates.size();
    s.sampleProblem = p.sampleProblem.has_value();
    s.lattice = p.grid.found;
    s.timings = p.timings;
    for (const Candidate& c : p.candidates) {
        switch (stateOf(c, side.threshold)) {
            case ObjectState::Sample:
            case ObjectState::Found: ++s.found; break;
            case ObjectState::Inferred: ++s.inferred; break;
            case ObjectState::Rejected: ++s.rejected; break;
        }
    }
    return s;
}

// 均勻格子：格子邊長 ＝ 容差，所以容差內的點一定在相鄰的 3×3 格子裡。
std::uint64_t cellKey(std::int64_t cx, std::int64_t cy) {
    return (static_cast<std::uint64_t>(cx) << 32) ^ static_cast<std::uint32_t>(cy);
}
std::int64_t cellOf(double v, double cell) { return static_cast<std::int64_t>(std::floor(v / cell)); }

struct Pair {
    std::size_t a = 0;
    std::size_t b = 0;
    double distance = 0.0;
};

// 兩邊所有候選在容差內的配對，由近到遠一對一（距離相同時依 A、再依 B 的索引：結果可重現）。
std::vector<Pair> pairCandidates(const std::vector<Candidate>& a, const std::vector<Candidate>& b, double tolerance) {
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> grid;
    grid.reserve(b.size());
    for (std::size_t j = 0; j < b.size(); ++j) {
        grid[cellKey(cellOf(b[j].pose.x, tolerance), cellOf(b[j].pose.y, tolerance))].push_back(j);
    }
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const std::int64_t cx = cellOf(a[i].pose.x, tolerance);
        const std::int64_t cy = cellOf(a[i].pose.y, tolerance);
        for (std::int64_t dy = -1; dy <= 1; ++dy) {
            for (std::int64_t dx = -1; dx <= 1; ++dx) {
                const auto it = grid.find(cellKey(cx + dx, cy + dy));
                if (it == grid.end()) continue;
                for (const std::size_t j : it->second) {
                    const double d = distance(a[i].pose.position(), b[j].pose.position());
                    if (d <= tolerance) pairs.push_back({i, j, d});
                }
            }
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) {
        if (x.distance != y.distance) return x.distance < y.distance;
        if (x.a != y.a) return x.a < y.a;
        return x.b < y.b;
    });
    std::vector<Pair> chosen;
    std::vector<bool> usedA(a.size(), false);
    std::vector<bool> usedB(b.size(), false);
    for (const Pair& p : pairs) {
        if (usedA[p.a] || usedB[p.b]) continue;
        usedA[p.a] = true;
        usedB[p.b] = true;
        chosen.push_back(p);
    }
    return chosen;
}

int kindOrder(CompareKind k) {
    switch (k) {
        case CompareKind::OnlyA: return 0;
        case CompareKind::OnlyB: return 1;
        case CompareKind::Both: return 2;
    }
    return 3;
}

// 把一邊配對後的結果加進清單與摘要。
void addItem(BackendComparison& out, ComparedItem item) {
    CompareSummary& s = out.summary;
    switch (item.kind) {
        case CompareKind::Both:
            ++s.both;
            break;
        case CompareKind::OnlyA:
            ++s.onlyA;
            if (item.gap == CompareGap::BelowThreshold) ++s.onlyABelowThreshold;
            else if (item.gap == CompareGap::InferredOnly) ++s.onlyAInferredOnly;
            else ++s.onlyANotFound;
            break;
        case CompareKind::OnlyB:
            ++s.onlyB;
            if (item.gap == CompareGap::BelowThreshold) ++s.onlyBBelowThreshold;
            else if (item.gap == CompareGap::InferredOnly) ++s.onlyBInferredOnly;
            else ++s.onlyBNotFound;
            break;
    }
    out.items.push_back(item);
}

}  // namespace

BackendComparison compareProposals(const SampleProposal& a, double thresholdA, const SampleProposal& b, double thresholdB,
                                   const CompareOptions& options) {
    BackendComparison out;
    const Side sa{&a, thresholdA};
    const Side sb{&b, thresholdB};
    out.summary.a = summarize(sa);
    out.summary.b = summarize(sb);

    // 容差依元件大小（兩邊的範本大小應該相同：同一個樣本框；取小的那一個，保守）。
    const auto shortSide = [](const SampleProposal& p) { return std::min(p.partSize.w, p.partSize.h); };
    double part = std::min(shortSide(a), shortSide(b));
    if (!(part > 0.0)) part = std::max(shortSide(a), shortSide(b));
    out.tolerancePx = std::max(options.minTolerancePx, part > 0.0 ? part * options.toleranceFraction : 0.0);

    const std::vector<Candidate>& ca = a.candidates;
    const std::vector<Candidate>& cb = b.candidates;
    std::vector<bool> pairedA(ca.size(), false);
    std::vector<bool> pairedB(cb.size(), false);
    double distanceSum = 0.0;

    for (const Pair& p : pairCandidates(ca, cb, out.tolerancePx)) {
        pairedA[p.a] = true;
        pairedB[p.b] = true;
        ComparedItem item;
        item.a = hitOf(ca[p.a], thresholdA);
        item.b = hitOf(cb[p.b], thresholdB);
        item.distancePx = p.distance;
        item.angleDiffDeg = normalizeAngleDeg(item.b.pose.angleDeg - item.a.pose.angleDeg);
        const bool foundA = isFound(item.a.state);
        const bool foundB = isFound(item.b.state);
        if (foundA && foundB) {
            item.kind = CompareKind::Both;
            distanceSum += item.distancePx;
            out.summary.maxDistancePx = std::max(out.summary.maxDistancePx, item.distancePx);
            out.summary.maxAngleDiffDeg = std::max(out.summary.maxAngleDiffDeg, std::abs(item.angleDiffDeg));
        } else if (foundA) {
            item.kind = CompareKind::OnlyA;
            item.gap = gapOf(item.b);
        } else if (foundB) {
            item.kind = CompareKind::OnlyB;
            item.gap = gapOf(item.a);
        } else {
            continue;  // 兩邊都沒找到（都 Rejected、都只是推算，或一個 Rejected 一個推算）
        }
        addItem(out, item);
    }
    for (std::size_t i = 0; i < ca.size(); ++i) {
        if (pairedA[i]) continue;
        ComparedItem item;
        item.a = hitOf(ca[i], thresholdA);
        if (!isFound(item.a.state)) continue;
        item.kind = CompareKind::OnlyA;
        item.gap = CompareGap::NotFound;
        addItem(out, item);
    }
    for (std::size_t j = 0; j < cb.size(); ++j) {
        if (pairedB[j]) continue;
        ComparedItem item;
        item.b = hitOf(cb[j], thresholdB);
        if (!isFound(item.b.state)) continue;
        item.kind = CompareKind::OnlyB;
        item.gap = CompareGap::NotFound;
        addItem(out, item);
    }
    if (out.summary.both > 0) out.summary.meanDistancePx = distanceSum / static_cast<double>(out.summary.both);

    std::stable_sort(out.items.begin(), out.items.end(), [](const ComparedItem& x, const ComparedItem& y) {
        if (x.kind != y.kind) return kindOrder(x.kind) < kindOrder(y.kind);
        const Pose2D px = x.pose();
        const Pose2D py = y.pose();
        if (px.y != py.y) return px.y < py.y;
        return px.x < py.x;
    });
    return out;
}

namespace {

// A 的進度放在 [0, 0.5)、B 的放在 [0.5, 1]：看數字就知道是哪一邊在跑（畫面的「比較中：OpenCV…」）。
class HalfProgress final : public IProgressSink {
public:
    HalfProgress(IProgressSink* target, double offset, double scale) : target_(target), offset_(offset), scale_(scale) {}
    void report(const ProgressEvent& event) noexcept override {
        if (target_ != nullptr) target_->report({event.stage, offset_ + scale_ * std::clamp(event.fraction, 0.0, 1.0)});
    }

private:
    IProgressSink* target_;
    double offset_;
    double scale_;
};

}  // namespace

Result<SideBySide> runSideBySide(const GenerateObjectsFromSample& a, const GenerateObjectsFromSample& b, const SampleRequest& request,
                                 const CancelToken& cancel, IProgressSink* progress) {
    HalfProgress first(progress, 0.0, 0.499);
    Result<SampleProposal> ra = a.run(request, cancel, &first);
    if (!ra) return ra.error();
    if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "The comparison was cancelled.");
    HalfProgress second(progress, 0.5, 0.5);
    Result<SampleProposal> rb = b.run(request, cancel, &second);
    if (!rb) return rb.error();
    return SideBySide{std::move(ra.value()), std::move(rb.value())};
}

}  // namespace aoi
