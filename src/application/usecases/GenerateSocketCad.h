#pragma once
#include "application/ports/ISocketCadGenerator.h"

namespace aoi {
class GenerateSocketCad final : public ISocketCadGenerator {
public:
    explicit GenerateSocketCad(std::shared_ptr<const ISocketRegionRecognizer> recognizer) : recognizer_(std::move(recognizer)) {}
    Result<SocketCadResult> generate(const SocketCadRequest& request, const CancelToken& cancel) const override;
private:
    std::shared_ptr<const ISocketRegionRecognizer> recognizer_;
};
}  // namespace aoi
