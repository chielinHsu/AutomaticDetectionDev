#pragma once

#include <memory>
#include "application/ports/ISocketSampleSearcher.h"
#include "application/ports/ITemplateBuilder.h"
#include "application/ports/IPatternSearcher.h"

namespace aoi {
// 嚴格全圖樣本匹配；不使用低分晶格補漏。adapter 由 app 組裝，生命期由 shared_ptr 保持。
class SearchSocketSamples final : public ISocketSampleSearcher {
public:
    SearchSocketSamples(std::shared_ptr<ITemplateBuilder> builder, std::shared_ptr<IPatternSearcher> searcher)
        : builder_(std::move(builder)), searcher_(std::move(searcher)) {}
    Result<std::vector<SocketSampleMatch>> search(const SocketSampleRequest& request,
                                                 const CancelToken& cancel) const override;
private:
    std::shared_ptr<ITemplateBuilder> builder_;
    std::shared_ptr<IPatternSearcher> searcher_;
};
}  // namespace aoi
