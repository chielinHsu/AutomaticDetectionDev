# 設計決策索引（ADR）

> 本專案一檔一決策：每個決策是 `docs/adr/NNN-slug.md`，本檔只是索引，新的放最上方。內容只寫在 ADR 檔，不要在這裡複製。
> 已記錄的決定，沒有新證據不推翻；要推翻就新增一筆，並在新舊兩筆都標明取代關係。

預定的 ADR（004、005）列在各步驟的 plan 檔（`docs/plan/Sxx-*.md`），寫成後再加進下表。S02 新增 007、008、009；S03 寫成預定的 003，並新增 010（座標與角度慣例）；S04 新增 011、012；S05 新增 013；S06 新增 014、015。編號 004（S09）、005 仍預留。

| 編號 | 標題 | 狀態 | 日期 |
|---|---|---|---|
| [ADR-015](adr/015-recipe-and-library-storage.md) | 配方與樣本庫的儲存：有版本的 JSON（嚴格讀取、逐欄位往返、遷移）、原子寫入、背景存檔（同一個檔案依序寫）；樣本庫一個檔案一層，照 TRI 分層（專案 → 使用者 → 唯讀的系統） | 採用（分層由使用者決定；schema 是 v1，S12 可升版） | 2026-10-06 |
| [ADR-014](adr/014-image-ownership-and-copies.md) | 影像的所有權（像素交出去就不改、owner 讓它活著）、新增 `Bgr24`／`Gray16`、影像來源只交出 `Gray8`／`Gray16`／`Bgrx32`、複製的定義與計數、一個函式庫一個 adapter（新增 `image_qt`）、Qt 在前 OpenCV 在後、校正值不從檔案讀 | 採用（HALCON 端的轉換是 S08 的預期，未驗證） | 2026-10-06 |
| [ADR-013](adr/013-lattice-acceptance-rules.md) | 晶格的接受規則：寧可沒有晶格，也不要錯的晶格（證據、補缺格的證據、二維成立的逐列判斷、連通、信心；不往外推） | 採用（門檻是起點，S07 用真實影像校準） | 2026-10-06 |
| [ADR-012](adr/012-dashed-frames-as-fill-rects.md) | 虛線與點線的框也畫成整數像素的填色矩形（取代 S02「虛線狀態仍用描邊」的做法；理由是 S04 量測推翻了「這兩種狀態的框很少」） | 採用 | 2026-10-06 |
| [ADR-011](adr/011-interaction-logic-in-application-layer.md) | 互動規則放在 application 層（沒有 Qt 的 `InspectionSession`），Presenter 只做 Qt 這一側 | 採用 | 2026-10-06 |
| [ADR-010](adr/010-coordinate-and-angle-convention.md) | 座標與角度慣例：影像像素、y 向下、角度為度、螢幕上逆時針為正（解決 ADR-006 未決 #5） | 採用（OpenCV／HALCON 端的轉換是文件上的假設，S07、S08 實測，S09 鎖住） | 2026-10-05 |
| [ADR-003](adr/003-test-framework.md) | 測試框架：統一用 Qt Test（涵蓋 domain 到 UI），效能量測自己用 `steady_clock`，不用 `QBENCHMARK` | 採用 | 2026-10-05 |
| [ADR-009](adr/009-ui-toolkit-review-after-s02.md) | S02 實測後重審 UI 工具集：I1 在預算邊緣、真實視窗超標；選項 A 維持、B OpenGL 視埠、C QML、D 調預算 | **提案（待使用者決定）**；決定前 ADR-001 照舊有效 | 2026-10-05 |
| [ADR-008](adr/008-message-bar-fixed-height.md) | 訊息列固定高度（畫布大小不因訊息而變；放不下就在列內捲動） | 採用 | 2026-10-05 |
| [ADR-007](adr/007-copy-json-as-translation-source.md) | 文案：`copy.json` 直接當翻譯來源（`JsonTranslator`＋`qtTrId`，不產生 `.ts`／`.qm`；佔位字維持具名） | 採用 | 2026-10-05 |
| [ADR-006](adr/006-sample-library-and-hierarchy.md) | 樣本庫與 Board › Component › Box 階層（流程 A 教樣本、流程 B 區域掃描、元件類型編輯） | 採用（核心方向、單位 µm、庫與板子是複本關係已由使用者決定；其餘未決見 ADR） | 2026-10-05 |
| [ADR-002](adr/002-undo-model.md) | 復原模型：Command ＋ 三個範圍（提案、類型編輯、文件） | 採用 | 2026-10-05 |
| [ADR-001](adr/001-ui-toolkit.md) | UI 工具集：Qt Widgets + QGraphicsView | 採用 | 2026-10-05 |
| [ADR-000](adr/000-ai-agent-structure.md) | AI agent 共用結構（Codex 與 Claude Code） | 採用 | 2026-10-05 |
