# Agent Handoff

- 更新者：Claude Code (Claude Fable 5.1)
- 更新日期：2026-10-29（R07.8；下方保留 Codex 2026-10-29 的 R07 回歸修復與 R08 交接）
- 分支：`fix/r07-multi-sample`（沒有合併、沒有 push）。

## 本次工作與狀態

- [R07 status](../docs/plans/active/R07-multi-sample-merge/status.md)：R07.8 完成——影像上驗證不到的推算格預設不回報（`GenerateOptions::reportUnverifiedHoles` 預設 false、`GridSummary::unverified`；ADR-035）。使用者在程式裡驗證 R07.2 時看到長槽角落的「推算」框與槽緣 0.71 的「找到」，量測後決定不畫。
- 量測工具 `tests/backend/perf_multi_sample_real` 多 `AOI_MULTI_MATCH`（矩形或自動擷取的遮罩）與 `AOI_MULTI_DUMP`；比對區的結論（矩形遍地誤報、HALCON 點端子的遮罩有效、OpenCV 維持整框）在 R07 `plan.md` 的「R07.8 的量測」。
- 畫面沒有改：「推算」的計數與狀態保留，預設下是 0。

## 實際驗證

- 完整 `test.ps1`（Release、Rust、含建置）：改 use case 後 66／67，只有 `tst_presenter` 的提案格數 37 → 32（影像上驗證不到的缺件不再回報）；改標準答案後 `-Filter tst_presenter` 通過。最後一次 `test.ps1 -NoBuild` 的結果見 R07 `changelog.md`／`status.md`。
- 原本就有的 2 個 XFAIL（`tst_contract_end_to_end` OpenCV B1、`tst_image_sources_agree` palette BMP）與 8 個有跳過項目的測試不變。
- 真實影像（LeftTopRegion、HALCON）推算 13 → 0、OpenCV 7 → 0，其餘數字不變。沒有跑 `-DomainImpl Cpp`、`-Perf`。

## 未提交與不要動

- 本次的提交只 stage 本計畫的檔案（use case、四個測試、量測工具、ADR-035、decisions、architecture、R07 的 plan／status／changelog、本交接單）。
- Codex 的 R08（Socket 教學；`src/presentation/solder/`、`GenerateSocketCad`、`OpenCvSocketRegionRecognizer` 等）與 R07.5–R07.7 的 `tests/domain/tst_lattice.cpp`、`tasks/regression-repair.md` 仍未提交：不清理、不回復、不一併 stage。R08 進行中時 `AiInspTool.exe` 曾連結失敗，幾分鐘後自己好；再遇到先等一下再建。
- `.archify/`、`AiInspTool/cargo/` 不是本計畫新增的；`build/` 與產線影像不入版控。
- 主視窗沒有儲存配方的功能（〔儲存〕停用）；要重現使用者的樣本用「選取資訊」表單的數字。

## 後續範圍（待使用者決定）

- 使用者看 R07.8 之後的畫面；R07.4 要不要做；HALCON 的召回／預設用遮罩要不要另開計畫；`fix/r07-multi-sample` 何時合併回 `main`。
- 問題 2（右側 dock 重疊）與問題 3（流程 B／不用樣本的提案）還沒開計畫。

---

# （保留）Codex (gpt-6.1-sol) 2026-10-29 的交接

- 分支：fix/r07-multi-sample；當時 HEAD 為 970ac5f。使用者當時明確要求：不 commit、不 push；沒有 stage。

## 本次工作與狀態

- [R07 status](../docs/plans/active/R07-multi-sample-merge/status.md)：R07.5–R07.7 完成；原 R07.1–R07.3 與提交 38e94de 保留，R07.4 仍待決定。
- 四個失敗中三組是過期 C++ obj／測試與新 Rust lib 混用；重建來源、legacy、moc 與測試後轉綠。Lattice 修正合成圖界及半缺件資料的間距證據，另補裁切、無證據角落與棋盤安全性案例。
- 先前 R07 回歸修復只改 tests/domain/tst_lattice.cpp；src/domain、rust/aoi_domain_rs 與開工快照 hash 相同，未修改 R07 演算法或 FFI。
- [R08 status](../docs/plans/active/R08-socket-teaching/status.md)：MVP 已交付；修正原 63/67 的過期資訊，最新全量 67/67。R08.4 的警示只保留既有未驗條件，不是四個失敗尚未修復。

## 實際驗證

- 完整 Release Rust＋OpenCV＋HALCON：test.ps1 -BuildDir build/qmake-r08 -NoBuild，exit 0，67/67。
- 四組：GenerateObjects 54/0/0、Lattice 77/0/0、Presenter 86/0/1、Rust/C++ 差異 170/0/1（passed/failed/skipped）；四個原始最小重現各兩次轉綠。
- C++ Lattice 77/0/0；Socket 專項 10/0/0，真實圖圈選、改框與配方往返通過，28 框。
- 仍有 7 組既有 skip 與 2 個已知 XFAIL（OpenCV B1、palette BMP），整體 PARTIALLY VERIFIED；不新增或放寬 XFAIL。
- 證據及前後流程：[regression-repair.md](../docs/plans/active/R07-multi-sample-merge/tasks/regression-repair.md)；log、來源快照、Socket 截圖在 build/r09-repro/（scratch 名稱，沒有 R09 計畫）。

## 未提交與不要動

- tests/domain/tst_lattice.cpp、R07 plan/status/changelog/tasks、R08 驗證文件未提交。
- 原 R08 的 app／presentation／application／OpenCV adapter、測試、qmake 清單、文案、ADR-033、文件仍未提交；不可清理、回復或一併 stage。
- docs/legacy/ 只讀；範本管理檔不要修改。不要在 AiInspTool/ 原地 qmake，來源樹生成檔會干擾 shadow build 的 moc 相依。
- 有其他 session 同時更新來源時，先比對 source/obj 時間及來源快照；不要以 NoBuild 或只重連結當成完整重建。

## R08.5–R08.7 樣本全圖搜尋

- 已完成 R 教樣本／F5 全圖匹配、第一樣本統一結果尺寸、相似度去重及樣本／門檻的 v3 配方存讀。沒有使用高度圖。
- 最新專項 15/0/0、全量 67/67；真實圖 3 樣本得到 632 個 44×24 結果。仍保留既有 7 組 skip 與 2 個 XFAIL，不宣稱精確率／召回率。
- Release、Debug 無 OpenCV／HALCON成功；build/SocketTeaching-Samples 的 StartSocketSamples.cmd 為新版入口，舊執行包不會自動更新。
- 來源：SearchSocketSamples、ISocketSampleSearcher、SolderTeachingSession、SolderSampleWorkflow、視窗／UI、app 組裝、文案／qmake／測試；ADR-034。R07 演算法未動。
- 已保留另一聊天 R08.8 拖移修正，其進度由該聊天維護；不覆蓋共享檔案。最新 log／截圖在 build/r08-samples。
- 未 stage、commit、push；其他既有未提交內容保留。
