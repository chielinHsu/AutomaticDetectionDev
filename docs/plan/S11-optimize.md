# S11 — 效能優化（先量再改）

| 項目 | 內容 |
|---|---|
| 層級 | 驗證 |
| 分支 | `step/11-optimize` |
| 依賴 | S10 |
| 產出位置 | 依需要修改；`docs/steps/S11-optimize/` |

> **S04 完成後的提醒（2026-10-06）**：S04 留下一個已知超標的預算：**U3**（分數滑桿到重繪，預算 16 ms；5 832 個元件 p95 46–61 ms，504 個元件的 p95 也常超過）。拆解在 `docs/steps/S04-presenter/perf.md`：整個畫布重繪 19–30 ms、場景重建＋畫布更新 9–15 ms、重新過濾不到 1 ms。先有 before 數字再動手。候選（都是推測，沒有量測）：只重畫狀態改變的框、把快照拆成幾何與狀態、把背景已接受的元件快取成一層。U3 的預算要不要重新定義是使用者的決定（S01 確認過現在的數字）。做完要拿掉 `tests/ui/perf_ui_s04.cpp` 的 `knownOver("U3")`。U3 與 S02 的 I1（ADR-009，待使用者決定）看起來是同一個瓶頸（整個畫布的繪製），OpenGL 視埠的試驗可以一起量。

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
