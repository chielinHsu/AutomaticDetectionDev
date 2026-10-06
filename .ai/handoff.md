# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-06

## 正在做

無。**S05（Domain：晶格估計與補漏）已完成**：分支 `step/05-lattice`，以 `--no-ff` 合併到 `main`，tag `step-05-done`。全部只在本機，**沒有 push**。S06 由新的 session 做。
**仍有兩件事等使用者決定**（S02、S04 留下的）：（1）[ADR-009](../docs/adr/009-ui-toolkit-review-after-s02.md)（I1 在預算邊緣）；（2）U3 超出預算（`knownOver("U3")`）。S05 新增一個：**T3 的預算沒有說是哪一種 1 000 點**（乾淨的陣列 p95 0.65–1.19 ms 通過；散佈的點 p95 3.1–4.2 ms 不在預算裡）。

## 已完成（S05）

- `src/domain/lattice/`（只用標準庫；`aoi_domain` 由 INTERFACE 改 STATIC）：`LatticeEstimator`（`fit`、`fitAll`）、`LatticePredictor`、`LatticeAccumulator`（O(1) 正規方程）、`NonMaxSuppression`、`SpatialGrid`、`IVectorClustering`；內部：`basis_finder`、`lattice_acceptance`、`cell_components`、`lattice_detail.h`。接受規則見 ADR-013（寧可沒有晶格，也不要錯的晶格）。
- use case `GenerateObjectsFromSample`：`SimpleGrid` 刪除；種子先 NMS；缺格與已有候選重疊不補；由近到遠、上限內留近的；驗證找到的用 O(1) 重新擬合。`presentation/` 沒改。
- 測試：`tests/domain/`（7 個執行檔＋`s00_demo_fixture_is_current`；`tst_grid` 搬進 `tst_lattice`）、`tst_generate_objects` 新增 6 個 S05 測試；32 個變異最終全被抓到。
- 文件：`docs/steps/S05-lattice/`（四種輸出＋`perf.md`）、ADR-013、`perf/baseline/casparhsu1-lattice.json`、`perf/budgets.json`（T3 `calibrated`，limit 沒改）；`docs/plan/S06`、`S07`、`S10`、`S11` 開頭有「S05 完成後的提醒」。
- 改了 S04 的 `tests/ui/tst_presenter.cpp` 兩個斷言（產品程式沒動）：原本在進入預覽的瞬間讀狀態列，搜尋超過測試台的忙碌門檻（40 ms）時讀到「搜尋中…」。Debug 全套跑出一次失敗；60 ms 的搜尋可以穩定重現；改成 `QTRY_VERIFY2` 等最終狀態。

## 下一步

**S06：影像緩衝、轉接器、Recipe 儲存**（`docs/plan/S06-image-storage.md`，開頭有 S03–S05 的提醒）。domain 已有 .cpp：加 storage 時，`AiInspTool.pro` 照 `domain/*.cpp` 的方式加萬用字元。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`、`copy.json`（改了要跑 `check-contrast`、`tst_theme_i18n`）。
- `perf/budgets.json` 的 limit 一個都沒改；不要為了讓測試過而放寬。`knownOver("U3")` 等 S11。
- `tests/ui/PresenterRig.h` 的視窗閘門（`gateWindows`）故意不看取消旗標。
- 晶格的門檻（`LatticeOptions`）與 ADR-013 的規則：改之前跑 `tst_lattice_scenarios`（1 029 組）與 `tst_lattice_rules`；規則彼此備援，只看估計器的結果常看不出拿掉了一條。
- `tests/domain/fixtures/s00-demo-lattice.json` 由 `tools/s00-demo-fixture.mjs` 產生，不要手改；`explainer.html` 的 `lattice-data` 由 `tst_lattice_explainer` 匯出（`AOI_EXPORT_EXPLAINER=1`），不要手改。
- port 的契約、`ImageView`、`SearchParams`；`tools/check-deps.mjs` 的 `RULES` 與 `docs/architecture.md` 的依賴表要一起改。

## 已知問題與風險

- 晶格的門檻是合成資料定的起點，S07 用真實影像校準；**非矩形陣列（L 形、環形）沒有測**；缺件 ≥ 50% 時整組二維不成立（刻意的取捨）。
- 信心與「建議檢查」在 `GridSummary` 裡，畫面沒顯示；沒有任何一步的 plan 檔寫到。
- 這台機器同一份程式會飄 1.4–2 倍；T3 的餘裕 40–68%。S10 的閘門要用多次執行。`casparhsu1-core.json` 的 P4 是 S03 暫代版的數字，S05 的在 `casparhsu1-lattice.json` 的 `useCase`。
- 時間相依的 UI 測試：搜尋變慢（Debug、真實 back end）時，「進入預覽的瞬間」的畫面可能還有忙碌標示；讀狀態列要等最終狀態。
- 大檔案：`lattice_estimator.cpp` 432 行（spec 有說明）；`AutoRoiPresenter.cpp` 707 行（S04）。
- 變異測試腳本不在 repo；還原要寫回原始位元並驗證雜湊（會更新修改時間，Ninja 才會重建）。
- 建置環境：`env.ps1` 之後才 configure／build；含中文或引號的檔案用 Write／Edit 寫，不用 heredoc 或 `node -e`；`.ps1` 不放中文；Qt Test 要 `-o 檔案`。
- `AiInspTool.pro`：命令列 qmake ＋ jom 建置過，**沒有在 Qt Creator 裡載入過**。未追蹤的 `AiInspTool/release/`、`.qtc_clangd/` 在使用者的主工作目錄，沒有動。

## 驗證（都已實際執行）

- 見 `docs/changelog-ai.md` 本步驟那一筆（Release、Debug 各 36 個 CTest 項目、check-deps、ste-lint、Mermaid、效能 3 次、變異 32 個）。
