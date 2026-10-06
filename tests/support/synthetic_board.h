#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/geometry.h"
#include "domain/image_view.h"

// 合成板子產生器（S06，測試用）：一張有元件陣列的影像，加上「標準答案」（每個元件的位姿、是否缺件、是否反光）。
// 用途：S07 以後的搜尋器同時量速度與準確度（召回率 T6a、位置誤差 T6b），S09 的契約測試，S10 的效能矩陣。
//
//  - 只用標準庫（domain 的型別），所以任何測試都能用；存成 PNG＋JSON 檔的部分在 synthetic_board_io.h（Qt）。
//  - 可重現：同樣的參數（含 seed）產生逐位元相同的影像與答案（自己的 xorshift32，不用 <random> 的分佈：
//    分佈的實作各家不同）。tiltDeg 不是 0 時，元件位置要算 sin、cos，不同的編譯器或 CPU 可能差最後一位，
//    所以「跨平台逐位元相同」只對 tiltDeg ＝ 0 保證（測試鎖住它的雜湊）；同一個執行檔裡永遠相同。
//  - 座標慣例照 geometry.h：像素 (i, j) 的中心在 (i + 0.5, j + 0.5)；角度在螢幕上逆時針為正。
//  - 每個元件：主體矩形（bodyLevel）兩端各有一個電極（padLevel，佔主體寬的 22%），像晶片電阻。邊緣用 4×4 超取樣做反鋸齒。
//  - 處理的順序像一台相機：畫元件 → 反光（飽和的亮斑）→ 光學模糊 → 感測器雜訊。
//  - 亂數的抽取順序固定：每個格子依列、欄的順序抽 4 個數（x 偏移、y 偏移、缺件、反光），不論那個效果有沒有開；
//    所以改缺件的比例，不會改到元件的位置。之後才是每個像素的雜訊。
namespace aoi::test {

struct SyntheticBoardSpec {
    int width = 1024;
    int height = 768;
    PixelFormat format = PixelFormat::Gray8;  // Gray8、Bgrx32（B ＝ G ＝ R ＝ 灰階值），或 Gray16（見 grayBits）
    // Gray16 的有效位元數（8..16）：值 ＝ 四捨五入 灰階 ×（2^grayBits − 1）／ 255，例如 12 bit 的值在 0..4095（低位對齊）；
    // 影像的 significantBits 也設成它。只有 Gray16 用。
    int grayBits = 12;
    // 陣列：rows × cols 個格子。格子 (r, c) 的中心 ＝ origin ＋ 轉 tiltDeg 之後的 (c·pitchX, r·pitchY) ＋ 隨機偏移。
    int rows = 6;
    int cols = 10;
    double pitchXPx = 80.0;
    double pitchYPx = 60.0;
    double originXPx = 120.0;
    double originYPx = 110.0;
    double tiltDeg = 0.0;  // 整個陣列與每個元件的角度
    double bodyWidthPx = 40.0;
    double bodyHeightPx = 20.0;
    double positionJitterPx = 0.0;  // 每個元件的中心在 x、y 各自 ±jitter 內均勻偏移
    double missingFraction = 0.0;   // 這個比例的格子不畫元件（缺件）
    double glareFraction = 0.0;     // 這個比例的元件上有一塊反光
    int blurRadius = 0;             // 方框模糊的半徑（像素），做三次（近似高斯）；0 ＝ 不模糊
    int noiseAmplitude = 0;         // 每個像素加上 [−a, a] 的均勻雜訊（灰階值）；0 ＝ 不加
    double umPerPixel = 10.0;       // 校正值（寫進影像與標準答案）
    std::uint32_t seed = 1;
    int backgroundLevel = 60;
    int bodyLevel = 150;
    int padLevel = 225;
};

struct SyntheticPart {
    int row = 0;
    int col = 0;
    Pose2D pose;  // 影像像素；中心與角度
    double widthPx = 0.0;
    double heightPx = 0.0;
    bool present = true;  // false ＝ 缺件：格子在，影像上沒有元件
    bool glare = false;   // 元件上有反光（只對 present 的有意義）
};

struct SyntheticBoard {
    SyntheticBoardSpec spec;
    ImageView image;                   // 擁有自己的緩衝；校正值 ＝ spec.umPerPixel
    std::vector<SyntheticPart> parts;  // 依列、欄排序；只有整個落在影像內的格子
    std::size_t presentCount() const;
};

// 參數不合法（尺寸 ≤ 0、格距 ≤ 0、比例不在 0..1、格式不是 Gray8／Bgrx32）時回傳 image 無效的板子。
SyntheticBoard makeSyntheticBoard(const SyntheticBoardSpec& spec);

// 標準答案（JSON 文字）：產生器的名稱與版本、全部參數、影像的大小與像素雜湊、每個元件。
std::string groundTruthJson(const SyntheticBoard& board);

// 像素內容的雜湊（FNV-1a 64 bit，逐列、不含 stride 的填充）：比對「同 seed 同輸出」用。
std::uint64_t pixelHash(const ImageView& image);

}  // namespace aoi::test
