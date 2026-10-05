# S08 — HALCON back end

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構 |
| 分支 | `step/08-halcon` |
| 依賴 | S03、S05、S06、S07（用 S07 當對照基準） |
| 產出位置 | `src/infrastructure/backend_halcon/`、`cmake/FindHalconLocal.cmake`、`tests/backend/`、`docs/steps/S08-halcon/` |

> **S01 修訂（2026-10-05）**：照 ADR-006：HALCON 版 `ILibraryMatcher`。預期每個庫條目建一個 shape model 再一次找多個（**未驗證**，這一步確認）。T7 補 HALCON 的實測。

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
