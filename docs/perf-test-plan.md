# 效能測試計畫（Performance Test Plan）

對應需求 4：測量**搜索比對時間**、**介面更新時間**、**介面互動**。
預算數字在 [`perf/budgets.json`](../perf/budgets.json)，目前是**提議值**（status = `proposed`），要在 S02（UI）與 S07（OpenCV）用本機實測校準。

<!--STE-->
## Purpose
This plan defines how we measure speed. We measure three things: the search time, the screen update time and the user interaction time.

## Reason
A fast program is a program that we can prove is fast. A feeling of speed is not enough. A number on one computer can differ from a number on another computer.

## Effect
Each step has a number to compare with a budget. A slow change fails a test before it reaches the main branch.
<!--/STE-->

繁中摘要：用數字證明「快」。三大類指標、固定量測方法、每次執行記錄機器指紋，慢了會在合併前失敗。

## 為什麼是這些門檻

人對回應時間的感受大致有三個界線（Nielsen 整理的經驗法則，源自 Miller 1968）：

| 延遲 | 使用者感受 | 本專案對應 |
|---|---|---|
| ≤ 0.1 s | 像是「立刻」反應 | 點擊/拖曳回饋（I2）、參數調整重繪（U3） |
| ≤ 1 s | 思緒不被打斷，但知道在等 | 放開滑鼠後出現提案框（T5，目標 ≤ 300 ms） |
| ≥ 10 s | 注意力離開 | 一定要有進度與取消（I4） |

另外 60 fps ＝ 每幀 16.7 ms，這是 I1、I5 的來源。

## 量測方法（所有類別共用）

1. **熱機**：丟掉前 5 次；**正式量 30 次**；回報**中位數與 p95**（平均值會被離群值拉動）。
2. **機器指紋**：每份結果記錄 CPU、核心數、RAM、GPU、OS、Qt / OpenCV / HALCON 版本、編譯器與 build type（必須是 Release 或 Profile；CMake 時代叫 RelWithDebInfo，2026-10-07 起建置只用 qmake，ADR-018）、commit hash。
3. **不同機器的數字不互相比較**；基準線（baseline）存在 `perf/baseline/<machine-id>.json`。
4. **回歸閘門**：與 baseline 相比慢超過 15%（`regressionLimitPercent`）或超過預算 → 測試失敗。
5. **準確度與速度一起量**：只快不準沒有意義（T6）。
6. **合成資料（synthetic）優先**：`SyntheticBoard` 產生可重現的陣列影像（指定 pitch、角度、雜訊、缺件、模糊），有 ground truth，不依賴機密的產線影像。
7. **輸出**：`perf/results/<date>-<commit>-<machine>.json`（不進版控）＋ S10 產生的互動式 HTML 報表。

## A. 搜索比對時間（Search / Matching）

| 項目 | 指標 | 量法 |
|---|---|---|
| T1 | 範本建立時間（64×32 樣本） | `QBENCHMARK`，包住 `ITemplateBuilder::build` |
| T2 | 種子搜尋時間（5 MP 影像） | 包住 `IPatternSearcher::find`（種子區域） |
| T3 | 晶格擬合（1 000 點） | domain 微基準，不含任何影像庫 |
| T4 | 視窗驗證（預測 200 個元件） | 包住 verify 迴圈 |
| T5 | 端到端：放開滑鼠 → 提案就緒 | use case 入口到出口，`PerfProbe` 各階段切片 |
| T6 | 準確度：召回率、位置誤差 | 與 ground truth 比對 |

**測試矩陣**（S10 實作，每格 × 兩個 back end）：

| 維度 | 值 |
|---|---|
| 影像大小 | 1 MP、5 MP、12 MP、25 MP |
| 樣本大小 | 32×16、64×32、128×128 |
| 旋轉範圍 | 0°、±5°、±180° |
| 元件數量 | 10、100、1 000 |
| 難度 | 乾淨、雜訊、缺件 5%、反光/低對比 |

額外輸出各階段切片（範本建立 / 種子搜尋 / 擬合 / 驗證 / 產生物件）與**峰值記憶體**，以便 S11 找出最大的一塊。

## B. 介面更新時間（UI Update）

| 項目 | 指標 | 量法 |
|---|---|---|
| U1 | 結果就緒 → 第一個繪製完成的畫面（200 / 5 000 框） | 結果 signal 時開始計時；在 canvas 的 paint 結束 hook 停止計時 |
| U2 | 物件表格填入 5 000 列 | model reset → 第一次 layout 完成 |
| U3 | 參數 slider 變動 → 重繪 | 事件注入 → paint 完成 |
| U4 | 開啟 5 MP 影像 → 第一次繪製 | 開檔 → paint 完成 |

## C. 介面互動（Interaction）

| 項目 | 指標 | 量法 |
|---|---|---|
| I1 | 平移/縮放時的幀時間 p95（5 000 框） | `QTest` 注入滾輪與拖曳事件，連續記錄每幀間隔 |
| I2 | 滑鼠按下 → 可見回饋 | 事件注入 → 下一次 paint |
| I3 | 搜尋進行中，事件迴圈最長阻塞時間 | 看門狗：每 10 ms 的 `QTimer` 量實際間隔 |
| I4 | 按取消 → UI 回到 idle | `CancelToken` 觸發 → 狀態機進入 Idle |
| I5 | 拖曳樣本框時的幀時間 p95 | 同 I1 |
| I6 | 復原一次 1 000 框的 commit | `UndoStack::undo()` 計時 |

## 工具與執行環境

- 框架：**Qt Test**（`QBENCHMARK`、`QTest::mouseMove` 等）。一個測試框架即可涵蓋 domain 到 UI（S03 定案）。
- 兩種執行模式：
  - `QT_QPA_PLATFORM=offscreen`：CI 與回歸用，穩定、不需要顯示器。
  - 真機視窗模式：量 GPU 與實際顯示相關的指標（I1、I5），結果另外標示。
- 計時：`std::chrono::steady_clock`；UI 端用 `QElapsedTimer`。`PerfProbe` 關閉時成本應可忽略（S03 驗證，目標 < 1 µs／次）。
- 回歸比較：`tools/perf-compare.mjs`（S10）。
- 建置與執行（2026-10-07 起，[ADR-018](adr/018-qmake-only-build.md)）：`scripts/dev/test.ps1 -Perf` 在 qmake 的 Release 建置（OpenCV＋HALCON 預設開）跑 `AiInspTool/tests/tests.pro` 的 `AOI_PERF_TESTS`；新的效能量測執行檔要加 `.pro` 並列進那份清單。`offscreen` 由編進每個測試的 `tests/support/test_environment.cpp` 設定（已經有設定時不覆寫，所以真機視窗模式先設 `QT_QPA_PLATFORM=windows`）。

## 每一步要量什麼

| 步驟 | 該步驟必須量並記錄在 `perf.md` 的項目 |
|---|---|
| S02 | U1、U4、I1、I2、I5（UI 基準線） |
| S03 | `PerfProbe` 開/關的額外成本 |
| S04 | U2、U3、I3、I4、I6 |
| S05 | T3、NMS 微基準 |
| S06 | 影像載入時間、**複製次數 = 0** 的斷言 |
| S07 | T1、T2a、T4、T6（OpenCV） |
| S08 | T2b、T6（HALCON） |
| S09 | 兩個 back end 的 A/B 時間與準確度對照 |
| S10 | 完整矩陣、T5、回歸閘門、報表 |
| S11 | 每個優化的 before / after |
| S12 | 發行版（Release）重跑全套，更新 baseline |
