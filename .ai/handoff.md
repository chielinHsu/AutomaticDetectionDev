# Agent Handoff

- 更新者：Claude Code (Claude Fable 5.2)，2026-11-12；目前 main。本輪只做 [R09](../docs/plans/active/R09-deep-learning-cad/status.md) 的 R09.26；使用者同意後已提交於 `feat/r09-live-geometry-fields` 並以 no-ff 合回 main（hash 見 git log），沒有 push。

## 本輪（R09.26）

- 使用者要求：Vision CAD Studio 第 2 步右側「幾何資訊」的欄位（中心 X／Y、寬、高、角度）一改，畫布上的框就即時更新，不必按〔套用矩形幾何〕。
- `tools/pcb_dl/workbench_ui.py`：欄位的 valueChanged／editingFinished 接線；按鈕保留。
- `tools/pcb_dl/workbench_presenter.py`：`live_annotation_geometry`（即時套用）、`annotation_field_points`（共用檢查）、程式回寫欄位時擋訊號、同一欄位的連續調整合成一筆 Undo。
- `tools/pcb_dl/canvas.py`：`Canvas.update_object` 就地更新單一框、相依檢測框與把手，不重建場景。
- 測試：新增 `tests/pcb_dl/test_live_geometry.py`；`tests/pcb_dl/test_workbench_shell.py` 的按鈕測試改成設定欄位時擋訊號。
- 文件：`tools/pcb_dl/README.md`、R09 的 plan.md（R09.26 一列）、status.md、changelog.md、本交接。INDEX 未改。

## 驗證

- 新測試以 HEAD 版程式跑為 7 列失敗、修改後通過；Python／Qt／ONNX 全套 65/65（59.742s），`build/r09-live-geometry-tests.log`。
- `AiInspTool/bin/vision-workbench/tools/pcb_dl/` 的 canvas.py、workbench_ui.py、workbench_presenter.py 已同步（原本與 HEAD 相同）。使用者執行中的工作台不會熱載入，需存檔後重啟。
- PARTIALLY VERIFIED：未在使用者實際視窗人工驗收；未重跑 C++／Rust 全套。

## 本輪提交的檔案（全部屬於 R09.26）

- `tools/pcb_dl/canvas.py`、`workbench_presenter.py`、`workbench_ui.py`、`README.md`
- `tests/pcb_dl/test_live_geometry.py`（新檔）、`tests/pcb_dl/test_workbench_shell.py`
- `docs/plans/active/R09-deep-learning-cad/plan.md`、`status.md`、`changelog.md`、`.ai/handoff.md`

## 下一步與既有限制

- 工作目錄沒有本輪未提交的變更；使用者尚未回報實際視窗的驗收結果。
- 可能的後續（未做）：第 5 步 CAD 檢視的欄位（〔套用單框修改〕，`workflow.apply_geometry`）也改成即時。
- R09.7 仍未做：需足夠獨立板子／批次的人工標註、正式訓練與品質驗收。同板不同 Z／通道不可拆 train/val/test；合成 smoke 模型 mAP=0，不可當可用模型。
- 前次 C++ Debug Socket 大量框拖曳門檻失敗仍未修，不調門檻。
- R08.8 仍進行中；S12 規劃中。

## 不要動

- `.archify/`、`AiInspTool/cargo/` 來源不明，不清理、不 stage。
- D:/BMP/PCB 原圖、使用者最新標註與執行中的工作台程序（有幾個 `tools.pcb_dl.cli gui` 與 VisionCadWorkbench 視窗開著，可能有未存檔的標註）；`build/r09-shapes-backup/last-saved.json` 是舊副本，不得覆蓋目前資料。
- 已合併的 `.claude/worktrees/r09-halcon-mask-hint-f0d064`，刪除須使用者決定。
