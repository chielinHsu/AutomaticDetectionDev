# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Sonnet 5.5)
- 日期：2026-10-05

## 正在做

無。**S01（UX 規格與可點擊原型）已完成**：commit 在分支 `step/01-ux-spec`，以 `--no-ff` 合併到 `main`，tag `step-01-done`。全部只在本機，**沒有 push**。S02 由新的 session 做。

## 已完成（S01）

- 設計：`docs/ux/ux-spec.md`、`tokens.json`、`copy.json`；`docs/adr/001`（Widgets + QGraphicsView）、`002`（復原模型）、`006`（樣本庫與 Board › Component › Box 階層），三者都「採用」。四種輸出與 `perf.md` 在 `docs/steps/S01-ux-spec/`。
- 原型 `explainer.html`（單一 HTML，離線可開）：教樣本、區域掃描與比對範圍、物件樹、樣本庫（複本語意）、類型編輯器（矩陣、相依窗）、CAD 清單。
- 工具：`tools/check-contrast.mjs`；後續步驟的提醒在 `docs/plan/S02`–`S10`、`S12` 開頭「S01 修訂」。

## 使用者已決定（2026-10-05）

Widgets + QGraphicsView；UI 預算 U/I 照原數字＋三點定義；兩種流程並存；庫與板子是**複本**；庫存 **µm**＋影像校正值；疊圖固定亮色；接受／捨棄後 `Ctrl+Z` 回 Preview；`Esc` 捨棄不確認；手動補的元件標示「手動」；訊息不蓋在影像上。

## 下一步

**S02：Qt UI 骨架與影像畫布**（`docs/plan/S02-ui-shell.md`）。第一件事是驗證 MSVC 2022／Qt 6.8／CMake 工具鏈，並把結果寫回 `AI_CONTEXT.md` 的環境表。開工前讀 `docs/ux/ux-spec.md`（版面、工具、狀態語言、訊息列）與 ADR-001／006 的「後果」。

## 不要動

- `ai-project-template/`：與產品無關的 AI 結構範本，已納入版控；除非使用者要求，不要修改。
- 其他步驟的 plan 檔只加了「S01 修訂」提醒，內容沒改。

## 已知問題與風險

- 原型的比對是**假的**（用板子上零件的真值），耗時是公式模擬；真實的 `ILibraryMatcher` 在 S07／S08，成本隨比對的樣本數成長（假設，未實測）。庫很大時的做法見 ADR-006 §7，除了「先比板子類型」外都是未量測的選項。
- 原型用像素；樣本庫的 µm 是決定，還沒有實作。
- 疊圖的綠色 Found 對綠板只有 3.14:1（剛好過線）；板子顏色是合成的參考色，不是實測。
- 真實滑鼠／鍵盤／滾輪的抽查只在第一版原型做過；新版是合成事件（整套 124 項，另有比對範圍約 20 項）加截圖目視。
- ADR-006 未決：實例覆寫、Panel 多連板、角度正負號、窗的演算法與參數、自由公式、庫的 System／Project 層。
- Codex 端的 AGENTS.md 流程沒有在本機實測過。

## 驗證（都已實際執行）

- `node tools/ste-lint.mjs --strict`：386 句，0 錯誤。
- `node tools/check-contrast.mjs`：淺 18、深 18、疊圖 24 組全過。
- `diagram.md` 的 12 張 Mermaid：mermaid 11 解析器全過（只驗語法）。
- 建置、單元測試、`check-deps`：不適用（尚未建立）。效能：全部未量測。
