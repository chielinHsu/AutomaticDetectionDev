#pragma once

#include <string>

// infrastructure/backend_halcon：HALCON 搜尋的調整參數（S08）。範本建立器、搜尋器與樣本庫比對器共用同一份（Backend 建立時給）。
// 參數的意義照 HALCON 的運算子說明（%HALCONROOT%\help\operators_en_US.ref：create_shape_model、find_shape_model）。
// 預設值是起點：建模型的參數多半用 'auto'（HALCON 從樣本自己決定），角度步進與範圍除外（見下）；
// 合成板子與真實影像上的實測在 docs/steps/S08-halcon/perf.md。
// 2026-10-13（ADR-004 §5 的 B1–B3 修正，證據在 docs/steps/S09-contract-ab/halcon-accuracy-probe.md）改了兩件事：
// MaxOverlap 0.5 → 0.2、建模型與搜尋之前依樣本的邊寬與雜訊先平滑（adaptiveSmoothing；雜訊高的樣本同時用 least_squares_high）。
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
    // 'auto' 在雜訊與對比同一個量級的樣本（B1：對比 12、雜訊 ±4、模糊 1）會把門檻選在雜訊上，在雜訊大的樣本（B3：雜訊 12）
    // 會把門檻拉到主體的長邊之上；解法不是換門檻（換了也不對），是先平滑（adaptiveSmoothing）再讓 'auto' 選。
    int contrast = 0;
    int minContrast = 0;

    // ---- 預先平滑（2026-10-13，ADR-004 的 B1–B3 修正；證據在 docs/steps/S09-contract-ab/halcon-accuracy-probe.md）----
    // 建範本時先量樣本兩個數（HalconSearchSteps.h 的 estimateSampleStatistics）：
    //   邊寬 ＝ 灰階的 p95 − p5 ÷ 3×3 Sobel 梯度的 p99（≈ 邊緣幾個像素寬；合成板子上銳利 ≈ 2、模糊半徑 1 ≈ 4、2 ≈ 5.5、4 ≈ 8）；
    //   相對雜訊 ＝ 像素和它 3×3 平均的差的中位數 × 1.4826 ÷（p95 − p5）（雜訊 σ 佔灰階範圍的比例）。
    // 然後（決定寫在範本裡 HalconTemplateTuning，搜尋影像跟著做同樣的平滑，模型與搜尋影像才是同一種影像）：
    //   邊寬 ≥ wideEdgeWidth：建模型與搜尋之前影像先 mean_image（2·wideRadius+1）做 meanPasses 次（3 次方框模糊 ≈ 高斯 σ 2.4；
    //     HALCON 的 gauss_filter、binomial_filter 同樣的 σ 都比較差），SubPixel 用 'least_squares'（'least_squares_high' 在寬的邊上
    //     跟著雜訊走：重模糊的板子 0.89° 對 0.67°），而且只追蹤到金字塔第 wideEdgeLowestLevel 層（HALCON 的說明對失焦、雜訊的影像建議
    //     「最低層的邊緣資訊不夠，用高一層的」：重模糊的板子 0.67° → 0.58°；銳利的板子反而變差，所以只在邊很寬時用）。
    //   否則相對雜訊 ≥ noisyRatio：mean_image（2·noisyRadius+1）做 meanPasses 次（≈ σ 1.4），SubPixel 用 noisySubPixel
    //     （低對比＋雜訊的板子 'auto' 的對比門檻落在雜訊的量級，先平滑再讓它選；least_squares_high 在那裡 0.91° → 0.75°）。
    //     合成板子上的相對雜訊：低對比 B1 10%、雜訊 25 9%、雜訊 12 5%、雜訊 4 2%、40% 反光 1.8%、乾淨 0；4% 把前三種算進去。
    //   否則不平滑、SubPixel 用 subPixel：Gray8 整張零複製照舊，行為和時間都和 S08 相同（一般的板子不付平滑的代價：
    //     全部平滑時 T2b 慢一倍、T4 超出預算）。
    // 流程 B（HalconLibraryMatcher）不平滑：幾個模型共用一張搜尋影像，沒有「一個樣本」可以決定。
    bool adaptiveSmoothing = true;
    double wideEdgeWidth = 5.0;
    int wideRadius = 2;
    int wideEdgeLowestLevel = 2;
    double noisyRatio = 0.04;
    int noisyRadius = 1;
    std::string noisySubPixel = "least_squares_high";
    int meanPasses = 3;

    // ---- find_shape_model ----
    // 次像素：'interpolation'（從分數內插，幾乎不花時間）、'least_squares'（最小平方調整，較準、較慢；說明建議的取捨）、
    // 'least_squares_high'、'least_squares_very_high'。S08 實測（perf.md 的 SUB-*）：least_squares 的位置誤差是 interpolation 的一半；
    // _high 在那塊板子沒有更準、多 6 ms（所以只在雜訊高的樣本用，見 noisySubPixel）。'interpolation' 在模糊的板子不穩（同 σ 的三種
    // 濾波器 0.24、0.60、0.91°），不要用。
    std::string subPixel = "least_squares";
    // 0 ＝ 安全但慢的搜尋，1 ＝ 最快但偶爾漏掉。說明：「幾乎所有情況 0.9 都找得到」。
    double greediness = 0.9;
    // HALCON 自己的重疊抑制（用最小外接旋轉矩形）：重疊超過這個比例只留最好的。
    // 0.2（2026-10-13，原本 0.5）：同一個模型的兩個實例不可能重疊超過兩成。相鄰兩個元件的電極在半個格距的位置會被當成
    // 一個元件（極性一樣，只差主體上下邊那三成的點；重模糊時分數 0.83，低對比沒有雜訊時 0.75），它和兩邊真的元件各重疊 25%，
    // 0.5 擋不住、0.3 在沒有模糊的板子擋不住、0.2 全擋，而且六種板子上真的匹配一個都沒有少。
    double maxOverlap = 0.2;
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
