# R07 變更紀錄

> 只新增，新紀錄放最上方；只記本計畫的事。每個 agent session 一筆。

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
- 未提交（見本筆更新）。

### 注意事項
- 第一版的區域只有條件 1、2（容差 1e-6），1 029 種情境裡 14 種缺角的格子被排除；容差改成四分之一個最短格距之後通過。只有條件 1、2 時，真實影像上長槽裡 288 個「缺格」全部補出來（116 個推算框、172 個低分候選）——就是使用者看到的垃圾；條件 3（兩側證據）解決。條件 3 只用在 `cellCount`／`holes` 時，規則 2 擋不住稀疏的假晶格（`randomPointsPredictAtMostAFlaggedHole`、掃描裡離群點湊出的晶格），所以規則 2 多算貼著內點的空格（證據數）。
- `build/clip/`（影片的影格、縮圖、裁切工具）不入版控，是這個 session 看影片與挑樣本座標用的。
