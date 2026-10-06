# S08 — HALCON back end

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構 |
| 分支 | `step/08-halcon` |
| 依賴 | S03、S05、S06、S07（用 S07 當對照基準） |
| 產出位置 | `src/infrastructure/backend_halcon/`、`cmake/FindHalconLocal.cmake`、`tests/backend/`、`docs/steps/S08-halcon/` |

> **S01 修訂（2026-10-05）**：照 ADR-006：HALCON 版 `ILibraryMatcher`。預期每個庫條目建一個 shape model 再一次找多個（**未驗證**，這一步確認）。T7 補 HALCON 的實測。

> **S03 完成後的提醒（2026-10-05）**：新增 back end 的方式與 S07 相同（adapter ＋ 在 `CompositionRoot.cpp` 註冊）。HALCON 的角度是弧度、座標是 `(Row, Column)`：轉換放在 adapter，正負號與軸順序是 ADR-010 裡「未驗證」的部分，要實測並用 S09 的契約測試鎖住。這台機器只有 HALCON runtime（沒有標頭與程式庫），要先補裝開發元件。

> **S06 完成後的提醒（2026-10-06）**：（1）**`HImage` ⇄ `ImageView` 與 `HalconImageSource` 沒有做**：這台機器沒有 HALCON 的 C++ 開發檔，寫了無法編譯與驗證。預期（未驗證，請查 HALCON 運算子說明再做）：`Gray8`、`Gray16` 的緊密排列可以零複製（外部記憶體），彩色的 `Bgrx32`（交錯）要轉成 HALCON 的分平面，一次複製並呼叫 `countImageCopy`。`domain/image_buffer` 的 `ImageBuffer` 是緊密排列的（為了這一點）；子影像（`subView`）的 stride 比一列大，HALCON 可能要複製。（2）影像來源的契約（ADR-014）：只交出 `Gray8`／`Gray16`／`Bgrx32`、`significantBits` 用 `detectSignificantBits`、校正值 0、不套用 EXIF 旋轉、canRead 只看檔名；加進 `app/CompositionRoot::makeImageSource` 的 `ImageSourceChain` 時要決定順序（現在 Qt 在前、OpenCV 在後）。`tests/infra/tst_image_sources_agree` 是「兩個來源給出相同影像」的寫法，加上 HALCON 的那一欄。（3）S06 起用 Qt 6.12.0。

> **S07 完成後的提醒（2026-10-06）**：（1）**OpenCV back end 是對照基準**（`src/infrastructure/backend_opencv/`、ADR-016）：`tests/backend/` 的 `tst_opencv_backend`（契約、座標與角度正負號、格式、取消、可重入、複製次數）與 `tst_opencv_end_to_end`（組裝根的 back end ＋ use case，合成板子與標準答案）可以照抄成 HALCON 版；`BackendTestSupport.h` 有樣本框與標準答案的比對。T6b 要扣掉「樣本框中心與元件中心的固定偏移」（匹配的參考點是使用者框的中心，見 `tst_opencv_end_to_end` 的 `sampleOffset`）。（2）**用同一組情境量 HALCON**：`perf_opencv` 的 T1、T2a、T4、T6、T7、角度範圍／影像大小／樣本大小的曲線（建議 `perf_halcon` 照抄），數字在 `docs/steps/S07-opencv/perf.md`。（3）`ILibraryMatcher` 的 OpenCV 版：每個類型建一次範本（不快取）、合併規則（交集 ÷ 較小的框 > 0.5）、`noMatch` 是 `LibraryEmpty`（沒有比對任何樣本）或 `NoLibraryMatch`；`Backend` 結構沒有 `libraryMatcher` 欄位（S07 沒有改 application；流程 B 的 UI 是使用者決定之後再接）。（4）**有效位元數**：搜尋器拿到的 `Gray16` 帶 `significantBits`（推測或使用者的覆寫，`effectiveBits`）；HALCON 轉影像時要用它（使用者的影像是 12 bit、存成 16 bit 容器）。（5）執行緒：組裝根的 `app::workerThreads()`（核心數 − 1）是 JobRunner 與 OpenCV 共用的數字；HALCON 的平行化（執行緒數的系統參數）也應該用它。（6）**真實影像**（使用者提供、去識別化、**不入版控**；路徑在 `.ai/handoff.md`）：24.4 MP、12 bit 的 HA、LA 兩張。OpenCV 的發現：HA 的種子分數分兩群（≥ 0.85 是針腳，0.70–0.85 多半是焊墊的邊緣）、LA 的晶格在空槽裡補了假缺格（S05 的已知失敗 #11）。HALCON 的 shape model 在這兩件事上是否比較好，是 S09 A/B 的重點。

<!--STE-->
## Purpose
This step builds the HALCON back end. It uses the HALCON shape model to find similar parts.

## Reason
HALCON is an industry tool for this job. The shape model handles rotation and gives sub-pixel poses.
We must prove that it is better. We do not assume it.

## Effect
The user can switch between OpenCV and HALCON. We have real numbers that compare the two.
<!--/STE-->

**繁中**：用 HALCON shape model 做第二個 back end。「HALCON 比較準比較快」是假設，要用 S07 的數字對照後才能下結論。

## 任務

1. **CMake**：`%HALCONROOT%`（本機為 HALCON 24.11 Progress-Steady）、include/lib/DLL 路徑；`AOI_WITH_HALCON=ON`；**沒有 HALCON 時整個專案仍可建置**。
2. **`HalconTemplateBuilder`**：`create_shape_model`（參數 auto 為預設；暴露進階參數）；可選 `create_ncc_model` 當對照。
3. **`HalconPatternSearcher`**：`find_shape_model(s)`；搜尋區域用 `reduce_domain`／視窗清單；回傳 Row/Column/Angle/Score → 轉成 domain 的 `Pose2D`。
4. **座標與角度轉換**：驗證 HALCON 的像素座標原點與角度正負號，在 adapter 內轉成 S03 文件規定的慣例；**寫測試鎖住**（S09 的契約測試會共用）。
5. **授權與錯誤處理**：啟動時檢查授權；失敗時回傳 `Error::BackendUnavailable`，UI 停用該選項並顯示原因，**不當機**。
6. **執行緒**：`set_system('thread_num', …)` 與 worker 數協調；確認所用運算子在多執行緒下的可重入性（查 HALCON 運算子說明，**不憑記憶**）。
7. **量測**：T2b、T6；與 S07 同樣的測試矩陣子集，供 S09 比較。

## SOLID 重點

- **L**：HALCON adapter 與 OpenCV adapter 可互換——同一份契約測試。
- **O**：加入 HALCON 不修改 application/domain。
- **D**：HALCON 型別（`HImage`、`HShapeModel`）不得出現在 adapter 以外。

## 效能檢查（寫入 `perf.md`）

T2b、T6；階段切片；與 OpenCV 的並排表；授權檢查耗時。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | shape model 的 Purpose/Reason/Effect；授權與可用性策略 |
| 2 | diagram.md | HALCON 搜尋管線圖；back end 可用性決策流程圖（有授權/無授權/未編入） |
| 3 | explainer.html | 互動頁：邊緣 vs 強度的差異示意（調對比/光照看兩種方法的分數變化）；嵌入實測並排數字 |
| 4 | storyboard.md | 60 秒「兩種找法的差別」 |

## 完成定義

- [ ] 有 HALCON 時端到端成功；無 HALCON（或無授權）時建置與啟動都正常
- [ ] 座標/角度轉換有測試
- [ ] T2b、T6 已量測，並與 OpenCV 並排記錄
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(halcon): add HALCON shape-model template builder and pattern searcher

- CMake option AOI_WITH_HALCON; project still builds without HALCON
- Shape model search with region/window support and pose convention conversion
- License check with graceful BackendUnavailable error
- T2b and T6 measured; side-by-side table against OpenCV
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S08-halcon.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S08（分支 step/08-halcon）。只做 S08。
先確認本機 HALCON 授權可用；不可用就如實回報並先完成「無授權時的優雅降級」部分。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S09 開新 session。
```
