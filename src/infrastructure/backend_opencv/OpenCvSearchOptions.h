#pragma once

#include <cstddef>

// infrastructure/backend_opencv：OpenCV 搜尋的調整參數（S07）。範本建立器與搜尋器共用同一份（Backend 建立時給）。
// 預設值是用合成板子與使用者的真實影像（docs/steps/S07-opencv/perf.md）定的起點，不是定理。
namespace aoi::infra {

struct OpenCvSearchOptions {
    // 金字塔：每層縮小一半，直到頂層範本的短邊剩 minTopSidePx（再縮就太模糊、太少像素可比）；最多 maxLevels 次。
    int minTopSidePx = 8;
    int maxLevels = 4;
    // 角度步進：範本轉一步，角落最多移動 cornerShiftPx 個像素（在那一層）。大的範本步進細、小的（頂層）步進粗。
    double cornerShiftPx = 1.0;
    // 每層最多幾個角度、所有旋轉範本最多佔多少記憶體：超過就加大步進（大範本 ＋ ±180° 時才會碰到）。
    int maxAnglesPerLevel = 720;
    std::size_t maxTemplateBytes = std::size_t{64} << 20;
    // 頂層與中間層的分數下限 ＝ minScore − coarseScoreDrop：縮小的影像、還沒對準的角度與位置，分數比最後低。
    double coarseScoreDrop = 0.20;
    // 往下一層時，在換算出來的位置 ± refineRadiusPx 個像素內找（上一層的 1 像素 ＝ 這一層的 2 像素）。
    int refineRadiusPx = 2;
    // 頂層的局部最大值：鄰域半徑 ＝ 範本邊長 × peakRadiusFraction（至少 1 個像素）。
    double peakRadiusFraction = 0.25;
    // 同一個元件被找到好幾次時只留一個：範本大小的旋轉矩形 IoU 超過這個值（和 use case 的 NmsOptions 預設相同）。
    double nmsMaxOverlap = 0.5;
    // 視窗搜尋：每個視窗最多精修幾個頂層候選（use case 每個缺格只要最好的那一個）。
    std::size_t maxCandidatesPerWindow = 4;
};

}  // namespace aoi::infra
