# S10 — 效能測試 harness 與回歸閘門

| 項目 | 內容 |
|---|---|
| 層級 | 驗證 |
| 分支 | `step/10-perf-harness` |
| 依賴 | S02–S09 |
| 產出位置 | `tests/perf/`、`tools/perf-compare.mjs`、`perf/baseline/`、`docs/steps/S10-perf-harness/` |

> **S01 修訂（2026-10-05）**：效能矩陣要加「樣本庫大小 N」維度（ADR-006、`docs/steps/S01-ux-spec/perf.md` 新增項目）。

> **S03 完成後的提醒（2026-10-05）**：S03 的 `PerfProbe` 與切片名稱（`uc.sample.template`、`.seed`、`.fit`、`.verify`、`.total`、`job.queue`、`job.run`）已經就位，可輸出 JSON；`perf_core` 的量測方法（熱機、p95、機器指紋、連跑 3 次看範圍）可沿用；核心基準線在 `perf/baseline/casparhsu1-core.json`。

> **S05 完成後的提醒（2026-10-06）**：（1）T3 的微基準是 `tests/domain/perf_lattice`（`fitAll`，1 000 點；Release 把預算當成失敗條件），另量 NMS（1 000／10 000 框）與 O(1) 漸進重新擬合對每次重算；基準線在 `perf/baseline/casparhsu1-lattice.json`。S05 改了 use case 的擬合階段，所以 `perf_core` 的 P4 在 S05 重量過（同一個檔案的 `useCase`）；`casparhsu1-core.json` 的 P4 還是 S03 暫代版的數字，不要拿來當回歸的基準。（2）晶格的成本隨形狀變：隨機點（沒有晶格，每個 u 的候選都試）是 1 000 點裡最慢的（p95 3.1–4.2 ms，超過 T3 的 2 ms，但 T3 量的是乾淨的陣列），矩陣要有「不成陣列的點」這一格；T3 要不要涵蓋這種輸入，是對預算的決定。（3）同一個小時內，完全沒改到的 NMS 與累加器量測也差到 1.4 倍：閘門要用多次執行的範圍。（4）use case 的 `uc.sample.fit` 切片現在包含 NMS 之後的晶格擬合與缺格預測。

> **S04 完成後的提醒（2026-10-06）**：（1）新增 `perf_ui_s04`（U2、U3、I3、I4、I6）與 `perf_paint_micro` 的 `dashedAndDottedFrames`；基準線在 `perf/baseline/casparhsu1-presenter.json`。PerfProbe 新增 `ui.*` 切片：`ui.threshold`、`ui.threshold.refilter`、`ui.threshold.params`、`ui.publishAll`、`ui.publishScene`、`ui.scene.build`、`ui.scene.canvas`、`ui.scene.tree`、`ui.chrome`、`ui.draft`、`ui.selection`。（2）同一份程式在這台機器連跑 3 次會差到 30%，I3 的餘裕只有 16–38%：閘門不能拿單次 p95 比預算，要用多次執行的範圍。（3）效能矩陣要加兩個維度：被拒絕（點線）的框的比例（ADR-012 之前成本差 12 倍）、背景已接受的元件數（`U3-bg` 比沒有背景慢約 16–28 ms）。（4）`perf_ui_s04` 用 `knownOver("U3")` 把已知超標的 U3 排除在失敗條件之外（其他指標超標仍會失敗）；閘門要有同樣的「已知、不擋」機制，並在 S11 處理 U3 後拿掉。

> **S06 完成後的提醒（2026-10-06）**：（1）`tests/infra/perf_storage`（效能量測）量影像載入（Qt 與 OpenCV、5 MP 與 25 MP、BMP／PNG／JPEG／TIFF／16 bit）、轉換的複製次數、配方 500／5 000 個元件的存讀、背景存檔佔呼叫端的時間、1 000 筆樣本庫；**這些都沒有預算**（要不要加，是對預算的決定）。基準線在 `perf/baseline/casparhsu1-storage.json`。U4 的量測改走產品的 `QtImageSource`。（2）S06 起用 **Qt 6.12.0**：`docs/steps/S06-image-storage/perf.md` 有同一個時段、同一份程式在 6.8.0 與 6.12.0 各跑一次全部效能測試的比較；S02–S05 的基準線是 6.8.0 量的，回歸閘門比較時要注意 Qt 版本。（3）開圖在 UI 執行緒解碼：25 MP 的 PNG 約 0.4 秒畫面停住，矩陣要有這一格。

> **S07 完成後的提醒（2026-10-06）**：（1）`tests/backend/perf_opencv`（效能量測）量 T1、T2a、T4、T6a／T6b、T7（樣本庫 N ＝ 1、10、100、1000）、角度範圍／影像大小（1–25 MP）／樣本大小的曲線、OpenCV 的執行緒數、use case 的各階段（`UC-*`）、峰值記憶體（背景執行緒每 1 ms 取樣 private bytes）；結果 `perf/results/opencv_*.json` 有 `curves` 一節（`explainer.html` 用它），基準線 `perf/baseline/casparhsu1-opencv.json`。（2）`perf/budgets.json` 的 T1、T2a、T4、T6a、T6b 已在 S07 校準（limit 沒有改）；T5 只有 use case 的部分（`UC-total`，沒有 UI），T7 的預算只是提案（`perf.md`，等使用者決定）。（3）`perf_opencv_real` 要設 `AOI_REAL_IMAGE_DIR` 才跑，沒有設就跳過（`test.ps1` 算通過）：回歸閘門不要依賴它。

> **S08 完成後的提醒（2026-10-07）**：（1）`tests/backend/perf_halcon`（效能量測；預設的建置就有 OpenCV＋HALCON）量 L（程式庫載入與授權檢查：第一次約 1.6 秒）、T1、T2b（預算 80 ms）、T2b-16（12 bit：換 8 bit 與原生 `uint2`）、T4、T6a／T6b、SUB-*（次像素的三種做法）、AS-*（角度步進與範圍餘量）、A／S／P／TH 曲線、UC-*、C-search（取消延遲）、MEM-*、T7（一次 `find_shape_models`、每個類型一次、OpenCV 三種）；OpenCV 也編進來時同一次量 `CV-*`。結果 `perf/results/halcon_*.json`，基準線 `perf/baseline/casparhsu1-halcon.json`。（2）`perf/budgets.json` 的 T2b 在 S08 校準（limit 沒有改）：3 次中 1 次（整台機器變慢的那次）p95 82.2 ms 超過 80，另外兩次與補跑的一次 29–35 ms；**HALCON 的時間在幾次之間差得比 OpenCV 多**。HALCON 的 T6b p95 0.48 px，**離預算 0.5 很近**。閘門要用多次執行的範圍，而且要能看出「整台機器變慢」（同一次的 OpenCV、UI 指標也一起變慢）。（3）`perf_halcon`、`tst_halcon_*` 在沒有 HALCON 授權的電腦會跳過（`test.ps1` 算通過）：回歸閘門要分辨「跳過」與「通過」。（4）授權檢查只在第一次建立 HALCON back end 時做（約 1.6 秒），T5（端到端）如果用 halcon，要決定算不算這一次。

> **建置改 qmake 之後的提醒（2026-10-07，[ADR-018](../adr/018-qmake-only-build.md)）**：建置只用 qmake（`AiInspTool/AiInspTool.pro`；CMake、preset、CTest 都已移除，S02–S08 文件裡的那些指令是歷史紀錄，對照表在 ADR-018），執行檔叫 `AiInspTool.exe`（GUI 子系統），在 `<建置>/bin`。OpenCV 與 HALCON **預設編進去**（以前的 preset `release-halcon`）；關掉用 qmake 變數 `AOI_WITH_OPENCV=OFF`／`AOI_WITH_HALCON=OFF`。命令列：`scripts/dev/test.ps1`（建置＋測試；`-Config Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`）；Qt Creator 開 `AiInspTool/AiInspTool.pro` 也能建置、執行與跑測試。**新增測試**：在 `AiInspTool/tests/<資料夾>/` 加一個 `.pro`（照同資料夾的寫法：`TARGET`、需要時 `AOI_LINK_EXTRA`、`include(<資料夾>_tests.pri)`、`SOURCES`），再列進 `AiInspTool/tests/tests.pro` 的 `AOI_TESTS`（效能量測列進 `AOI_PERF_TESTS`；只有某個 back end 才有的放進對應的 `equals(AOI_WITH_…, ON)` 區塊）。**新增原始碼**要加進對應的 `.pro`（不會自動納入）。S09 之前還有一個 session B（授權提醒的卡片、執行中切換 back end、Designer 的 `.ui`），開工時先看 `.ai/handoff.md`。效能量測不再有 CTest 的 label：清單是 `tests.pro` 的 `AOI_PERF_TESTS`，`test.ps1 -Perf` 只跑它們（每項逾時 300 秒，`-TimeoutSec` 可以改）。harness 的新執行檔（本計畫寫 `aoi_perf`）**不要叫 `aoi_perf`**：那是 PerfProbe 程式庫（`aoi_perf.lib`）的名字，會混淆。回歸閘門要分辨「跳過」與「通過」：`test.ps1` 最後列出「有跳過項目的測試」（讀 log 的 Totals 行），可以沿用。`AOI_BUILD_TYPE`（結果 JSON 的 `buildType`）現在是 `Release`／`Debug`／`Profile`（CMake 的 `RelWithDebInfo` 對應 `Profile`），`AOI_PERF_ENFORCE` 在 Release 與 Profile 定義。測試的工作資料夾是 `build/qmake-*/test-work/`（`perf-results`、`perf-images` 在那裡）；`perf/results/` 不變。

> **S09 完成後的提醒（2026-10-09，[ADR-004](../adr/004-backend-divergence.md)）**：（1）**A/B 並排的量測**是 `tests/contract/perf_ab`（`-Perf`）：組裝根註冊的每一個會讀像素的 back end（沒有的 SKIPPED 並說原因）、12 種 5 MP 合成板子（T6 的情況加上光照不均、低對比、重模糊、大片反光、強雜訊），時間、召回率、位置誤差、分數的分布、峰值記憶體，兩個都能用時再用 `compareProposals` 比一次；結果在 `perf/results/ab_*.json`，每一種情境的比較在 `<建置>/test-work/ab-*.json`。S10 的效能矩陣可以直接用 `tests/contract/ContractSupport.h` 的資料列（每個 back end 一列）與這組情境。（2）`tests/ui/perf_compare_ui`：A/B 疊圖的 AB-U1a（約 200 個標記，用 U1a 的預算）、AB-U1b（約 5 600 個，用 U1b 的預算）、AB-U3（改門檻之後重新比較，**沒有預算**，p95 48.8–63.4 ms）、AB-CMP（`compareProposals` 本身 7–9 ms）；`perf/budgets.json` 沒有改，要不要給 AB-U3 預算是 S10 的事。數字見 `docs/steps/S09-contract-ab/perf.md`。（3）預設的建置現在是 **56 項**（`-NoHalcon` 54、`-NoOpenCv` 53、兩個都關 51）。（4）HALCON 在 S09 途中又讀不到 dongle（錯誤 2300，第四次；2026-10-12 恢復）：回歸閘門要把「有跳過的」當成「沒有驗證」，不是通過（`test.ps1` 已經列出）。（6）**2026-10-12 HALCON 補測**：`perf_ab` 的 HALCON 那一邊在低對比（除了樣本一個都沒找到）、重模糊（多 812 個候選，242 個誤判；use case 130–189 ms、峰值多約 80 MB）、雜訊 12＋模糊 2（位置 p95 0.933 px）超過 T6a／T6b（ADR-004 §5 的 B1–B3，**怎麼處理等使用者決定**）。效能矩陣要把召回率與誤判一起看：重模糊時 HALCON 的時間變長，推測是多出來的候選讓晶格與驗證多做了工作（**沒有分開量過**，`perf_ab` 只量 use case 端到端）。（7）**使用者 2026-10-12 決定**：B1–B3 在 S10 **不修**；S10 開工前，`tst_contract_end_to_end` 已經加了這三種板子（2026-10-12 加完：B1、B2 用 `perf_ab` 的 5 MP 板子、B3 用原本的板子；`evaluate()` 多了「重複」的計數），HALCON 的三列與 **OpenCV 的 B1 列（O1：角度最大 1.2°，加測試時發現）** 是預期失敗（`QEXPECT_FAIL`，log 是 XFAIL；`test.ps1` 最後已經列出「有預期失敗項目的測試」與列名）——回歸閘門要把 XFAIL 當成「已知、待修」列出來，不是通過也不是失敗；修好時會變成 XPASS（失敗）。S10 的效能矩陣量這幾種板子時，HALCON 的 B1（沒有晶格、很快）與 B2（多 812 個候選、慢 3 倍）的時間不能當基準。**S10 收工時 `spawn_task` 開的是 HALCON 準確度的修正**（Start prompt 在 `docs/plan/S11-optimize.md` 開頭），不是 S11；修正的 session 收工時再開 S11。（5）S07、S08 的真實影像已不在 `D:\Benano\20260922_*`（`perf_*_real` 要 `AOI_REAL_IMAGE_DIR`）；新的影像在 `D:\Benano\20261007_4677_AdvancePin_Proj\`，能不能用要使用者確認。

<!--STE-->
## Purpose
This step builds the full performance test. It measures the search time, the screen update time and the interaction time.

## Reason
Earlier steps measured single items. We now need one repeatable test that covers the whole program and catches a slow change.

## Effect
One command gives a report with numbers. A change that is slower than the budget fails the test.
<!--/STE-->

**繁中**：把前面各步驟零散的量測整合成一套可重複執行的完整測試，慢了就失敗。實作 `docs/perf-test-plan.md` 的全部內容。

## 任務

1. **`aoi_perf` 執行檔**（搜尋矩陣）：影像大小 × 樣本大小 × 旋轉範圍 × 元件數 × 難度 × back end。每格熱機 5 次、量 30 次，輸出中位數／p95／峰值記憶體／各階段切片／準確度。
2. **UI 效能測試**（`offscreen` 與真機視窗兩種模式）：U1–U4。
3. **互動效能測試**：用 `QTest` 注入滾輪、拖曳、按鍵事件；每幀計時；事件迴圈看門狗（I3）；取消延遲（I4）；復原（I6）；I1、I2、I5。
4. **端到端 T5**：放開滑鼠 → 提案就緒（5 MP、200 個元件）。
5. **輸出**：`perf/results/<date>-<commit>-<machine>.json`（含機器指紋）；`tools/perf-compare.mjs` 對照 baseline 與預算；退出碼可供 CI 使用。
6. **互動式 HTML 報表**：由結果 JSON 產生；顯示預算線、通過/失敗、各階段堆疊圖。
7. **建立 baseline**（`perf/baseline/<machine-id>.json`，進版控）；把 `perf/budgets.json` 的所有已量測列標為已校準。
8. **穩定性**：量測時固定電源模式、關閉不必要程式的建議清單；記錄量測當下的 CPU 頻率/負載以判斷雜訊。

## SOLID 重點

harness 只透過 port 與 `PerfProbe` 取得資料，不碰 adapter 內部（D）；新增指標＝新增一個 case 類別並註冊（O）。

## 效能檢查（本步驟自己的）

harness 自身開銷（空跑一個 case 的額外時間）；同一份程式跑兩次的結果差異（量測雜訊，應遠小於 15% 閾值，否則閘門會誤報）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 測試方法的 Purpose/Reason/Effect；如何解讀報表 |
| 2 | diagram.md | harness 結構圖；結果 JSON schema；回歸閘門決策流程 |
| 3 | explainer.html | 即**互動式效能報表**：篩選 back end／影像大小，看預算線與實測 |
| 4 | storyboard.md | 60 秒「我們怎麼證明程式夠快」 |

## 完成定義

- [ ] 一個指令跑完整套並產生報表
- [ ] 故意放慢某處（例如加 sleep）時，閘門確實失敗
- [ ] 量測雜訊已量化並低於閾值
- [ ] baseline 已建立
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
test(perf): add performance harness, regression gate and interactive report

- Search matrix, UI update and interaction tests (T1-T6, U1-U4, I1-I6)
- JSON results with machine fingerprint; perf-compare gate against baseline and budgets
- Baseline recorded; budgets marked calibrated
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S10-perf-harness.md 與 docs/perf-test-plan.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S10（分支 step/10-perf-harness）。只做 S10。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S11 開新 session。
```
