# S09 — 契約測試與 A/B 比較

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構（驗證互換性） |
| 分支 | `step/09-contract-ab` |
| 依賴 | S07、S08 |
| 產出位置 | `tests/contract/`、`src/presentation/compare/`、`docs/steps/S09-contract-ab/` |

> **S01 修訂（2026-10-05）**：契約測試要涵蓋 `ILibraryMatcher`：辨識正確率、候選排序、位姿誤差、庫大小 N 的行為（ADR-006）。

> **S03 完成後的提醒（2026-10-05）**：`tests/core/tst_fake_backend.cpp` 已經是「對假 back end 跑的契約測試」：把它的斷言改成以工廠參數化、對每個已編入的 back end 各跑一次（沒編入的標示 SKIPPED）。ADR-010 的角度慣例要用旋轉過的合成影像鎖住（目前只有 `tst_domain` 用數字鎖 domain 的數學）。

> **S06 完成後的提醒（2026-10-06）**：`IImageSource` 已經有 Liskov 的測試：`tests/infra/tst_image_sources_agree`（Qt 與 OpenCV，18 種檔案：16 種完全相同；已知例外：16 階灰色表的 BMP，`QEXPECT_FAIL`；灰階＋alpha 的 PNG 沒有驗證）。HALCON 加進來時把它改成以來源參數化。`IRecipeStore`、`ILibraryStore` 的契約測試在 `tests/infra/tst_recipe_store`、`tst_library_store`、`tst_library_layers`（只有 JSON 一個實作）。

> **S07 完成後的提醒（2026-10-06）**：（1）**契約測試的草稿**是 `tests/backend/tst_opencv_backend.cpp`（ITemplateBuilder／IPatternSearcher 的每一條契約，加上座標與角度的正負號）與 `tst_opencv_library_matcher.cpp`（ILibraryMatcher）；泛化時注意 OpenCV 與假 back end 的差別：OpenCV 搜尋的角度範圍要被範本的範圍涵蓋（否則 `InvalidArgument`），範本有一部分在影像外的元件找不到，平的樣本給 `sampleProblem` ＝ `LowContrast`。（2）A/B 的情境：`perf_opencv`（合成板子：T6 的 8 種情況、角度範圍、影像大小、樣本大小）與 `perf_opencv_real`（使用者的真實影像，`AOI_REAL_IMAGE_DIR`）。（3）真實影像上兩個值得比的現象：相似的別的東西（焊墊邊緣）的分數 0.70–0.85、在非矩形陣列的空槽裡補假缺格（S05 的已知失敗 #11）。（4）use case 的門檻（`seedMinScore` 0.70、預設的分數門檻 0.70）在真實影像上偏低：要不要改、改成多少，是使用者的決定（`docs/current-status.md`）。

> **S08 完成後的提醒（2026-10-07）**：（1）**HALCON 的契約草稿**是 `tests/backend/tst_halcon_backend.cpp`、`tst_halcon_end_to_end.cpp`、`tst_halcon_library_matcher.cpp`，和 OpenCV 版同一組情境；泛化時用 `REQUIRE_HALCON()`（`BackendTestSupport.h`）讓沒有授權的電腦跳過。兩邊**刻意不同的門檻**：HALCON 在強雜訊的板子上角度最大約 0.9°（`tst_halcon_end_to_end` 改用「框的角落位移 ≤ 0.5 px」）；0° 附近 ±0.5° 的元件有一部分回報接近 0°（HALCON 本身的行為）。（2）**A/B 的現成數字**：`perf_halcon` 在預設的建置（OpenCV＋HALCON）同一次量 HALCON 與 OpenCV（`CV-*`），`perf_halcon_real` 對使用者的真實影像兩邊並排；`docs/steps/S08-halcon/perf.md` 有並排表。合成板子上 HALCON 的種子搜尋較快、位置誤差 p95 較大（0.48 對 0.11 px；雜訊大時差最多，反光時 HALCON 較好）。**真實影像 HA 上 HALCON 用一根針腳當樣本只找到少數針腳**（`perf.md` §4，原因是推測），是 A/B 的重點之一。（3）**分數不是同一把尺**：HALCON 是邊緣方向的一致度，OpenCV 是 NCC；同一個 `minScore` 在兩邊意義不同，A/B 畫面要說明，門檻可能要依 back end 分開（使用者決定）。（4）流程 B：兩個 adapter 的「合併成元件」規則各抄一份（不能共用程式碼），契約測試要鎖住兩邊一致；HALCON 一次 `find_shape_models` 比每個類型分開快 1.1–2.0 倍（N ≥ 10）。（5）`tst_image_sources_agree` 沒有 HALCON 的欄位：**HALCON 的影像來源沒有做**（ADR-017，等使用者決定）。（6）ADR-010 的 HALCON 端已在 S08 實測並以測試鎖住（ADR-010 的「後續（S08）」）。

> **建置改 qmake 之後的提醒（2026-10-07，[ADR-018](../adr/018-qmake-only-build.md)）**：建置只用 qmake（`AiInspTool/AiInspTool.pro`；CMake、preset、CTest 都已移除，S02–S08 文件裡的那些指令是歷史紀錄，對照表在 ADR-018），執行檔叫 `AiInspTool.exe`（GUI 子系統），在 `<建置>/bin`。OpenCV 與 HALCON **預設編進去**（以前的 preset `release-halcon`）；關掉用 qmake 變數 `AOI_WITH_OPENCV=OFF`／`AOI_WITH_HALCON=OFF`。命令列：`scripts/dev/test.ps1`（建置＋測試；`-Config Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`）；Qt Creator 開 `AiInspTool/AiInspTool.pro` 也能建置、執行與跑測試。**新增測試**：在 `AiInspTool/tests/<資料夾>/` 加一個 `.pro`（照同資料夾的寫法：`TARGET`、需要時 `AOI_LINK_EXTRA`、`include(<資料夾>_tests.pri)`、`SOURCES`），再列進 `AiInspTool/tests/tests.pro` 的 `AOI_TESTS`（效能量測列進 `AOI_PERF_TESTS`；只有某個 back end 才有的放進對應的 `equals(AOI_WITH_…, ON)` 區塊）。**新增原始碼**要加進對應的 `.pro`（不會自動納入）。S09 之前還有一個 session B（授權提醒的卡片、執行中切換 back end、Designer 的 `.ui`），開工時先看 `.ai/handoff.md`。契約測試如果放在新的資料夾（例如 `tests/contract/`），照 `AiInspTool/tests/backend/backend_tests.pri` 寫一個 `contract_tests.pri`。「只有一個 back end」的建置用 `test.ps1 -NoOpenCv` 與 `-NoHalcon` 驗證；A/B 的效能數字用 `test.ps1 -Perf -Filter 'perf_halcon*'`。

<!--STE-->
## Purpose
This step proves that the two back ends are interchangeable. It also shows their differences on the screen.

## Reason
A common interface does not guarantee a common behavior. The coordinates, the angles and the scores can differ without a compile error.

## Effect
One test suite runs on each back end. The user can compare OpenCV and HALCON on the same sample.
<!--/STE-->

**繁中**：介面相同不代表行為相同。用同一組契約測試跑每個 back end（Liskov），並提供 A/B 對照畫面，讓差異看得見。

## 任務

1. **契約測試套件**（以 factory 參數化，對每個已編入的 back end 各跑一次）：
   - 座標原點與角度正負號慣例。
   - 分數範圍 0..1；結果依分數排序。
   - 空結果行為（找不到時回傳空集合，不是錯誤也不是當機）。
   - 搜尋區域限制確實有效（區域外不會有結果）。
   - 取消旗標在 `I4` 預算內生效。
   - 合成資料集上的容差（位置誤差、角度誤差、召回率）；**容差寫在一個地方並說明依據**。
2. **A/B 比較功能**：同一樣本同時跑兩個 back end；畫布疊圖（兩邊都找到＝綠、只有 A＝藍、只有 B＝橘；不只靠顏色，用不同標記形狀）；表格顯示各階段耗時與數量差異。
3. **差異紀錄（decision log）**：強度式 NCC 與邊緣式 shape model 的行為差異（對光照、反光、缺件的反應），記錄在 `docs/adr/004-backend-divergence.md`；哪些差異可接受、哪些是 bug。
4. **CI 友善**：沒有 HALCON 授權時契約測試跳過該 back end 並**明確標示 SKIPPED**，不是靜默通過。

## SOLID 重點

這一步就是 **L（Liskov）的驗證**；也驗證 O：兩個 back end 都沒有逼 application 層改動。

## 效能檢查（寫入 `perf.md`）

兩個 back end 在同一測試矩陣子集的並排：時間、準確度、峰值記憶體。A/B 疊圖的 UI 更新時間（U1 類）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 契約的 Purpose/Reason/Effect；每條契約寫出「違反時會發生什麼壞事」 |
| 2 | diagram.md | 契約測試的參數化結構圖；A/B 疊圖的資料流 |
| 3 | explainer.html | 互動頁：載入 A/B 結果 JSON，切換顯示、點一個差異看原因與兩邊分數 |
| 4 | storyboard.md | 75 秒「兩個 back end 為什麼可以互換」 |

## 完成定義

- [ ] 兩個 back end 都通過契約測試（或差異已記入 ADR-004 並有明確容差）
- [ ] A/B 比較畫面可用
- [ ] 無授權環境的測試輸出為 SKIPPED，不是 PASSED
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
test(contract): add back-end contract suite and A/B compare view

- Parameterized contract tests: conventions, ordering, empty result, region limits, cancel, tolerances
- A/B overlay and timing table in the canvas
- ADR-004 documents accepted differences between NCC and shape-model back ends
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S09-contract-ab.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S09（分支 step/09-contract-ab）。只做 S09。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S10 開新 session。
```
