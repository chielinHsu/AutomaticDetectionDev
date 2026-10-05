# S10 — 效能測試 harness 與回歸閘門

| 項目 | 內容 |
|---|---|
| 層級 | 驗證 |
| 分支 | `step/10-perf-harness` |
| 依賴 | S02–S09 |
| 產出位置 | `tests/perf/`、`tools/perf-compare.mjs`、`perf/baseline/`、`docs/steps/S10-perf-harness/` |

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
