# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史，保持在 60 行內。流程見 `ai-handoff` skill。
> 本檔只寫「上一個 session 留下了什麼」。計畫的進度與接續資訊在各計畫的 `status.md`（入口 `docs/plans/INDEX.md`），這裡不重抄；2.x 的紀錄在 `docs/legacy/`。

- 最後一位 agent：Codex (gpt-6.1-sol，R08)；下方保留 Claude Code 的 R07 交接。
- 日期：2026-10-28
- 分支：`fix/r07-multi-sample`（從 `main` 開；本機提交，沒有合併、沒有 push）。

## 上一個 session 動到的計畫

- R07（多樣本搜尋）：R07.1–R07.3 完成，見 [`docs/plans/active/R07-multi-sample-merge/`](../docs/plans/active/R07-multi-sample-merge/status.md)。結論：使用者回報的「樣本越多越差」主因是晶格估計不支援交錯排列與陣列中間的長槽；晶格的索引區域改成三個條件的交集（ADR-013 的「後續（2026-10-28，R07.2）」），Rust 與 legacy C++ 兩份，C ABI 版本 5。
- 同一時間 Codex 在同一個工作目錄做 R08（Socket 教學，未提交）：它的檔案本 session 沒有動、沒有 stage。

## 接下來

- 使用者用程式驗證 R07.2（HALCON／OpenCV、同一張板子、幾個樣本）；再決定 R07.4（合併規則）與 HALCON 召回的計畫（`status.md`「待使用者決定」）。
- `docs/plans/INDEX.md` 裡還在規劃中的是 S12；R08 由 Codex 進行中。

## 不要動

- `docs/legacy/` 只讀；範本管理的檔案不要在專案裡改（`engineering-workflow.md`、`ai-handoff` skill、`check-ai-structure.ps1`、`sync-skills.ps1`、`migrate-ai-structure-3.mjs`、`tools/ste-lint.mjs`、`docs/plans/_template/`、`docs/adr/_template.md`）。
- 不要在 `AiInspTool/` 原地執行 qmake；建置資料夾不要改名。
- Codex 的 R08 修改（`src/app/CompositionRoot.*`、`src/app/main.cpp`、`src/presentation/shell/MainWindow.*`、`src/presentation/solder/`、`src/application/interaction/SolderTeachingSession.*`、`src/application/ports/ISolderRegionDetector.h`、`src/infrastructure/backend_opencv/OpenCvSolderRegionDetector.*`、`tests/ui/tst_solder_teaching.*`、`docs/ux/copy.json`、各 `.pro` 的對應行、`docs/plans/active/R08-socket-teaching/`、ADR-033）是別人的未完成工作，不清理、不回復、不 stage。

## 未提交的變更

- 本 session 的檔案已提交在 `fix/r07-multi-sample`（見 R07 的 `changelog.md`）；Codex 的 R08 未提交。
- `build/clip/`（影片影格、縮圖與裁切工具）不入版控，可以刪。

## 假設與未驗證項目

- R08 早期的 QCOMPARE 列舉編譯問題已修正；最終 Socket 測試 10 passed、0 failed、0 skipped（Codex 補充）。
- HALCON 在同一張板子的種子召回只有約一半（210／約 360；OpenCV 360），規則 2 剛好在界線上：有些樣本仍退回一維。這是搜尋器的參數問題，沒有處理。
- `perf_match_region_real` 與 `tst_repeatability::realFiveRuns` 照舊跳過，未驗證。

## Codex 補充：R08 Socket MVP

- 更新者：Codex (gpt-6.1-sol)，2026-10-28；保留上方 R07 交接與其他 session 的提交。
- R08 狀態見 [status.md](../docs/plans/active/R08-socket-teaching/status.md)：MVP 已交付，整體 PARTIALLY VERIFIED。
- Release、Debug 無 OpenCV／HALCON建置成功；Socket 10/0/0；全量 63/67，未通過：tst_generate_objects、tst_lattice、tst_presenter、tst_domain_rs_diff。
- R08 未提交、未 push；不要清理其新增檔案或與 R07 共用 .pro 中的對應行。
- 用 scripts/dev/build-socket-mvp.ps1 重建；一般程式入口或 --socket 開啟。原圖另存，JSON v3 配方保存框與圖片路徑。
