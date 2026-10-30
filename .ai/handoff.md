# Agent Handoff

- 更新者：Claude Code (Claude Opus 5.5)，2026-10-30
- 分支：`main`（本機，沒有 push）。這一輪把 R08、R10 合併進 `main`；之前 Codex 已把 R09 合併（`666d0d1`、`e28af42`）。

## 本次合併（使用者決定：先提交 R08，再合併 R10）

- `feat/r08-socket-teaching`：`79002af` R07 晶格與一維弱候選測試的測資修復（`main` 原本會失敗的三項，Codex 在 R07.5–R07.7 的診斷，`docs/plans/completed/R07-multi-sample-merge/tasks/regression-repair.md`）、`6860de3` R08 Socket 焊點教學（Codex 的工作原樣提交）；`926f510` `--no-ff` 合併。
- [R10 status](../docs/plans/completed/R10-halcon-mask-hint/status.md)：✅ 完成。HALCON 建好樣本、還沒設定比對區時，訊息列用卡片建議「用 M 點端子的遮罩」；OpenCV 不提醒。原本編 R09／ADR-036，因為 Codex 先用了而改成 R10／[ADR-042](../docs/adr/042-match-mask-advice.md)。`3ed3832` `--no-ff` 合併，`c1ab755` 收尾。
- 衝突只在文件：INDEX 三列都留、下一個可用編號 **R11**；`decisions.md` 的 ADR-036–042 都在；本交接單重寫。

## 實際驗證

- 合併後的 `main`：`scripts/dev/test.ps1`（Release＋OpenCV＋HALCON＋Rust，含建置）**68／68 通過**；2 個既有 XFAIL（OpenCV B1、palette BMP），8 個有跳過項目的測試（含 `tst_solder_teaching` 3 項、`tst_presenter` 1 項）。HALCON 可用（`tst_halcon_backend` 通過）。`ste-lint` 0 errors，`check-ai-structure.ps1` OK（10 個既有警告）。
- 沒有跑：`-Config Debug`、`-DomainImpl Cpp`、`-Perf`、R09 的 Python／工作台測試（Codex 在合併前跑過，見下）。

## 仍進行中與已知問題

- [R08 status](../docs/plans/active/R08-socket-teaching/status.md)：⏳。R08.8（千框多選拖移）由另一個聊天負責；Codex 記錄 **Debug** 建置有 Socket 大量框拖曳的門檻失敗（1017 框 3201 ms，門檻 2000），原因未判定，沒有改源碼或門檻。
- [R09 status](../docs/plans/active/R09-deep-learning-cad/status.md)：⏳。元件深度學習工作台（Vision CAD Studio，`tools/pcb_dl/`、`VisionCadWorkbench.exe`，Qt Creator 同一個 `AiInspTool.pro`）；Codex 的驗證是 PARTIALLY VERIFIED（Python／Qt／ONNX 37/37、工作台三種組態建置；沒有重訓正式模型、GPU、高 DPI；R09.7 品質待真實資料）。
- R07 留下的後續還沒開計畫：通用樣本比對與 Socket 結構偵測並存或整合；右側 dock 重疊。

## 不要動

- `.archify/`、`AiInspTool/cargo/`：來源不明、不入版控，不清理、不 stage。
- R09 的使用者標註資料：原圖 `D:/BMP/PCB`；`build/r09-shapes-backup` 只有舊存檔，不能覆蓋使用者現有的工作。同板同視野的影像不可拆到 train／val／test。
- worktree `.claude/worktrees/r09-halcon-mask-hint-f0d064`（分支 `claude/r09-halcon-mask-hint-f0d064`）與 `feat/r08-socket-teaching` 內容都已在 `main`，要不要刪由使用者決定。
