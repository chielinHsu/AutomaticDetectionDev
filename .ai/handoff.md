# Agent Handoff

- 更新者：Claude Code (Claude Fable 5.2)，2026-11-12；目前 main。本輪只做 [R09](../docs/plans/active/R09-deep-learning-cad/status.md) 的 R09.25；使用者同意後已提交於 `fix/r09-handle-frame-sync` 並以 no-ff 合回 main（hash 見 git log），沒有 push。

## 本輪（R09.25）

- 使用者錄影回報：Vision CAD Studio 拖角點時藍框與白色把手分離。根因：`Vertex` 設了 ItemIsMovable，Qt 內建拖曳會把已選取的框一起平移，`Canvas.preview_vertex` 又以絕對座標重設形狀。
- 修改 `tools/pcb_dl/canvas_items.py`（把手自行處理拖曳，不再 ItemIsMovable）；新增回歸測試於 `tests/pcb_dl/test_contour_interaction.py`。
- 文件：R09 的 plan.md（R09.25 一列）、status.md、changelog.md、本交接。INDEX 未改。

## 驗證

- 新測試修正前 3 列失敗、修正後通過；Python／Qt／ONNX 全套 60/60（54.829s），`build/r09-handle-sync-tests.log`。
- `AiInspTool/bin/vision-workbench/tools/pcb_dl/canvas_items.py` 已同步修正後的檔案（原本與 HEAD 相同）。使用者執行中的 VisionCadWorkbench 不會熱載入，需存檔後重啟。
- PARTIALLY VERIFIED：未在使用者實際視窗人工驗收；未重跑 C++／Rust 全套。

## 本輪提交的檔案

- `tools/pcb_dl/canvas_items.py`、`tests/pcb_dl/test_contour_interaction.py`
- `docs/plans/active/R09-deep-learning-cad/plan.md`、`status.md`、`changelog.md`、`.ai/handoff.md`
- 工作目錄沒有本輪未提交的變更；使用者尚未回報實際視窗的驗收結果。

## 下一步與既有限制

- R09.7 仍未做：需足夠獨立板子／批次的人工標註、正式訓練與品質驗收。同板不同 Z／通道不可拆 train/val/test；合成 smoke 模型 mAP=0，不可當可用模型。
- 前次 C++ Debug Socket 大量框拖曳門檻失敗仍未修，不調門檻。
- R08.8 仍進行中；S12 規劃中。

## 不要動

- `.archify/`、`AiInspTool/cargo/` 來源不明，不清理、不 stage。
- D:/BMP/PCB 原圖、使用者最新標註與執行中的工作台程序（有幾個 `tools.pcb_dl.cli gui` 與 VisionCadWorkbench 視窗開著，可能有未存檔的標註）；`build/r09-shapes-backup/last-saved.json` 是舊副本，不得覆蓋目前資料。
- 已合併的 `.claude/worktrees/r09-halcon-mask-hint-f0d064`，刪除須使用者決定。
