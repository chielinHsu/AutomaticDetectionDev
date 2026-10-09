# S11 — 效能優化（先量再改）

| 項目 | 內容 |
|---|---|
| 層級 | 驗證 |
| 分支 | `step/11-optimize` |
| 依賴 | S10 |
| 產出位置 | 依需要修改；`docs/steps/S11-optimize/` |

> **S05 完成後的提醒（2026-10-06）**：S05 已經做過一輪「先量再改」（`docs/steps/S05-lattice/perf.md`：T3 由 p95 3.3–3.6 ms 降到 0.65–1.19 ms，瓶頸是熱迴圈裡的 `hypot`／`lround`／`floor` 函式呼叫與連通性的鄰居檢查）。剩下的候選（都有數字，但沒有改）：NMS 每次比較都重算兩個框的角點（`cos`／`sin`），10 000 框中位數 8–10 ms，可以先算好；use case 視窗驗證的「匹配歸給哪個缺格」是 O(匹配數 × 缺格數)（約 4 900 個元件時，驗證階段 S03 量到 7–8 ms、S05 量到 2.2–4.5 ms，沒有拆開量）；散佈的點（沒有晶格）1 000 點 p95 3.1–4.2 ms，可以提早結束，但要先有「三排間距不同」那類情況的測試與數字。

> **S04 完成後的提醒（2026-10-06）**：S04 留下一個已知超標的預算：**U3**（分數滑桿到重繪，預算 16 ms；5 832 個元件 p95 46–61 ms，504 個元件的 p95 也常超過）。拆解在 `docs/steps/S04-presenter/perf.md`：整個畫布重繪 19–30 ms、場景重建＋畫布更新 9–15 ms、重新過濾不到 1 ms。先有 before 數字再動手。候選（都是推測，沒有量測）：只重畫狀態改變的框、把快照拆成幾何與狀態、把背景已接受的元件快取成一層。U3 的預算要不要重新定義是使用者的決定（S01 確認過現在的數字）。做完要拿掉 `tests/ui/perf_ui_s04.cpp` 的 `knownOver("U3")`。U3 與 S02 的 I1（ADR-009，待使用者決定）看起來是同一個瓶頸（整個畫布的繪製），OpenGL 視埠的試驗可以一起量。

> **S06 完成後的提醒（2026-10-06）**：有數字、沒有改的候選（`docs/steps/S06-image-storage/perf.md`）：（1）開圖在 UI 執行緒同步解碼（25 MP 的 PNG 約 0.5 秒、TIFF 約 0.6 秒）：可以改到背景解碼，但要先決定開圖期間的互動（狀態機沒有「開圖中」這個狀態）。（2）樣本庫存一筆要整份重寫（1 000 筆約 0.12–0.14 秒）：一個條目一個檔案可以改善，代價是 id 與「下一個號碼」的一致性。（3）12 bit 影像的顯示每次開圖轉一次 8 bit（一次複製，5 MP 約幾 ms）；16 bit 的影像畫布用 `Format_Grayscale16` 零複製，**繪製成本沒有量**。

> **S07 完成後的提醒（2026-10-06）**：OpenCV 搜尋的候選（數字在 `docs/steps/S07-opencv/perf.md`）：（1）頂層的 `matchTemplate` 約佔 5 MP 種子搜尋的一半；（2）頂層的候選約是元件數的 2 倍（`minScore − 0.2` 放進來的假峰，精修時淘汰）；（3）流程 B 每個類型約 9 ms、每次重建範本（ADR-006 §7.3 的快取沒有做）；（4）`Gray16` 每次搜尋都轉一次 8 bit（種子一次、視窗一次），24 MP 的真實影像種子搜尋約 170 ms；（5）S07 已經做過一次改寫（精修的小分數圖自己算：T2a 193 → 64 ms，ADR-016 的證據），before／after 的方法可以沿用。

> **S08 完成後的提醒（2026-10-07）**：HALCON 的候選（數字在 `docs/steps/S08-halcon/perf.md`）：（1）`create_shape_model` 約 10 ms（OpenCV 的範本 0.4 ms），流程 B 每個類型都要建一次：在樣本庫載入時快取模型（ADR-006 §7.3）對 HALCON 的效果比對 OpenCV 大；（2）`Gray16` 換 8 bit 每次搜尋一次（查表，5 MP 約 8 ms），`nativeGray16`（原生 `uint2`，零複製）一樣準、更快，是不是預設要和 S09 的 A/B 一起決定；（3）S08 已經做過一次「先量再改」：Gray16 的轉換由逐像素 `lround`（5 MP 213 ms）改成查表（約 8 ms）。（4）授權第一次檢查約 1.6 秒（HALCON 的初始化），在建立 back end 時做；如果要在背景先暖機，要先有數字與 UI 的決定。

> **建置改 qmake 之後的提醒（2026-10-07，[ADR-018](../adr/018-qmake-only-build.md)）**：建置只用 qmake（`AiInspTool/AiInspTool.pro`；CMake、preset、CTest 都已移除，S02–S08 文件裡的那些指令是歷史紀錄，對照表在 ADR-018），執行檔叫 `AiInspTool.exe`（GUI 子系統），在 `<建置>/bin`。OpenCV 與 HALCON **預設編進去**（以前的 preset `release-halcon`）；關掉用 qmake 變數 `AOI_WITH_OPENCV=OFF`／`AOI_WITH_HALCON=OFF`。命令列：`scripts/dev/test.ps1`（建置＋測試；`-Config Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`）；Qt Creator 開 `AiInspTool/AiInspTool.pro` 也能建置、執行與跑測試。**新增測試**：在 `AiInspTool/tests/<資料夾>/` 加一個 `.pro`（照同資料夾的寫法：`TARGET`、需要時 `AOI_LINK_EXTRA`、`include(<資料夾>_tests.pri)`、`SOURCES`），再列進 `AiInspTool/tests/tests.pro` 的 `AOI_TESTS`（效能量測列進 `AOI_PERF_TESTS`；只有某個 back end 才有的放進對應的 `equals(AOI_WITH_…, ON)` 區塊）。**新增原始碼**要加進對應的 `.pro`（不會自動納入）。S09 之前還有一個 session B（授權提醒的卡片、執行中切換 back end、Designer 的 `.ui`），開工時先看 `.ai/handoff.md`。before／after 用 `test.ps1 -Perf`（Release；`-NoBuild` 重跑不重建）。S02–S08 的基準線是 CMake 的建置量的；建置改 qmake 時在 qmake 的 Release 重量過一次全部效能量測（數字在 `docs/changelog-ai.md` 那一筆），旗標的最佳化相同（`-O2`、`NDEBUG`）。

> **S09 完成後的提醒（2026-10-09）**：A/B 比較視窗的 UI 已經量過也改過一次（先量再改：表格的 `ResizeToContents` 每填一格就重量整欄、標記每個描兩次邊；改完 AB-U1a p95 19.6–27.0 ms、AB-U1b 30.5–38.8 ms，見 `docs/steps/S09-contract-ab/perf.md` §3）。還沒改的：**AB-U3**（改門檻之後重新比較，p95 48.8–63.4 ms，其中 `compareProposals` 7–9 ms，其餘是每次重建 5 600 個標記與 1 000 列文字）——沒有預算，要不要做由 S11 依預算決定。HALCON 的 `nativeGray16`（12 bit 的種子搜尋快約 1.7 倍）要不要當預設，ADR-004 列為使用者決定。

<!--STE-->
## Purpose
This step makes the slowest parts faster. We change only the parts that the test shows as slow.

## Reason
A guess about speed is often wrong. A change that we do not measure can make the code harder to read and no faster.

## Effect
Each change has a before number and an after number. We keep a change only if the number improves.
<!--/STE-->

**繁中**：只改實測證明慢的地方，每個優化都要有 before／after 數字；沒進步就回復。

## 規則

1. **沒有 S10 的 before 數字，不准動。**
2. 每次只做一個優化、一個 commit，附 before／after。
3. 改善不到 5%、或讓程式明顯更難讀 → 回復。
4. 優化不得破壞契約測試（S09）與準確度（T6）。

## 候選清單（依 S10 報表排序後取前 3 名；以下只是可能性，不是必做）

| 領域 | 候選 | 預期要看什麼指標 |
|---|---|---|
| 搜尋 | 視窗大小與數量調校、金字塔快取、減少角度步數、提早終止（分數夠高就停）、OpenCV 的 SIMD 路徑 | T2、T4、T5 |
| 記憶體 | 避免暫存複製、重用緩衝、檢查 `ImageView` 計數 | 峰值記憶體 |
| 繪製 | 批次繪製、可見範圍裁切、縮放層級簡化、OpenGL/RHI viewport | U1、I1、I5 |
| 表格/模型 | 延遲載入、批次通知 | U2 |
| 執行緒 | worker 數與函式庫執行緒數的搭配 | T5、I3 |

## 任務

1. 依 S10 報表挑出各類別最差的項目（搜尋、UI 更新、互動各取最大差距者）。
2. 用 profiler（Visual Studio 效能分析器或 Qt Creator）＋ `PerfProbe` 切片確認熱點。
3. 逐一優化並記錄。
4. 重跑 S10，更新 baseline。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 每個優化的 Purpose/Reason/Effect（附數字） |
| 2 | diagram.md | 優化前後的管線/呼叫圖（標出省掉的複製與工作） |
| 3 | explainer.html | 互動「前後對照」：切換優化開關看指標變化（嵌入實測數字） |
| 4 | storyboard.md | 75 秒「我們怎麼讓它變快」 |

## 完成定義

- [ ] 每個優化都有 before／after，且有 commit
- [ ] 契約測試與準確度不退步
- [ ] baseline 已更新

## Commit 範本（每個優化各一個）

```
perf(<area>): <what changed>

before: <metric> <value>  after: <metric> <value>  (machine: <id>, N=30, median)
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S11-optimize.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S11（分支 step/11-optimize）。只做 S11。沒有 before 數字不要動手。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S12 開新 session。
```
