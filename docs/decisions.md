# 設計決策索引（ADR）

> 本專案一檔一決策：每個決策是 `docs/adr/NNN-slug.md`，本檔只是索引，新的放最上方。內容只寫在 ADR 檔，不要在這裡複製。
> 已記錄的決定，沒有新證據不推翻；要推翻就新增一筆，並在新舊兩筆都標明取代關係。

預定的 ADR（003–005）列在各步驟的 plan 檔（`docs/plan/Sxx-*.md`），寫成後再加進下表。S02 新增 007、008、009（編號 003–005 留給 S03 之後的步驟）。

| 編號 | 標題 | 狀態 | 日期 |
|---|---|---|---|
| [ADR-009](adr/009-ui-toolkit-review-after-s02.md) | S02 實測後重審 UI 工具集：I1 在預算邊緣、真實視窗超標；選項 A 維持、B OpenGL 視埠、C QML、D 調預算 | **提案（待使用者決定）**；決定前 ADR-001 照舊有效 | 2026-10-05 |
| [ADR-008](adr/008-message-bar-fixed-height.md) | 訊息列固定高度（畫布大小不因訊息而變；放不下就在列內捲動） | 採用 | 2026-10-05 |
| [ADR-007](adr/007-copy-json-as-translation-source.md) | 文案：`copy.json` 直接當翻譯來源（`JsonTranslator`＋`qtTrId`，不產生 `.ts`／`.qm`；佔位字維持具名） | 採用 | 2026-10-05 |
| [ADR-006](adr/006-sample-library-and-hierarchy.md) | 樣本庫與 Board › Component › Box 階層（流程 A 教樣本、流程 B 區域掃描、元件類型編輯） | 採用（核心方向、單位 µm、庫與板子是複本關係已由使用者決定；其餘未決見 ADR） | 2026-10-05 |
| [ADR-002](adr/002-undo-model.md) | 復原模型：Command ＋ 三個範圍（提案、類型編輯、文件） | 採用 | 2026-10-05 |
| [ADR-001](adr/001-ui-toolkit.md) | UI 工具集：Qt Widgets + QGraphicsView | 採用 | 2026-10-05 |
| [ADR-000](adr/000-ai-agent-structure.md) | AI agent 共用結構（Codex 與 Claude Code） | 採用 | 2026-10-05 |
