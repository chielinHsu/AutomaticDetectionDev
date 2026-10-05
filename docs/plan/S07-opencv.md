# S07 — OpenCV back end

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構 |
| 分支 | `step/07-opencv` |
| 依賴 | S03、S05、S06 |
| 產出位置 | `src/infrastructure/backend_opencv/`、`cmake/FindOpenCVLocal.cmake`、`tests/backend/`、`docs/steps/S07-opencv/` |

> **S01 修訂（2026-10-05）**：照 ADR-006：要實作 `ILibraryMatcher`（區域＋樣本庫 → 元件候選、位姿、分數、候選類型的分數）；成本預期隨庫大小 N 成長（**假設**，要實測）。新預算 T7（暫稱）在這一步實測後提案。

> **S03 完成後的提醒（2026-10-05）**：新增 back end ＝ adapter ＋ 在 `src/app/CompositionRoot.cpp` 把 `registerUnavailable("opencv", …)` 換成 `registerBackend`；不要改 `application/`、`domain/`（`check-deps` 會擋）。`ITemplateBuilder`、`IPatternSearcher` 的契約（排序、視窗限制、取消、可重入、不丟例外）寫在標頭；`tests/core/tst_fake_backend.cpp` 是假 back end 版本的契約測試，`tst_backend_registry` 的 `ScriptedBackend` 示範怎麼放進新的 back end。OpenCV 只能出現在 `infrastructure/backend_opencv`（`check-deps` 的規則表已有這一列）；`AiInspTool.pro` 目前不納入它。ADR-010 的座標與角度轉換（像素中心偏移 0.5、`matchTemplate` 回傳左上角整數座標）只是文件上的假設，要實測。

> **S04 完成後的提醒（2026-10-06）**：（1）流程 B 的接口：`AutoRoiPresenter::finishRegionScan()` 現在直接回「樣本庫是空的」（庫與比對器都還沒有實作）。S07 做好 `ILibraryMatcher` 後，把它換成真的呼叫（走 `JobRunner`，和流程 A 同一條路）；狀態機、取消、忙碌提示、結果卡都已經就位，不必改。（2）`Stage::Match` 的進度已經對應到進度列（`ProgressBridge`，整段 0..1）。（3）執行中切換 back end 的下拉選單仍是停用的（`MainWindow::setBackends` 只填清單）；`--backend` 命令列是唯一的切換方式。（4）Presenter 的測試用 `tests/ui/PresenterRig.h` 的假 back end（可設成失敗、可卡住視窗驗證階段）；新的 back end 不需要動它。

<!--STE-->
## Purpose
This step builds the OpenCV back end. It builds the template and searches the image for similar parts.

## Reason
OpenCV is free and gives us the first working product. It also gives us a speed and accuracy baseline for HALCON.

## Effect
The whole program works from the sample box to the inspection rectangles. We have the first real numbers for search time.
<!--/STE-->

**繁中**：做出第一個真正能用的 back end，整條流程（框樣本 → 檢測框）第一次跑通，並取得第一組真實的搜尋時間與準確度。

## 任務

1. **CMake**：`OpenCV_DIR=D:\opencv\opencv\build`（`vc16`）；Release/Debug 對應的 DLL 複製規則；`AOI_WITH_OPENCV=ON`。
2. **`OpenCvTemplateBuilder`**：裁切樣本、（選用）正規化；依大小自動決定金字塔層數；預先建立旋轉範本集合（角度範圍與步進來自 `SearchParams`）。
3. **`OpenCvPatternSearcher`**：
   - 由粗到細：頂層金字塔 `matchTemplate(TM_CCOEFF_NORMED)` → 往下層在候選附近精修。
   - 角度掃描（自適應步進）；**支援「視窗清單」搜尋區域**（S05 漸進驗證要用）。
   - 候選用 domain 的 NMS；子像素以拋物線插值。
4. **執行緒政策**：`cv::setNumThreads` 與 `JobRunner` 的 worker 數協調，避免超額訂閱；介面執行緒保留一顆。
5. **接進 use case**：以真實 searcher 取代 Fake，端到端跑通合成板子。
6. **量測與校準**：T1、T2a、T4、T6；各階段切片；峰值記憶體。**用實測校準 `perf/budgets.json` 的搜尋類預算**（status 對應列改為已校準）。
7. **已知限制寫入文件**：強度相關法對光照變化、反光、縮放敏感；大角度範圍成本高。

## SOLID 重點

- **L**：此 adapter 必須通過 S09 的契約測試（先在本步驟寫好測試草稿）。
- **O**：加入 OpenCV 不修改 application/domain（PR 差異可驗證）。
- **S**：範本建立與搜尋分兩個類別。

## 效能檢查（寫入 `perf.md`）

T1、T2a、T4、T6a/T6b；階段切片（範本／種子／擬合／驗證）；峰值記憶體；角度範圍對時間的影響曲線。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 由粗到細與角度掃描的 Purpose/Reason/Effect；已知限制 |
| 2 | diagram.md | OpenCV 搜尋管線圖（金字塔→候選→精修→NMS）；序列圖 |
| 3 | explainer.html | 互動頁：調角度範圍/樣本大小/影像大小，看**實測**時間曲線（嵌入本步驟結果 JSON） |
| 4 | storyboard.md | 75 秒「由粗到細：為什麼先看小圖」 |

## 完成定義

- [ ] 合成板子端到端成功，召回率與位置誤差達標（或明確記載差距）
- [ ] T1、T2a、T4、T6 已量測；預算已校準
- [ ] 沒有修改 `application/`、`domain/`（除了 bug 修正，需說明）
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(opencv): add OpenCV template builder and pattern searcher

- Coarse-to-fine NCC search with angle sweep and window-list regions
- Thread policy shared with the job runner
- End-to-end run on synthetic boards; T1, T2a, T4, T6 measured and budgets calibrated
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S07-opencv.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S07（分支 step/07-opencv）。只做 S07。
開始前請向使用者要 1–2 張去識別化的真實板子影像（沒有的話先用合成影像並註明）。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S08 開新 session。
```
