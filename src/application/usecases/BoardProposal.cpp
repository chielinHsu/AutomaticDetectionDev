#include "application/usecases/BoardProposal.h"

#include <algorithm>
#include <cmath>

namespace aoi {

namespace {

BoardCandidate toBoard(const Candidate& c, const std::string& typeId, Size2D partSize) {
    BoardCandidate b;
    b.typeId = typeId;
    b.pose = c.pose;
    b.score = c.score;
    b.source = c.source;
    b.partSize = partSize;
    return b;
}

RectObj boxOf(const BoardCandidate& c) { return RectObj{c.pose, c.partSize.w, c.partSize.h}; }

void addTimings(StageTimings& total, const StageTimings& t) {
    total.templateMs += t.templateMs;
    total.seedMs += t.seedMs;
    total.fitMs += t.fitMs;
    total.verifyMs += t.verifyMs;
    total.totalMs += t.totalMs;
}

}  // namespace

BoardProposal boardProposalFrom(const SampleProposal& proposal, std::string typeId) {
    BoardProposal out;
    out.sampleProblem = proposal.sampleProblem;
    out.samplesSearched = proposal.sampleProblem ? 0 : 1;
    out.timings = proposal.timings;
    out.candidates.reserve(proposal.candidates.size());
    for (const Candidate& c : proposal.candidates) out.candidates.push_back(toBoard(c, typeId, proposal.partSize));
    return out;
}

BoardProposal mergeSampleProposals(const std::vector<SampleRun>& runs, const std::vector<RectObj>& occupied,
                                   const NmsOptions& nms) {
    BoardProposal out;

    // 格子的大小：所有樣本中最長的對角線（太小也正確，只是看的格子多）。
    double cell = 1.0;
    for (const SampleRun& run : runs) {
        const Size2D s = run.proposal.partSize;
        cell = std::max(cell, std::sqrt(s.w * s.w + s.h * s.h));
    }
    for (const RectObj& r : occupied) cell = std::max(cell, std::sqrt(r.width * r.width + r.height * r.height));
    OverlapIndex taken(cell, nms.maxOverlap, nms.measure);
    for (const RectObj& r : occupied) taken.add(r);

    std::optional<NoMatchInfo> firstProblem;
    std::vector<BoardCandidate> scored;
    std::vector<BoardCandidate> predicted;
    for (const SampleRun& run : runs) {
        addTimings(out.timings, run.proposal.timings);
        if (run.proposal.sampleProblem) {
            if (!firstProblem) firstProblem = run.proposal.sampleProblem;
            continue;
        }
        ++out.samplesSearched;
        for (const Candidate& c : run.proposal.candidates) {
            BoardCandidate b = toBoard(c, run.typeId, run.proposal.partSize);
            switch (c.source) {
                case CandidateSource::Sample:
                    taken.add(boxOf(b));  // 樣本自己已經在板子上：只佔位，不放進結果
                    break;
                case CandidateSource::Predicted:
                    predicted.push_back(std::move(b));
                    break;
                case CandidateSource::Seed:
                case CandidateSource::Verified:
                    scored.push_back(std::move(b));
                    break;
            }
        }
    }
    if (out.samplesSearched == 0) {
        out.sampleProblem = firstProblem;
        return out;
    }

    // 分數由高到低；同分時依 y、再依 x（和搜尋器的排序規則相同），再依原本的順序（stable）。
    std::stable_sort(scored.begin(), scored.end(), [](const BoardCandidate& a, const BoardCandidate& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.pose.y != b.pose.y) return a.pose.y < b.pose.y;
        return a.pose.x < b.pose.x;
    });

    // 被手動元件佔住的先剔除（excluded），和合併（merged）分開計數：兩個數字在卡片上的意義不同。
    if (!occupied.empty()) {
        OverlapIndex manual(cell, nms.maxOverlap, nms.measure);
        for (const RectObj& r : occupied) manual.add(r);
        auto dropOccupied = [&](std::vector<BoardCandidate>& v) {
            std::vector<BoardCandidate> kept;
            kept.reserve(v.size());
            for (BoardCandidate& c : v) {
                if (manual.overlapsAny(boxOf(c))) ++out.excluded;
                else kept.push_back(std::move(c));
            }
            v = std::move(kept);
        };
        dropOccupied(scored);
        dropOccupied(predicted);
    }
    for (BoardCandidate& c : scored) {
        const RectObj box = boxOf(c);
        if (taken.overlapsAny(box)) {
            ++out.merged;
            continue;
        }
        taken.add(box);
        out.candidates.push_back(std::move(c));
    }
    for (BoardCandidate& c : predicted) {
        const RectObj box = boxOf(c);
        if (taken.overlapsAny(box)) {
            ++out.merged;
            continue;
        }
        taken.add(box);
        out.candidates.push_back(std::move(c));
    }
    return out;
}

}  // namespace aoi
