# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Sonnet 5.5)
- 日期：2026-10-05

## 正在做

無。**S02（Qt UI 骨架與影像畫布）已完成**：分支 `step/02-ui-shell`，以 `--no-ff` 合併到 `main`，tag `step-02-done`。全部只在本機，**沒有 push**。S03 由新的 session 做。
**有一件事等使用者決定：[ADR-009](../docs/adr/009-ui-toolkit-review-after-s02.md)**（I1 在預算邊緣、真實視窗超標，要不要重審 UI 工具集）。在決定之前 ADR-001 照舊有效，S03 不受影響。

## 已完成（S02）

- 建置：`CMakeLists.txt`、`CMakePresets.json`（debug／release／relwithdebinfo，`/W4 /WX`）、`cmake/AoiTargets.cmake`、`scripts/dev/env.ps1`（進入開發環境）、`test.ps1`。
- 程式：`src/domain/image_view.h`、`src/presentation/{canvas,tools,shell,theme,i18n,dev}`、`src/app/main.cpp`。畫布、批次繪製層、四個工具、訊息列（固定高度）、tokens → QPalette／QSS、文案直接讀 `copy.json`、`FakeScene`。
- 測試：`tests/ui/` 5 個 Qt Test 單元測試執行檔（Release、Debug 都通過）、`perf_ui_baseline`、`perf_paint_micro`（label `perf`）。
- 文件：`docs/steps/S02-ui-shell/`（四種輸出＋`perf.md`）、ADR-007、008、009、ADR-001 補充、`architecture.md` 模組表、`perf/baseline/casparhsu1.json`、`perf/budgets.json` 的校準狀態。

## 使用者要決定（ADR-009）

1. 維持 Widgets，S11 試 `QOpenGLWidget` 視埠與「縮放時快取」？（我的建議）還是現在換方向？
2. I1 預算要不要註明視埠大小與平台（1000×700 在邊緣，1600×900 與真實視窗超標）？
3. 使用者平常的螢幕解析度與縮放習慣（決定哪個視埠才是代表情境）。

## 下一步

**S03：核心（ports、use case、DI、執行緒、PerfProbe）**（`docs/plan/S03-core.md`）。S02 提前放了最小版 `ImageView`（`src/domain/image_view.h`），S03 可以擴充欄位，欄位意義不要改。`tools/check-deps.mjs` 在 S03 建立；掃 `#include` 時要讓 `tests/` 與 `src/presentation/dev/` 不被當成五層之一。

## 不要動

- `ai-project-template/`：與產品無關的 AI 結構範本，已納入版控；除非使用者要求，不要修改。
- `docs/ux/tokens.json`、`copy.json` 是畫面唯一的來源（內嵌進執行檔）；改了之後要重建，且跑 `node tools/check-contrast.mjs`、`tst_theme_i18n`。
- `perf/budgets.json` 的 limit 沒有改（使用者在 S01 確認過）；不要為了讓測試過而放寬。

## 已知問題與風險

- **I1**：offscreen 13 次執行 p95 在 14.1–20.7 ms（7 次 ≤ 16.7），真實視窗 22.1 ms；同一份程式在不同時間量會飄 ±30%，原因未查明。回歸閘門（S10）不能拿單次 p95 比預算。
- **建置環境（繁中 Windows）**：`cl` 的 `/showIncludes` 前綴是中文，主控台字碼頁與 configure 時不同時，Ninja 掉標頭相依（只改標頭檔不重新編譯）。一定要在 `env.ps1` 之後 configure 與 build；症狀出現時刪掉 `build/<preset>` 重來。
- **Bash heredoc 會壓縮連續反斜線**（專案規則早已寫明）：用 Bash 寫含 `\\` 的檔案內容時會被破壞；寫檔用 Write／Edit 工具，或用 `String.fromCharCode(92)` 組出反斜線。
- **HALCON 只有 runtime，沒有 `include`／`lib`**：S08 前要補裝開發元件。OpenCV 4.9.0 只確認 `find_package` 找得到（`OpenCV_DIR` 要指到 `build\x64\vc16\lib`），沒有連結過。
- Qt Test 在這台機器不寫 stdout，要用 `-o 檔案,txt`；offscreen 要設 `QT_QPA_FONTDIR`；Qt 6.8 的 `LanguageChange` 是排入佇列的。
- 暫代品：`ShellController`（S04 取代）、`ImageLoader`（S06 取代）、`ui.shell.*` 文案（S04 移除）。高 DPI 沒有在真正的高 DPI 螢幕上看過；字級、主題、語言不會記住。
- Codex 端的 AGENTS.md 流程沒有在本機實測過（承接自 S01）。

## 驗證（都已實際執行）

- `scripts/dev/test.ps1`：Release 與 Debug 各 5 個測試執行檔全過（Release：17＋19＋25＋12＋20 項通過，含每個檔案的 init／cleanup；1 項略過＝需要環境變數的截圖）。
- 變異測試：拿掉縮放錨點補償、拿掉 LOD 門檻、拿掉框的夾限，對應測試都會失敗。
- `node tools/ste-lint.mjs --strict`：434 句 0 錯誤；`check-contrast`：全過；`diagram.md` 4 張 Mermaid 語法全過；`check-ai-structure.ps1`：OK。
- 效能：見 `docs/steps/S02-ui-shell/perf.md`（offscreen 全套＋真實視窗子集）。U2、U3、I3、I4、I6 未量測（S04）。
- `-DAOI_WITH_HALCON=ON` 在缺標頭時明確失敗；`-DAOI_WITH_OPENCV=ON` 找到 4.9.0（只 configure）。
