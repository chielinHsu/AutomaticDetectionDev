# Agent Handoff

- 更新者：Claude Code (Claude Fable 5.1)
- 更新日期：2026-10-29（R07 完成並合併；末段濃縮保留 Codex 2026-10-29 的 R07 回歸修復與 R08 交接）
- 分支：`fix/r07-multi-sample` 以 `--no-ff` 合併回 `main`（本機，沒有 push）；現在在 `main`。

## 本次工作與狀態

- [R07 status](../docs/plans/completed/R07-multi-sample-merge/status.md)：✅ 完成，資料夾搬到 `completed/`。這個 session 做了 R07.8（影像上驗證不到的推算格預設不回報）與 R07.9（低分的補漏候選要有 4 個鄰居；一維兩側都要有），都在 use case（`GenerateOptions::reportUnverifiedHoles`、`minNeighboursForWeakHoles`；`GridSummary::unverified`、`unsupported`），domain 與 Rust 沒動；設計與量測在 ADR-035（含後續）。R07.4 取消（主因是晶格，不是合併規則）。
- 量測工具 `tests/backend/perf_multi_sample_real` 多 `AOI_MULTI_MATCH`（矩形或自動擷取的遮罩）、`AOI_MULTI_DUMP`（CSV）、log 的 `unverified`／`unsupported`。比對區的結論（矩形遍地誤報、HALCON 點端子的遮罩有效、OpenCV 維持整框）在 R07 `plan.md` 的「R07.8 的量測」。
- 畫面沒有改：「推算」永遠 0、「拒絕」少掉邊界的雜訊。

## 實際驗證

- R07.9：完整 `test.ps1`（Release、Rust、含建置）66／67，只有 `tst_presenter` 的提案格數 32 → 28；改標準答案後 `-Filter tst_presenter` 通過。R07.8 當時完整 `-NoBuild` 67／67。原本就有的 2 個 XFAIL（`tst_contract_end_to_end` OpenCV B1、`tst_image_sources_agree` palette BMP）與 8 個有跳過項目的測試不變。
- 真實影像（LeftTopRegion、整框）：HALCON 推算 13 → 0、拒絕 216 → 180，OpenCV 推算 7 → 0、拒絕 42 → 9；找到的數字不變。沒有跑 `-DomainImpl Cpp`、`-Perf`；合併後沒有再建置（`main` 側沒有改到同一批檔案）。

## 未提交與不要動

- Codex 的 R08（Socket 教學；`src/presentation/solder/`、`GenerateSocketCad`、`OpenCvSocketRegionRecognizer` 等）、`tests/domain/tst_lattice.cpp`、`docs/plans/INDEX.md` 的 R08 列與「下一個可用編號 R09」、`docs/plans/active/R08-socket-teaching/`、`docs/plans/completed/R07-multi-sample-merge/tasks/regression-repair.md`（隨資料夾搬過來）仍未提交：不清理、不回復、不一併 stage（INDEX 只 stage 了 R07 那一列）。R08 的 `changelog.md` 裡指到 R07 `tasks/` 的連結因為搬資料夾改成 `../../completed/…`（只改那一行）。R08 進行中時 `AiInspTool.exe` 曾連結失敗，幾分鐘後自己好；再遇到先等一下再建。
- `.archify/`、`AiInspTool/cargo/` 不是本計畫新增的；`build/` 與產線影像不入版控。
- 主視窗沒有儲存配方的功能（〔儲存〕停用）；要重現使用者的樣本用「選取資訊」表單的數字。

## 後續範圍（待使用者決定）

- HALCON 的召回／畫面上建議用 `M` 點端子的遮罩；產品方向（通用的樣本比對與 Socket 結構偵測並存或整合、問題 3 的深度學習方向）；問題 2（右側 dock 重疊）。都還沒開計畫。

---

# （保留、濃縮）Codex (gpt-6.1-sol) 2026-10-29 的交接

- 當時分支 `fix/r07-multi-sample`、HEAD `970ac5f`；使用者當時要求不 commit、不 push。
- R07.5–R07.7（已併入 R07）：四個回歸失敗中三組是過期 C++ obj／測試與新 Rust lib 混用，重建後轉綠；Lattice 是測資（合成圖界、半缺件的間距證據），只改 `tests/domain/tst_lattice.cpp`，`src/domain`、`rust/aoi_domain_rs` 沒動。證據：[regression-repair.md](../docs/plans/completed/R07-multi-sample-merge/tasks/regression-repair.md)；log 與快照在 `build/r09-repro/`（scratch 名稱，沒有 R09 計畫）。
- [R08 status](../docs/plans/active/R08-socket-teaching/status.md)：MVP 已交付；R08.5–R08.7 樣本全圖搜尋完成（`SearchSocketSamples`、`ISocketSampleSearcher`、`SolderTeachingSession`、`SolderSampleWorkflow`、視窗／UI、app 組裝、文案／qmake／測試；ADR-034）。專項 15/0/0、全量 67/67；真實圖 3 樣本得到 632 個 44×24 結果；不宣稱精確率／召回率。入口 `build/SocketTeaching-Samples/StartSocketSamples.cmd`；log／截圖在 `build/r08-samples`。另一聊天的 R08.8 拖移修正由該聊天維護。
- 規則：`docs/legacy/` 只讀；範本管理檔不改；不要在 `AiInspTool/` 原地 qmake；其他 session 同時改來源時先比對 source／obj 時間，不要以 `-NoBuild` 當成完整重建。
