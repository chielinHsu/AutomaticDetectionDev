# R07 變更紀錄

> 只新增，新紀錄放最上方；只記本計畫的事。每個 agent session 一筆。

## 2026-10-29 — Claude Code (Claude Fable 5.1)：R07.9 低分的補漏候選要有足夠的鄰居；R07 收尾

### 細項
- 使用者看了 R07.8 之後的畫面：長槽鋸齒、板邊還有紅色虛線的「拒絕」（補漏驗證到 0.30–0.69 的候選）。用同一張影像的候選分區統計「晶格上 8 個鄰居裡被佔的數量」：鋸齒、板邊、槽內的最多 3–4 個，陣列裡真端子的低分候選 HALCON 190 個裡 183 個 ≥ 4（數字在 ADR-035 的「後續（R07.9）」）。
- R07.9：完成。`GenerateOptions::minNeighboursForWeakHoles`（預設 4；一維兩側都要有；0 ＝ 不檢查）、`GridSummary::unsupported`：驗證分數低於 `seedMinScore` 的缺格鄰居不夠就不回報、也不拿來修正晶格。假世界的標準答案 `Oracle::supported`／`weakReported`（`tst_generate_objects` 的 `weakPartsAreRecoveredByWindowVerification`、`aRotatedNoisyGridWithMixedPartsMatchesTheOracle`），新增 `aWeakPartWithFewerThanFourNeighboursIsNotReported`（6 × 4 缺角，關掉規則照舊回報）與 `aWeakPartInARowNeedsBothNeighbours`（一維）；`tst_presenter` 的提案格數 32 → 28。
- R07.4：取消（使用者同意）。after 的數字顯示主因是晶格、不是合併規則。
- 計畫完成：資料夾搬到 `completed/`，`fix/r07-multi-sample` 以 `--no-ff` 合併回 `main`（本機，沒有 push）。

### 修改的檔案
- src/application/usecases/GenerateObjectsFromSample.h、.cpp
- tests/core/tst_generate_objects.cpp、tests/ui/tst_presenter.cpp、tests/backend/perf_multi_sample_real.cpp（log 多 `unsupported`）
- docs/adr/035-unverified-holes-not-reported.md（後續）、docs/plans/INDEX.md、docs/plans/completed/R07-multi-sample-merge/（plan、status、changelog）、.ai/handoff.md

### 驗證
- 完整的 `test.ps1`（Release、Rust 預設、含建置）：66／67，只有 `tst_presenter` 的格數 32 → 28（邊上 4 個低分的弱零件不回報）；改標準答案後 `test.ps1 -Filter tst_presenter` 通過（見下一行）。原本就有的 2 個 XFAIL 與 8 個有跳過項目的測試不變。
- 真實影像（LeftTopRegion、`perf_multi_sample_real`）：HALCON 拒絕 216 → 180（`unsupported 37`：鋸齒 10、板邊 20、陣列裡 7）、分數 0.3–0.5 的候選 20 → 0、離格 1 → 0；OpenCV 拒絕 42 → 9（`unsupported 33`）；兩邊「找到」的數字不變（861／1083）。疊圖 `build/qmake-release/test-work/r079-*`：槽角只剩鋸齒上 2 個剛好有 4 個鄰居的灰框。
- 沒有跑 `-DomainImpl Cpp`、`-Perf`。

### Commit
- 見下方（提交後補）。

## 2026-10-29 — Claude Code (Claude Fable 5.1)：R07.8 影像上驗證不到的推算格預設不回報

### 細項
- 使用者在程式裡驗證 R07.2（LeftTopRegion 的 `0_0_HA.tif`、HALCON、樣本 427,634 100×100）：長槽角落仍有「推算」框、槽緣有 0.71 的「找到」；另問「只框端子當比對區會不會比較好」。量測（本次把工具加上比對區與 CSV）：槽內的推算格是陣列角落「兩個方向都貼著內點」的規則補出來的、驗證不到（< 0.30），換範本（整框、矩形、自動擷取的遮罩、外擴）都還在；槽緣鋸齒的分數整框時剛好 0.70；矩形比對區在兩個 back end 都遍地誤報（HALCON 2250、OpenCV 1771 個）；點端子自動擷取的遮罩在 HALCON 有效（鋸齒 0.70 → 0.48、≥ 0.70 的真端子 855 → 1059、沒有離格的，但分數壓到 0.73–0.85），OpenCV 用遮罩多約 100 個離格誤報。數字在 `plan.md` 的「R07.8 的量測」。
- R07.8：完成。使用者決定槽內那些格子不畫：`GenerateOptions::reportUnverifiedHoles`（預設 false）、`GridSummary::unverified`；use case 第 5 步只在打開時回報 `Predicted`。核心（`CoreTestSupport.h` 的 `makeRequest`）與契約（`tst_contract_end_to_end`）的請求打開選項，繼續驗證補漏的機制；新測試 `tst_generate_objects::unverifiedHolesAreCountedButNotReportedByDefault`；`tst_presenter` 的提案格數 37 → 32、推算 ≥ 1 → 0。畫面沒有改。設計：ADR-035。

### 修改的檔案
- src/application/usecases/GenerateObjectsFromSample.h、.cpp
- tests/core/CoreTestSupport.h、tests/core/tst_generate_objects.cpp、tests/contract/tst_contract_end_to_end.cpp、tests/ui/tst_presenter.cpp
- tests/backend/perf_multi_sample_real.cpp（`AOI_MULTI_MATCH`：矩形或 `mask:cx,cy[,grow][,edge]` 自動擷取的遮罩；`AOI_MULTI_DUMP`：候選倒成 CSV；log 多 `unverified`）
- docs/adr/035-unverified-holes-not-reported.md（新增）、docs/decisions.md、docs/architecture.md、docs/plans/active/R07-multi-sample-merge/（plan、status、changelog）、.ai/handoff.md

### 驗證
- 完整的 `test.ps1`（Release、Rust 預設、含建置）：改完 use case 後 66／67，只有 `tst_presenter` 的格數 37 → 32；改了標準答案之後 `test.ps1 -Filter tst_presenter` 通過（85 passed、1 skipped：原本就有）。最後再跑一次 `test.ps1 -NoBuild`：67／67 通過。原本就有的 2 個 XFAIL（OpenCV 的 B1、palette BMP）與 8 個有跳過項目的測試不變。
- 真實影像（LeftTopRegion、`perf_multi_sample_real`）：HALCON 推算 13 → 0（`unverified 13`），OpenCV 7 → 0（`unverified 7`）；找到、拒絕、離格的數字不變（HALCON 0.69 門檻 found 862／rejected 216，OpenCV 1083／42）。
- 一度建置失敗（`AiInspTool.exe` 連結不到 Codex R08 的 `GenerateSocketCad`、`OpenCvSocketRegionRecognizer`）：是另一個 session 進行中的檔案，幾分鐘後自己好了，不是本計畫的事。
- 沒有跑 `-DomainImpl Cpp`（改的是 application 層，domain 兩種實作都沒動）、沒有跑 `-Perf`。

### Commit
- `ad88b05` [Caspar]fix: 影像上驗證不到的推算格預設不回報（分支 `fix/r07-multi-sample`，本機，沒有合併、沒有 push）。

## 2026-10-29 — Codex (gpt-6.1-sol)：四個回歸修復收尾

- R07.5–R07.7 完成；診斷及測試於 2026-10-28 執行，本日完成文件收尾。原 R07.1–R07.3 紀錄及 R07.4 待決定事項保留。
- GenerateObjects、Presenter 與 Rust/C++ 差異的失敗來自過期 C++ 測試／wrapper／legacy 物件混入較新的 Rust 庫。來源已是新規則，但舊 exe 仍預期 44（來源為 37）；重編對應目標後轉綠，未改演算法。
- Lattice 的剩餘問題是測資：交錯板到 x=2994，卻用只到 x=2000 的範圍取缺格；新增專用範圍，保留精確 6 格斷言，另驗證舊範圍裁掉 2 格。
- 原半缺件資料為棋盤式刪除，沒有 154 px 相鄰對，不符合預設方向投票證據。正面案例仍缺 30/60，每欄保留連續三點；保留一組二維、30 個內點、154 px 間距，並逐點驗證手列的 15 個有證據缺格。原棋盤資料保留為安全性案例，另補無證據角落案例。
- 修改：`tests/domain/tst_lattice.cpp`、本計畫 plan/status/tasks/changelog、R08 的驗證快照與交接。`src/domain`、`rust/aoi_domain_rs` 的來源 hash 與開工快照相同；不回復交錯陣列／長槽設計。
- 驗證：四個最小案例紅→綠各兩次；完整 `test.ps1 -BuildDir build/qmake-r08 -NoBuild` exit 0、67/67；Rust 與 C++ Lattice 各 77 passed。四組分別為 GenerateObjects 54/0/0、Lattice 77/0/0、Presenter 86/0/1、Rust/C++ 差異 170/0/1（passed/failed/skipped）。
- Socket 專項 10/0/0；真實圖選區 28 框，圈選、單框編輯、取消與配方存讀正常；截圖 `build/r09-repro/Socket-verified.png`。更新見 [R08 changelog](../R08-socket-teaching/changelog.md)。
- 全域仍有原本 7 組 skip 與 2 個 XFAIL，整體不標為完全 VERIFIED。詳見 [診斷與驗證](tasks/regression-repair.md)。未做效能最佳化或修改預算。
- 未 stage、未 commit、未 push；HEAD 仍為 `970ac5f`。保留 R08 未提交內容、`.archify/` 與 `AiInspTool/cargo/`。

## 2026-10-28 — Claude Code (Claude Fable 5.1)

### 細項
- R07：建立計畫（診斷寫在 `plan.md`）。
- R07.1：完成。新增 `tests/backend/perf_multi_sample_real`（效能量測，`AOI_WITH_OPENCV=ON` 才有；每個可用的 back end 一列；含晶格的診斷 `diagnoseLattices`），before 的數字與晶格的診斷寫進 `plan.md`。結論：多樣本合併不是主因，主因是晶格估計不支援交錯排列與陣列中間的大片空區。
- R07.2：完成（使用者決定改晶格估計）。晶格的索引區域從外接矩形改成三個條件的交集（索引的外接矩形 ∩ 影像空間裡沿 u 的外接矩形 ∩ 有兩側證據的空格），規則 2 用「證據數」（內點 ＋ 有證據的空格 ＋ 貼著內點的空格）。Rust（`region.rs`）與 legacy C++（`lattice_region.h/.cpp`）兩份、運算順序相同；`LatticeModel::cellCount()` 的定義搬到 `lattice_region.cpp`（兩種建置都編）；`LatticePredictor::holes` 只補區域裡的；C ABI 版本 5（`aoi_rs_lattice_holes_are_backed` 要讀 cells）。設計、代價與真實影像的結果：ADR-013 的「後續（2026-10-28，R07.2）」。
- R07.3：完成。`tst_lattice` 加三個交錯陣列的測試、`aSparseSecondRowNeedsEnoughEvidence` 的「找到 3 個」改成補 5 格（只補有證據的）；`LatticeTestSupport.h` 的 `compare` 用 `holeIsBacked` 判斷哪些缺件本來就不預測；差異測試 `extraCases` 加 12 種交錯情境；Rust `region.rs` 5 個單元測試；說明頁的資料重新產生；`docs/steps/S05-lattice/spec.md` 的失敗情況表加 #14、#15；`perf_multi_sample_real` 的 after 數字寫進 `plan.md`。

### 修改的檔案
- rust/aoi_domain_rs/src/lattice/region.rs（新增）、mod.rs、model.rs、predictor.rs、acceptance.rs、estimator.rs；rust/aoi_domain_rs/src/ffi.rs、ffi_lattice.rs
- src/domain/lattice/lattice_region.h、lattice_region.cpp（新增）、lattice_model.h、lattice_acceptance_rs.cpp、legacy/lattice_predictor_legacy.cpp、legacy/lattice_acceptance_legacy.cpp；src/domain/ffi/aoi_domain_rs.h
- AiInspTool/src/domain/domain.pro（加 `lattice_region.cpp`）、AiInspTool/tests/tests.pro（`AOI_PERF_TESTS` 加 `backend/perf_multi_sample_real`）、AiInspTool/tests/backend/perf_multi_sample_real.pro（新增）
- tests/backend/perf_multi_sample_real.cpp（新增）；tests/domain/tst_lattice.cpp、tst_domain_rs_diff.cpp、LatticeTestSupport.h；tests/core/tst_generate_objects.cpp（假世界的標準答案用同一條證據規則：`Oracle::backed`／`predicted`）、tests/ui/tst_presenter.cpp（提案的格數 44 → 37：沒有證據的缺件與弱零件不補）
- docs/adr/013-lattice-acceptance-rules.md（後續）、docs/steps/S05-lattice/spec.md、docs/steps/S05-lattice/explainer.html（資料重新產生）、docs/architecture.md、docs/plans/INDEX.md、docs/plans/active/R07-multi-sample-merge/

### 驗證
- `cargo fmt --check`、`cargo clippy --all-targets -- -D warnings`、`cargo test`（46 個）通過。
- `test.ps1 -NoBuild -Filter 'tst_lattice*'`（Release、Rust）：6 個全部通過（`tst_lattice` 72 項、`tst_lattice_scenarios` 1 031 項、說明頁的資料重產後 `tst_lattice_explainer` 通過）；`tst_domain_rs_diff` 通過（1 個項目跳過：原本就有）；`perf_multi_sample_real` 兩個 back end 都跑過（after 的數字在 `plan.md`）。
- 完整的 `test.ps1`（Release）：`-DomainImpl Cpp -NoBuild`（`build/qmake-release-cpp`，`jom /K` 建置）全部通過（只有原本就有的 2 個 XFAIL：`tst_contract_end_to_end` OpenCV 的 B1、`tst_image_sources_agree` 的 palette BMP）；Rust 預設（`build/qmake-release`）`-NoBuild` 全部通過（同樣的 2 個 XFAIL；7 個測試有原本就有的跳過項目）。`tst_generate_objects` 與 `tst_presenter` 原本因為補漏的數量變了而失敗，標準答案改用同一條證據規則後通過。
- 建置用 `jom /K`：Codex 的 `tests/ui/tst_solder_teaching.cpp`（R08，未提交）編不過，不是本計畫的檔案。
- `node tools/ste-lint.mjs` 0 錯誤；`node tools/check-deps.mjs` 0 違規。

### Commit
- `38e94de` [Caspar]fix: 晶格的索引區域不再是外接矩形，交錯陣列與長槽的板子能補漏（分支 `fix/r07-multi-sample`，本機，沒有合併、沒有 push）。

### 注意事項
- 第一版的區域只有條件 1、2（容差 1e-6），1 029 種情境裡 14 種缺角的格子被排除；容差改成四分之一個最短格距之後通過。只有條件 1、2 時，真實影像上長槽裡 288 個「缺格」全部補出來（116 個推算框、172 個低分候選）——就是使用者看到的垃圾；條件 3（兩側證據）解決。條件 3 只用在 `cellCount`／`holes` 時，規則 2 擋不住稀疏的假晶格（`randomPointsPredictAtMostAFlaggedHole`、掃描裡離群點湊出的晶格），所以規則 2 多算貼著內點的空格（證據數）。
- `build/clip/`（影片的影格、縮圖、裁切工具）不入版控，是這個 session 看影片與挑樣本座標用的。
