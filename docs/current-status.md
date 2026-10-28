# 目前狀態（專案層級）

> 原地修改，保持精簡。只寫**整個專案**的快照：階段、跨計畫的已知問題與待使用者決定。
> 各計畫的細項、進度與變更紀錄在自己的資料夾，入口是 [`plans/INDEX.md`](plans/INDEX.md)；不要把計畫的細節寫進本檔（規則見 `.agents/harness/engineering-workflow.md`「計畫分開管理」）。
> 3.0.0 之前的需求進度表與目前狀態（含 S01–S11、R01–R04 各自的「產出與發現」、預算校準狀態、完整的「未解決事項」）封存在 [legacy/current-status.md](legacy/current-status.md)（只讀）。

- 更新日期：2026-10-28
- 階段：S00–S11 完成，R01–R04 完成；R05（Rust 混合架構第一階段：domain 的演算法搬進 Rust，ADR-031）2026-10-28 完成，`AOI_DOMAIN_IMPL` 預設 `rust`，C++ 實作保留成 `cpp` 選項；S12（匯出、打包、最終文件）尚未開始。2026-10-27 起 AI 結構 3.0.0（R06，ADR-032）。全部只在本機，沒有 push。

## 計畫

- 所有計畫與狀態：[`plans/INDEX.md`](plans/INDEX.md)。規劃中：[S12](plans/active/S12-release/status.md)。

## 跨計畫的已知問題

細節與證據在 [legacy/current-status.md](legacy/current-status.md)「未解決事項」與各步驟的「產出與發現」：

- 對時間敏感的測試在機器滿載時會失敗（`tst_cli` 的 HALCON、`tst_presenter`、`tst_contract_searcher` 的取消延遲），單獨重跑；Debug 的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce` 本來就不穩定。
- 效能量測雜訊大（處理器常只跑在額定速度的 54–67%）：量測前關掉其他負載，用 `perf.ps1 -Runs` 看範圍，before／after 交替。
- HALCON 的 dongle 偶爾讀不到授權：等 5 分鐘再試，看 CodeMeter 的 log（`AI_CONTEXT.md` 的環境表）。
- 不要在 `AiInspTool/` 原地建置（Qt Creator 的建置資料夾不要設成原始碼資料夾、不要在那裡執行 qmake）：原地的 `.moc` 會被 qmake 的相依掃描抓到，**全新的建置資料夾編不過**（找不到 `tst_*.moc`），既有的建置資料夾則不會重新 moc（新的測試函式不執行）。2026-10-27 13:00 的那一份原地建置在 2026-10-28（R05.5，使用者同意）搬到 `build/r05-5-insource-backup/`（`moved-list.txt` 是清單，可以搬回；不要的話整個刪掉）。
- 存檔與開啟配方的 UI、樣本庫 UI（`LibraryPresenter`、`TypeEditorPresenter`）、流程 B 的 UI 沒有排進任何計畫。
- 設定記憶（主題、字級、語言）、減少動態、高 DPI 的實際外觀沒有做；Codex 端沒有端到端實測。
- 程式碼註解裡有兩處舊計畫檔路徑（`tests/infra/perf_storage.cpp`、`tests/perf/HarnessSupport.h`），檔案現在在 `docs/legacy/plan/`。

## 待使用者決定（跨計畫）

1. 真實影像上的門檻與晶格（S07）：`seedMinScore` 與預設分數門檻 0.70 會放進焊墊邊緣；晶格在非矩形陣列的空槽補假缺格。
2. HALCON（S08）：影像來源（`read_image`）要不要做、授權提醒規則是否符合期待、兩個 back end 的預設門檻要不要分開、`nativeGray16` 要不要改成預設（ADR-004）。
3. 預算：AB-U3、T7（流程 B 的樣本庫比對）要不要寫進 `perf/budgets.json`；I1 在預算邊緣（ADR-009）。
4. 存檔與開啟配方的 UI、樣本庫 UI、流程 B 的 UI（ADR-025 的 P2）要排進哪個計畫；S12 何時開始。
5. Debug 的 `tst_presenter` 2 秒忙碌等待要不要放寬。
6. ADR-028 的真實影像標準答案（1109 根端子的 Otsu 重心）要使用者目視確認；HALCON 比對區沒有門檻同時達到召回 ≥ 99.5% 與誤報 ≤ 5，要不要調。
7. 靜態重複性（ADR-029）：效能預算與基準線是否建立。
8. use case 驗證階段的漏洞（視窗裡找到的匹配離別的已知候選更近時仍當成那個缺格的元件）要不要補（application 層，另開計畫）。

9. Rust 第二階段（application 層搬進 Rust；FFI 要不要改用 cxx，ADR-031）：使用者 2026-10-28 決定先不做，之後再決定。
10. 真實資料的兩項驗證（R05 沒有做到）：`perf_match_region_real` 的影像 `FovImages1\0_0_HA.tif` 2026-10-20 換過，`AOI_REAL_MATCH_CLICK`／`BODY` 的預設對不上；`tst_repeatability::realFiveRuns` 的原始五份 CSV（2026-10-14、703 Box）在這台機器找不到。使用者 2026-10-28 決定先跳過，之後提供位置或新參數再補測（細節在 [R05 的 status.md](plans/completed/R05-rust-domain/status.md)）。
