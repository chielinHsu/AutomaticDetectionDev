# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Sonnet 5.5)
- 日期：2026-10-05

## 正在做

無。**S03（系統核心）已完成**：分支 `step/03-core`，以 `--no-ff` 合併到 `main`，tag `step-03-done`。全部只在本機，**沒有 push**。S04 由新的 session 做。
**有一件事仍等使用者決定：[ADR-009](../docs/adr/009-ui-toolkit-review-after-s02.md)**（I1 在預算邊緣、真實視窗超標，要不要重審 UI 工具集）。決定之前 ADR-001 照舊有效，S04 不受影響。

## 已完成（S03）

- domain：`geometry.h`（座標與角度慣例，ADR-010）、`match.h`、`object_state.h`（含 `NoMatchReason`）、`search_params.h`、`component.h`（Board › Component › Box）、`recipe.h`；`ImageView` 新增 `umPerPixel`。
- application：`Result`／`Error`／`CancelToken`；九個 port（契約寫在標頭）；`JobRunner`（latest-wins）；`GenerateObjectsFromSample`、`classify`、暫代的 `SimpleGrid`；`BackendRegistry`。
- infrastructure：`backend_fake`（可重現的合成網格，不看影像）、`PerfProbe`、`ThreadPoolExecutor`。app：`CompositionRoot`、`--backend=fake|opencv|halcon`。
- 工具與測試：`tools/check-deps.mjs`（CTest，13 個 fixture）；`tests/core/`（8 個單元測試執行檔、`tst_cli`、`perf_core`）；ADR-003、ADR-010；`docs/steps/S03-core/`（四種輸出＋`perf.md`）；`perf/baseline/casparhsu1-core.json`；`AiInspTool.pro` 同步。

## 下一步

**S04：Presenter、互動狀態機、復原／重做**（`docs/plan/S04-presenter.md`，開頭有「S03 完成後的提醒」）。要用的東西：`composeCore`／`CoreServices`（`main.cpp` 已組裝但還沒交給畫面）、`JobRunner`、`classify`、`IExecutor`（Qt 的 `QThreadPool` 版是 S04 的工作）。`JobReport::latest` 在 worker 上算，Presenter 轉回 UI 執行緒後要再檢查。

## 不要動

- `ai-project-template/`：與產品無關的 AI 結構範本；除非使用者要求，不要修改。
- `docs/ux/tokens.json`、`copy.json`：畫面唯一的來源；改了要重建並跑 `check-contrast`、`tst_theme_i18n`。
- `perf/budgets.json` 的 limit 沒有改，S03 也沒有新增預算；不要為了讓測試過而放寬。
- `tests/core/fixtures/deps/` 的 expect.json 是依賴檢查的標準答案；改規則表（`tools/check-deps.mjs` 的 `RULES`）要同步改 `docs/architecture.md` 的依賴表與 fixture。
- port 的契約（可重入、不丟例外、取消協作式、排序與範圍）寫在標頭；要改就是改 S07／S08／S09 的前提，先看 `docs/steps/S03-core/spec.md`。

## 已知問題與風險

- **`SimpleGrid` 是暫代品**：約 4 900 點要 97–134 ms（T3 目標 1 000 點 ≤ 2 ms，屬 S05）；已知限制（間距差 20 倍被當成一排、只補索引範圍內的缺格）由 `tst_grid` 記錄。
- **ADR-010 的 OpenCV／HALCON 轉換未驗證**（像素中心偏移、角度正負號、HALCON 的軸順序與弧度）：S07、S08 實測，S09 鎖住。
- 取消延遲的上限是工作兩次看旗標之間的時間；每 1 ms 看一次的工作，量到 1.4–8.7 ms（中位數），由 Windows 計時器解析度決定。
- 假 back end 不看影像：任意影像上框樣本都排出貼著樣本的網格。流程 B 只有 port 與型別。
- **`AiInspTool.pro`** 已納入新的層，只驗證命令列 qmake ＋ jom 建置，**沒有在 Qt Creator 裡載入過**；storage、OpenCV、HALCON 之後要再調整。未追蹤的 `AiInspTool/release/`、`.qtc_clangd/` 是 Qt Creator 的建置產物（在使用者的主工作目錄，我沒有動）。
- **I1**（承接 S02）：offscreen p95 在 14.1–20.7 ms 飄、真實視窗 22.1 ms；同一份程式不同時間量會飄 ±30%，原因未查明。回歸閘門（S10）不能拿單次 p95 比預算。
- **建置環境（繁中 Windows）**：一定要在 `env.ps1` 之後 configure 與 build（字碼頁與 Ninja 標頭相依）；症狀（亂碼洪水、改標頭沒反應）出現時刪掉 `build/<preset>` 重來。
- **Bash heredoc 與 `node -e` 會壓縮或吃掉反斜線**（專案規則早已寫明；這個 session 又踩到兩次）：含反斜線或中文字串的檔案內容用 Write／Edit 工具寫，不要用 shell 拼。PowerShell 5.1 讀 `.ps1` 不認 UTF-8（無 BOM），腳本裡不要放中文字串。
- Qt Test 在這台機器不寫 stdout，要用 `-o 檔案`；`QVERIFY`／`QCOMPARE` 不要放在 worker 的 callback 裡（ADR-003）。
- 暫代品：`ShellController`（S04 取代）、`ImageLoader`（S06 取代）、`ui.shell.*` 文案（S04 移除）。Codex 端的 AGENTS.md 流程沒有在本機實測過。

## 驗證（都已實際執行）

- `scripts/dev/test.ps1`：Release 與 Debug 各 17 個 CTest 項目全過（核心 Qt Test 共 155 項含 init／cleanup；UI 沿用 S02）。
- 變異測試 14 個全被抓到（`docs/steps/S03-core/perf.md`）；`node tools/check-deps.mjs`：80 個檔案 0 違規；`--self-test`：13 個 fixture 全過。
- `node tools/ste-lint.mjs --strict`：488 句 0 錯誤；`diagram.md` 10 張 Mermaid 解析全過（只驗語法）；`check-ai-structure.ps1`：OK。
- 效能：`perf_core` 連跑 3 次，結果在 `docs/steps/S03-core/perf.md`；`aoi.exe --backend=opencv|halcon|turbo` 結束碼 2 並有訊息（`tst_cli`）。
