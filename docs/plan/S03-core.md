# S03 — 系統核心：Ports、Use Case、DI、執行緒、PerfProbe

| 項目 | 內容 |
|---|---|
| 層級 | 系統架構 |
| 分支 | `step/03-core` |
| 依賴 | S02（建置系統） |
| 產出位置 | `src/domain/`、`src/application/`、`src/app/`、`src/infrastructure/perf/`、`tools/check-deps.mjs`、`tests/core/` |

<!--STE-->
## Purpose
This step builds the core of the program without a screen. It defines the ports, the use case and the job runner.

## Reason
The core must not know the screen or the vision library. Then we can test it alone and swap its parts.

## Effect
We run the whole use case with a fake searcher in a test. The real back ends plug in later with no change to the core.
<!--/STE-->

**繁中**：核心不認識畫面也不認識視覺函式庫，所以能單獨測試、可抽換。用假的 searcher 就能跑完整個 use case；之後接上真的 back end 不必改核心。

## 任務

1. **Domain 型別**：`ImageView`（不擁有資料的 view＋共享擁有者）、`Pose2D`、`RectObj`、`Match`、`ObjectState{Sample,Found,Inferred,Rejected}`、`InspectionObject`、`SearchParams`。
   **座標慣例要寫成文件**：像素原點、x 向右 y 向下、角度單位與正負方向。OpenCV 與 HALCON 的慣例可能不同，由 adapter 轉換（S06、S08 驗證，S09 用測試鎖住）。
2. **Ports**（每個 ≤ 5 個方法）：`ITemplateBuilder`、`IPatternSearcher`（支援「整張影像」與「一組視窗」兩種搜尋區域）、`IImageSource`、`IRecipeStore`、`IProgressSink`、`IPerfProbe`、`IExecutor`。
3. **錯誤模型**：`Result<T, Error>`＋錯誤碼；**不讓例外穿過 port**。
4. **`CancelToken`**（原子旗標）與 **`JobRunner`**（latest-wins：新工作提交時取消舊工作）。
5. **Use case `GenerateObjectsFromSample`**：流程＝建範本 → 種子搜尋 → 擬合 → 預測 → 視窗驗證 → 產生物件。先用 `FakeTemplateBuilder`／`FakeSearcher`（可重現的合成網格）；擬合先放簡化版，真正的在 S05。
6. **`BackendRegistry`＋工廠＋組裝根**（`app/`）：`--backend=fake|opencv|halcon`；沒有授權/未編入的 back end 回傳明確錯誤。
7. **`PerfProbe`**：`ScopedTimer`、環狀緩衝、輸出 JSON；關閉時成本接近零。
8. **`tools/check-deps.mjs`**：掃 `#include`，依 `docs/architecture.md` 的依賴表檢查，掛進 CTest；**故意寫一個違規 include 的測試案例，確認會失敗**。
9. **ADR-003**：測試框架統一用 Qt Test（涵蓋 domain 到 UI，也有 `QBENCHMARK`）。

## SOLID 重點

這是 SOLID 的主戰場：O（新 back end 不改 use case）、I（窄介面）、D（use case 只依賴 port）。每項在 `spec.md` 的表格寫出具體類別與驗證方式。

## 效能檢查

- `PerfProbe` 開／關的單次呼叫成本（目標關閉時 < 1 µs，**實測再下結論**）。
- 工作提交 → 開始執行的延遲；取消旗標生效延遲。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 核心的 Purpose/Reason/Effect；SOLID 對照表 |
| 2 | diagram.md | Ports/Use case 類別圖；工作生命週期序列圖（提交、取代、取消、完成） |
| 3 | explainer.html | 互動頁：「換 back end」——切換 Fake／OpenCV／HALCON，use case 區塊維持不動；再按「偷 include」按鈕，看依賴檢查器報錯 |
| 4 | storyboard.md | 60 秒「為什麼換 back end 不用改核心」 |

## 完成定義

- [ ] 用 `FakeSearcher` 的端到端 use case 測試通過
- [ ] 取消、latest-wins 的測試通過
- [ ] `check-deps` 通過，且違規案例會失敗
- [ ] `PerfProbe` 開/關成本已量測
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(core): add ports, use case skeleton, job runner, perf probe and dependency check

- Domain types and documented coordinate conventions
- Ports (template builder, searcher, image source, recipe store, progress, perf, executor)
- GenerateObjectsFromSample with fake back end, latest-wins JobRunner, CancelToken
- BackendRegistry and composition root, tools/check-deps.mjs wired into CTest
- ADR-003: Qt Test as the single test framework
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S03-core.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S03（分支 step/03-core）。只做 S03。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S04 開新 session。
```
