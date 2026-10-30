#pragma once
#include "application/ports/ISocketCadGenerator.h"

namespace aoi::infra {
// Socket 專用結構模型：端點尺度族群、分離的較大焊墊、左右配對與重複排列。
// 不依賴使用者樣本，不呼叫 NCC，不冒稱通用 PCB 元件辨識。
class OpenCvSocketRegionRecognizer final : public ISocketRegionRecognizer {
public:
    Result<SocketRegionObservation> recognize(const ImageView& image,const Rect& region,
                                             const CancelToken& cancel) const override;
};
}  // namespace aoi::infra
