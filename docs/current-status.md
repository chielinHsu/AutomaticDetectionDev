# 目前狀態（進度表）

新 session 開工前先讀這份，再讀 `docs/plan/Sxx-*.md`。步驟完成後由該步驟的 session 更新。原檔名 `STATUS.md`，2026-10-05 更名為本檔以符合共用結構（見 `docs/adr/000-ai-agent-structure.md`）。

- 更新日期：2026-10-05
- 階段：S00、S01 完成；S02 待開始。S01 已用 `--no-ff` 合併到 `main` 並打 tag `step-01-done`；AI 共用結構與 commit 格式規則也已合併到 `main`（2026-10-05，全部只在本機，**沒有 push**）

| 步驟 | 層級 | 主題 | 狀態 | 分支 | tag |
|---|---|---|---|---|---|
| S00 | 基礎 | 版控 + 總體計畫 + 架構導覽 | ✅ 完成 | `step/00-master-plan` | `step-00-done` |
| S01 | 介面 | UX 規格與可點擊原型（含樣本庫與 Board › Component › Box 階層） | ✅ 完成 | `step/01-ux-spec` | `step-01-done` |
| S02 | 介面 | Qt UI 骨架與影像畫布 | ⏳ 下一個 | `step/02-ui-shell` | |
| S03 | 系統 | 核心：ports、use case、DI、執行緒、PerfProbe | ⬜ | `step/03-core` | |
| S04 | 系統 | Presenter、互動狀態機、復原/重做 | ⬜ | `step/04-presenter` | |
| S05 | 底層 | Domain：晶格估計與補漏（純 C++） | ⬜ | `step/05-lattice` | |
| S06 | 底層 | 影像緩衝、轉接器、Recipe 儲存 | ⬜ | `step/06-image-storage` | |
| S07 | 底層 | OpenCV back end | ⬜ | `step/07-opencv` | |
| S08 | 底層 | HALCON back end | ⬜ | `step/08-halcon` | |
| S09 | 底層 | 契約測試與 A/B 比較 | ⬜ | `step/09-contract-ab` | |
| S10 | 驗證 | 效能測試 harness 與回歸閘門 | ⬜ | `step/10-perf-harness` | |
| S11 | 驗證 | 效能優化（先量再改） | ⬜ | `step/11-optimize` | |
| S12 | 發行 | 匯出、打包、最終文件 | ⬜ | `step/12-release` | |

圖例：✅ 完成　⏳ 下一個　⬜ 尚未開始　⚠️ 有未解決問題

## S01 的產出與使用者決定（2026-10-05）

- 設計：`docs/ux/ux-spec.md`、`tokens.json`、`copy.json`；ADR-001（Widgets + QGraphicsView）、ADR-002（復原模型）、ADR-006（樣本庫與階層）全部「採用」。四種輸出與 `perf.md` 在 `docs/steps/S01-ux-spec/`。
- **範圍在 S01 中擴大**：使用者看原型後提出第二個流程（框任意區域 → 與樣本庫比對 → 列出對應的 CAD 框），決定兩種流程並存，並提供三份 TRI 參考文件。模型變成 Board › Component › Box，框由「元件類型」推導，見 ADR-006。後續步驟的影響已用一行提醒寫在 `docs/plan/S02`–`S10`、`S12` 開頭。
- 使用者決定：UI 預算（U/I）照原數字＋三點定義；兩種流程並存；庫與板子是**複本**（TRI 的 Merge）；庫存 **µm**＋影像校正值；疊圖固定亮色；接受／捨棄後 `Ctrl+Z` 回 Preview；`Esc` 捨棄不確認；手動補的元件標示「手動」；訊息不蓋在影像上。
- 驗證：`ste-lint --strict` 386 句 0 錯誤；`check-contrast` 淺 18、深 18、疊圖 24 組全過；`diagram.md` 12 張 Mermaid 語法全過（只驗語法）；原型頁內腳本整套 124 項斷言全過（另有比對範圍約 20 項）。**沒有建置、沒有 Qt 程式、沒有任何效能量測**（`perf.md` 全部「未量測」）。

## 預算校準狀態

`perf/budgets.json` 目前 `status: "proposed"`。S02 與 S07 完成實測後改為 `calibrated`。

## 未解決事項

- ~~UI 工具集（Widgets vs QML）~~ — 已由 S01 決定：Widgets + QGraphicsView（ADR-001）。重審條件：S02 量測 I1（5 000 框）超標且瓶頸在繪製。
- ADR-006 未決：實例覆寫（只改這一個元件）、Panel 多連板、角度正負號、窗的演算法與參數、自由公式、庫的 System／Project 層。新預算 T7（區域掃描隨樣本庫大小 N）等 S07 實測後再提。
- 原型的比對是假的；真實的 `ILibraryMatcher` 在 S07／S08。HALCON「每個條目一個 shape model」是預期做法，未驗證。
- MSVC 2022 工具鏈是否可用 — S02 第一件事驗證
- 真實板子影像（去識別化 1–2 張）— S07 前請使用者提供
- AI 共用結構（Codex 與 Claude Code）已於 2026-10-05 套用，commit `fd4fd2a` 並以 `--no-ff` 合併到 `main`，**沒有 push**。S01 的 Start prompt 已改成引用 `AGENTS.md` 與 `docs/current-status.md`，這些檔案都在 `main` 上。變更含：新增 `AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/`、`scripts/ai/`、`docs/adr/000-*`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`；改寫 `CLAUDE.md`；`docs/STATUS.md` 更名為本檔；14 個檔案的引用更新。stage 時一律明確列出路徑，不要 `git add -A`。
- `ai-project-template/`：AI 結構範本，與產品程式碼無關。2026-10-05 由 Claude Code 在這個 session，依使用者「設計共用結構」的要求產生，並依使用者決定納入版控、放在這個 repo。若日後要搬離，同步更新 `AI_CONTEXT.md` 等引用它的文件。
- Codex 端的 AGENTS.md 流程沒有在本機端到端測過（該機器的 Codex CLI 無法執行 `codex exec`）。
