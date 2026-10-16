#include "application/interaction/StepGuide.h"

#include <algorithm>

namespace aoi {

namespace {

bool hasAutoComponents(const InspectionDocument& document) {
    return std::any_of(document.components().begin(), document.components().end(),
                       [](const ComponentInstance& c) { return c.origin == ObjectOrigin::Auto; });
}

// 板上有沒有「還沒搜尋過」的樣本：它的類型在板上沒有任何自動產生的元件。接受之後再框一個樣本就是這種；
// 接受過的樣本，它的類型至少有一個找到的元件。InspectionSession 回到 Idle 時會清掉 lastSearch()（接受、捨棄、取消、
// 建立樣本都會），所以不能拿它判斷。已知的限制：一個樣本搜尋後一個都沒找到，接受之後它仍算「還沒搜尋過」，
// 面板會繼續指著〔搜尋〕（訊息列的提示照舊）。
bool anySampleWithoutResults(const InspectionDocument& document) {
    for (const SampleOnBoard& sample : document.samples()) {
        const bool found = std::any_of(document.components().begin(), document.components().end(), [&](const ComponentInstance& c) {
            return c.origin == ObjectOrigin::Auto && c.typeId == sample.typeId;
        });
        if (!found) return true;
    }
    return false;
}

}  // namespace

GuideStatus guideStatus(const InspectionSession& session) {
    using S = InteractionState;
    GuideStatus g;
    const bool image = session.hasImage();
    const bool samples = session.hasSamples();
    const bool accepted = hasAutoComponents(session.document());
    const S state = session.state();

    g.done[static_cast<std::size_t>(GuideStep::OpenImage)] = image;
    g.done[static_cast<std::size_t>(GuideStep::DrawSample)] = samples || accepted;
    g.done[static_cast<std::size_t>(GuideStep::Search)] = state == S::Preview || accepted;
    g.done[static_cast<std::size_t>(GuideStep::Accept)] = accepted;

    switch (state) {
        case S::Empty:
            g.current = GuideStep::OpenImage;
            break;
        case S::DrawingRegion:
            g.current = GuideStep::DrawSample;
            g.busy = true;
            break;
        case S::Searching:
            g.current = GuideStep::Search;
            g.busy = true;
            break;
        case S::Preview:
            g.current = GuideStep::Accept;
            break;
        case S::NoMatch:
            g.current = GuideStep::Search;
            g.attention = true;
            break;
        case S::Error:
            g.current = session.errorSource() == ErrorSource::Image ? GuideStep::OpenImage : GuideStep::Search;
            g.attention = true;
            break;
        case S::EditingType:
            g.current = GuideStep::Refine;
            break;
        case S::Idle:
            if (!samples) g.current = accepted ? GuideStep::Refine : GuideStep::DrawSample;
            else if (anySampleWithoutResults(session.document())) g.current = GuideStep::Search;  // 新的樣本、捨棄或取消之後
            else g.current = GuideStep::Refine;
            break;
    }
    return g;
}

}  // namespace aoi
