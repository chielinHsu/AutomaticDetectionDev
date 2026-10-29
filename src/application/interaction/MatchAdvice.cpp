#include "application/interaction/MatchAdvice.h"

namespace aoi {

std::vector<SampleOnBoard> MatchAdvice::pending(const InspectionSession& session, const BackendTraits& traits) const {
    std::vector<SampleOnBoard> out;
    if (!traits.suggestsMatchMask) return out;
    if (session.state() != InteractionState::Idle) return out;
    const InspectionDocument& document = session.document();
    for (SampleOnBoard& sample : document.samples()) {
        const ComponentType* type = document.findType(sample.typeId);
        if (type == nullptr || !type->match.isDefault()) continue;  // 已經設了比對區（或改了特徵）：使用者處理過了
        if (dismissed(sample.typeId)) continue;
        out.push_back(std::move(sample));
    }
    return out;
}

void MatchAdvice::dismiss(const std::vector<SampleOnBoard>& samples) {
    for (const SampleOnBoard& sample : samples) dismissed_.insert(sample.typeId);
}

}  // namespace aoi
