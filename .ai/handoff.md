# Agent Handoff

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-27
- 分支：`refactor/rust-domain`（從 `main` 1216cd9 開）；本次只有文件，已在本分支本機提交（見 git log），未推送、未合併。

## 本次交付：Rust 混合架構的計畫（R05、[ADR-031](../docs/adr/031-rust-domain-hybrid.md)）

- 使用者決定：混合架構；第一階段把 domain 的演算法（`lattice/` 全部 .cpp、`feature_extract`、`match_region`、`repeatability`）搬進 Rust crate；C++ 標頭不變；用既有測試做差異驗證。UI、OpenCV、HALCON、application 留在 C++。
- 計畫：`docs/plan/R05-rust-domain.md`（R05.1 建置整合與 NMS 試點〔關卡〕→ R05.2 晶格 → R05.3 比對區與特徵擷取 → R05.4 重複性統計 → R05.5 收尾），每一步一個 session；Start prompt 在計畫檔底部。
- 進度表：R05 ⏳（計畫完成），R05.1–R05.5 ⬜。

## 下一步

- 開新 session 做 R05.1（分支沿用 `refactor/rust-domain`），prompt 用計畫檔的「Start prompt（R05.1）」。

## 環境事實（2026-10-27 實測）

- Rust：`%USERPROFILE%\.cargo\bin`，`rustc 1.99.1`、`cargo 1.99.1`、`stable-x86_64-pc-windows-msvc`，含 clippy、rustfmt。使用者用 rustup 的預設選項安裝（改的是使用者的 PATH）。
- 安裝前啟動的程式（Claude 桌面 app、Qt Creator）的 PATH 沒有 cargo：用完整路徑，或在 `scripts/dev/env.ps1` 加入（R05.1 的工作），Qt Creator 要重新開。

## 實際驗證

- `node tools/ste-lint.mjs`：995 sentences、0 errors、0 warnings。
- 沒建置、沒跑 `scripts/dev/test.ps1`：本次不改程式、測試或建置設定。

## 要在 R05.1 實測的風險

- Debug 組態 `/MDd` 配 Rust 標準庫的 release CRT（LNK4098 或更糟）；三種組態都要建置與跑測試。
- 跨語言呼叫沒有 inline（`OverlapIndex::overlapsAny` 每個框呼叫一次）：先量 before 再搬。
- 浮點數最後一位的差異：差異測試分「完全相同」與「容差內」，容差要寫理由。
- `tst_lattice` 注入自訂的 `IVectorClustering`（`CountingClustering`）：R05.2 要用 C 函式指標回呼支援。

## 不要動與界線

- 不改 Qt Widgets、OpenCV、HALCON 的 adapter 與 application 層（不在 R05 範圍）。
- 效能預算與基準線要等使用者指示才重建（R05.5 會問）。
- C++ 的重複實作要不要刪由使用者決定（R05.5）。

## 先前仍有效的提醒

- 建置：沙箱 `env.ps1` 曾選到 MSVC 19.00；要用本機權限並前置 `VCToolsInstallDir/bin/Hostx64/x64`（確認 19.44）。source-side 的舊 `.moc` 會污染 qmake 依賴，已備份在 `build/repeatability-cache-backup`，不要回填。
- Debug 的 Presenter 2 秒忙碌等待案例是既有的不穩定測試；HALCON 讀不到授權時等 5 分鐘再試、看 CodeMeter 的 log。
- 其他待決定事項見 `docs/current-status.md` 的「待使用者決定」。
