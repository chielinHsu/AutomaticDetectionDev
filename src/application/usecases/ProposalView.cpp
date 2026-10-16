#include "application/usecases/ProposalView.h"

#include <algorithm>
#include <cmath>

namespace aoi {

ProposalView classify(const BoardProposal& proposal, double threshold, double candidateFloor, PositionMode mode) {
    ProposalView view;
    if (proposal.sampleProblem) {
        view.noMatch = proposal.sampleProblem;
        return view;
    }

    view.objects.reserve(proposal.candidates.size());
    double highestOther = 0.0;
    bool anyOther = false;  // 有沒有 Sample 以外、帶分數的候選
    bool anySample = false;
    int nextId = 1;
    for (const BoardCandidate& c : proposal.candidates) {
        InspectionObject object;
        object.id = nextId++;
        // 位置（ADR-026）：Lattice 模式用格點；Predicted 的 pose 本來就是格點，所以兩種模式都在格點上。
        const bool snap = mode == PositionMode::Lattice && c.latticePose.has_value();
        object.rect = RectObj{snap ? *c.latticePose : c.pose, c.partSize.w, c.partSize.h};
        object.onLattice = snap || c.source == CandidateSource::Predicted;
        object.latticeDeviationPx = c.latticeDeviationPx;
        object.origin = ObjectOrigin::Auto;
        object.typeId = c.typeId;
        switch (c.source) {
            case CandidateSource::Sample:
                object.state = ObjectState::Sample;
                object.score = c.score;
                anySample = true;
                break;
            case CandidateSource::Predicted:
                object.state = ObjectState::Inferred;
                ++view.inferred;
                break;
            case CandidateSource::Seed:
            case CandidateSource::Verified:
                object.score = c.score;
                object.state = c.score >= threshold ? ObjectState::Found : ObjectState::Rejected;
                if (object.state == ObjectState::Found) ++view.found; else ++view.rejected;
                anyOther = true;
                highestOther = std::max(highestOther, c.score);
                break;
        }
        // 不在格點上的（兩種模式都一樣算：它是「候選有沒有晶格位姿」，不是「現在顯示在哪」）：卡片上提醒使用者這些位置是匹配的。
        if (!c.onLattice() && (object.state == ObjectState::Found || object.state == ObjectState::Inferred)) ++view.offLattice;
        view.objects.push_back(object);
    }

    if (view.found == 0) {
        // 只有樣本自己（或什麼都沒有）：板上沒有別的相似元件。合併過的提案不含樣本本身，所以「空的」也算。
        const bool onlySample = proposal.candidates.size() <= (anySample ? std::size_t{1} : std::size_t{0});
        if (onlySample) {
            view.noMatch = NoMatchInfo{NoMatchReason::OnlyOne, 0.0, 0.0, 0};
        } else if (anyOther) {
            // 建議值取到小數第二位、向下取整，才保證最高分的候選過得了新門檻；不低於搜尋下限。
            const double suggested = std::max(candidateFloor, std::floor(highestOther * 100.0) / 100.0);
            view.noMatch = NoMatchInfo{NoMatchReason::ThresholdTooHigh, highestOther, suggested, 0};
        }
    }
    return view;
}

ProposalView classify(const SampleProposal& proposal, double threshold, double candidateFloor, PositionMode mode) {
    return classify(boardProposalFrom(proposal, std::string()), threshold, candidateFloor, mode);
}

}  // namespace aoi
