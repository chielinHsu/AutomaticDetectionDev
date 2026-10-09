#pragma once

#include <string_view>

// 契約測試（S09）的容差與 back end 的特性：**只寫在這裡**（S09 任務 1：「容差寫在一個地方並說明依據」）。
// 改一個數字之前，先有量測（docs/steps/S09-contract-ab/perf.md、ADR-004）；放寬要在 ADR-004 寫理由。
//
// 為什麼分 back end：契約（port 標頭的文字：排序、區域、空結果、取消、錯誤碼）每個 back end 都一樣；
// 「找得多準」不是 port 的契約，是品質：兩種方法（強度式 NCC、邊緣式 shape model）的準確度不同，差異記在 ADR-004，
// 這裡給每一邊明確的上限。假 back end 不看影像（S03：結果來自 FakeGrid），所以準確度的測試不包括它。
namespace aoi::contract {

// 搜尋器在合成板子上的準確度（tst_contract_searcher；1024×768、6×10 個 40×20 的元件、樣本框外加 4 像素 ＝ 48×28）。
struct SearcherTolerances {
    // 乾淨的板子（沒有雜訊、沒有傾斜）：每個元件的中心誤差與角度誤差上限；每個匹配的分數下限。
    // 依據：S07（OpenCV）、S08（HALCON）的單元測試同一組上限，兩邊實測都在 0.1 px、0.1° 以內。
    // 0.25 px 也是座標慣例的檢查：像素中心差半個像素（ADR-010 的 ±0.5）一定會超過。
    double cleanPositionPx = 0.25;
    double cleanAngleDeg = 0.3;
    double cleanMinScore = 0.9;
    // 板子整個傾斜 ±3°、+4.5°（角度的正負號，ADR-010）：同 S07、S08 的上限。
    double tiltedPositionPx = 0.35;
    double tiltedAngleDeg = 0.35;
    // 角度掃描（−5..5° 每 0.5°、−30..30° 每 2.5°）：角度誤差 × 範本半對角線 ≤ 這個值（範本角落的位移）。
    // 依據：S07 的變異 M11 與 S08 的 AS-*；48×28 的範本 0.3 px ＝ 0.62°。
    double cornerShiftPx = 0.3;
    // 同一個元件，視窗搜尋與整張影像搜尋的位置差（兩種區域，結果要是同一個位姿）。
    // 依據：S08 實測 HALCON 0.00 px；S09 實測 OpenCV（docs/steps/S09-contract-ab/perf.md）。
    double windowVsWholePx = 0.05;
    // 子影像（原點平移 96、64 像素）與整張影像的位置差（座標原點的檢查）。
    double subViewPx = 0.01;
};

// use case 端到端（tst_contract_end_to_end；1280×960、12×14 個元件，8 種情況）：T6a、T6b 的單元版本。
struct EndToEndTolerances {
    double positionMaxPx = 0.5;  // perf/budgets.json 的 T6b
    double angleMaxDeg = 0.6;    // 每個候選的 |角度|（樣本與其他元件同一個方向）；S07 的上限
    // 強雜訊（noiseAmplitude ≥ 12）時改用「框的角落位移」的上限（像素），0 ＝ 不改，仍用 angleMaxDeg。
    // HALCON：S08 實測最大 0.84–0.90°（48×28 的範本角落位移 0.41 px）；shape model 只看邊緣點，雜訊大的小元件角度
    // 比 NCC（用整塊像素）不穩。這是 ADR-004 接受的差異，上限是 T6b 的 0.5 px。
    double strongNoiseCornerShiftPx = 0.0;
};

// 流程 B 的樣本庫比對器（tst_contract_library_matcher）。
struct LibraryTolerances {
    // 辨識出的元件中心和標準答案的距離：T6b 的 0.5 px。S09 實測 OpenCV ≤ 0.001、HALCON ≤ 0.004 px（參考影像從同樣的板子裁下來）；
    // 兩個 back end 之間同一個元件的位置差也用它（實測 0.004 px）。
    double positionPx = 0.5;
};

struct ContractProfile {
    std::string_view id;
    bool readsPixels = true;        // false：不看影像（假 back end），準確度的測試不包括它
    bool hasLibraryMatcher = true;  // 有 ILibraryMatcher 的實作（流程 B）
    SearcherTolerances searcher;
    EndToEndTolerances endToEnd;
    LibraryTolerances library;
};

inline ContractProfile profileOf(std::string_view id) {
    ContractProfile p;
    p.id = id;
    if (id == "fake") {
        p.readsPixels = false;
        p.hasLibraryMatcher = false;
    } else if (id == "halcon") {
        p.endToEnd.strongNoiseCornerShiftPx = 0.5;
    }
    return p;
}

// 取消的契約：cancel 升起之後，搜尋器要在這麼久之內回 Cancelled。等於 perf/budgets.json 的 I4（按取消到 UI 閒置）：
// UI 不等 worker（latest-wins），但 worker 停不下來就會佔住執行緒，下一次搜尋要等。tst_contract_searcher 檢查兩邊一致。
inline constexpr double kCancelLatencyMs = 100.0;

}  // namespace aoi::contract
