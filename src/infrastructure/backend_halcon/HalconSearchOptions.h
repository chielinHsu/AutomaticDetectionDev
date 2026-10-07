#pragma once

#include <string>

// infrastructure/backend_halcon：HALCON 搜尋的調整參數（S08）。範本建立器、搜尋器與樣本庫比對器共用同一份（Backend 建立時給）。
// 參數的意義照 HALCON 的運算子說明（%HALCONROOT%\help\operators_en_US.ref：create_shape_model、find_shape_model）。
// 預設值是起點：建模型的參數多半用 'auto'（HALCON 從樣本自己決定），角度步進與範圍除外（見下）；
// 合成板子與真實影像上的實測在 docs/steps/S08-halcon/perf.md。
namespace aoi::infra {

struct HalconSearchOptions {
    // ---- create_shape_model ----
    // 金字塔層數：0 ＝ 'auto'。
    int numLevels = 0;
    // 角度步進（度）。> 0：照這個值。否則 cornerShiftPx > 0 時自己算：範本轉一步，角落移動 cornerShiftPx 個像素
    // （步進 ＝ atan(cornerShiftPx ／ 半對角線)，和 OpenCV 版同一個想法）；兩個都是 0 ＝ HALCON 的 'auto'。
    // 不用 'auto' 的原因（S08 實測，perf.md）：64×32 以下的樣本 'auto' 給 5.2°，±5° 的範圍裡只剩 −5、0、5 三個角度，
    // 次像素精修拉不回來（傾斜 3° 的元件有一半回報 5°）。
    double angleStepDeg = 0.0;
    double cornerShiftPx = 0.5;
    // 模型的角度範圍比要求的兩端各多 angleMarginSteps 步（整圈時不加）：範圍剛好等於要求時，接近兩端的元件精修會被範圍卡住
    // （S08 實測：傾斜 −4° 的元件回報 −5°、位置差 0.4 像素）。搜尋仍只要求使用者的範圍，結果的角度夾回範圍內（契約）。
    double angleMarginSteps = 2.0;
    // 'auto'、'none'、'point_reduction_low|medium|high'：模型點太多時要不要減少。
    std::string optimization = "auto";
    // 'use_polarity'：亮暗方向要和樣本一樣（元件比底色亮就一直比較亮）。另有 'ignore_global_polarity'、'ignore_local_polarity'。
    std::string metric = "use_polarity";
    // 邊緣的對比門檻（灰階）：0 ＝ 'auto'。最小對比（搜尋時，低於它的梯度當成雜訊）：0 ＝ 'auto'。
    int contrast = 0;
    int minContrast = 0;

    // ---- find_shape_model ----
    // 次像素：'interpolation'（從分數內插，幾乎不花時間）、'least_squares'（最小平方調整，較準、較慢；說明建議的取捨）、
    // 'least_squares_high'。S08 實測（perf.md 的 SUB-*）：least_squares 的位置誤差是 interpolation 的一半；_high 沒有更準、多 6 ms。
    std::string subPixel = "least_squares";
    // 0 ＝ 安全但慢的搜尋，1 ＝ 最快但偶爾漏掉。說明：「幾乎所有情況 0.9 都找得到」。
    double greediness = 0.9;
    // HALCON 自己的重疊抑制（用最小外接旋轉矩形）：重疊超過這個比例只留最好的。
    double maxOverlap = 0.5;
    // 之後再做一次 domain 的 NMS（範本大小的旋轉矩形，IoU），和 OpenCV、use case 的規則一樣：
    // 視窗清單的視窗彼此重疊時，同一個元件會被找到兩次。
    double nmsMaxOverlap = 0.5;

    // ---- 影像 ----
    // false（預設）：Gray16 依有效位元數換成 8 bit 再搜尋（一次複製；和 OpenCV 同一個輸入，A/B 比較公平）。
    // true：Gray16 直接當 HALCON 的 uint2（緊密排列時零複製）。shape model 的分數看梯度方向，和亮度的倍數無關；
    // 對比門檻 'auto' 是從樣本自己估的，所以位元數不影響結果。perf.md 量兩種。
    bool nativeGray16 = false;

    // ---- 流程 B（HalconLibraryMatcher）----
    // true（預設）：所有候選類型的模型一次 find_shape_models（說明：「比分開呼叫有效率得多」）；false：每個類型一次 find_shape_model。
    bool libraryOneCall = true;
};

}  // namespace aoi::infra
