# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-14

## 正在做

無。**選取資訊表單與樣本庫面板（ADR-025 的「後續（2026-10-14）」）完成**：分支 `feat/selection-info-panel`（從 `main` 600c757 開；兩個 commit：程式與測試、文件），這個 session 在 worktree 執行，`main` 在主 checkout（`D:\AOI_Project\Qt\AutoCadCreated`）上簽出，所以用 `git -C <主 checkout> merge --no-ff` 合併（合併前確認主 checkout 乾淨），沒有 tag、沒有 push。**接下來仍是 S12**（Start prompt 在 `docs/plan/S12-release.md` 底部），或使用者指定的 P2（完整流程 B 與樣本庫檔案，另開 session）。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、ADR-025 的後續一節、`docs/current-status.md` 的「選取資訊表單與樣本庫面板」）

- 使用者回報：編輯只能拉框，要能用數字改樣本的長寬、讓幾個樣本一致；建立樣本時要看得到樣本的圖。做法由使用者確認。
- application：`InspectionDocument::setBodySizeForTypes`（新 Command `ResizeTypes`）、`InspectionSession::selectionInfo`（`SelectionInfo`：能不能改位置、改大小的規則在這裡）、`resizeBodyKeepingCenter`、`applyBodySizeToSamples`；`ProposalSession::poseOf`。
- presentation：`ObjectsPanel` 多選取資訊的表單（`ISelectionInfoView`；`editingFinished` 才送出、值沒變不送）、新 `library/SamplePanel`（`ISampleListView`，放進 `libraryDock`）、新 `presenters/SampleListBuilder`（縮圖與快取）、`AutoRoiPresenter` 實作新的 `ISelectionEditController`、`MainWindow` 多 `samplePanel()` 與接線。`copy.json` 新增 20 個 key。

## 建置與測試

- `test.ps1` 預設 Release：**57／57**（HALCON 0 跳過；有跳過項目的 5 個測試與 2 個 XFAIL 和改之前相同）。
- `test.ps1 -Config Debug`：**57／57**（`tst_presenter` 82 passed、1 skipped ＝ 截圖那一項）。
- `node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：220 files、0 violations。
- `-Perf` 與基準線**沒有跑**（上一個 session 已經要重建；這次 `publishSelection` 多了 `selectionInfo()`（單一選取 O(1)）與樣本清單的快取判斷）：下一個動效能的 session 先 `perf.ps1 -Runs 3 -WriteBaseline`。

## 不要動

- `SampleListCache`／`DocumentSceneCache` 的鍵（文件的修訂號；改板子內容一律做成 Command）。
- 表單「Enter 或離開欄位才生效、值沒變不送出」（ADR-002 規則 7）；預覽中不能改大小（ADR-025 §4）。
- `ResizeTypes` 略過已經一樣的類型、全部一樣不進堆疊（`tst_document`、`tst_session` 鎖住）。
- 樣本庫 dock 的篩選欄仍是停用的佔位；不讀寫樣本庫檔案（P2）。
- 上一次交接的清單照舊：`mergeSampleProposals` 的規則、`accept` 的整批換掉、`RoiLayerItem` 的字形路徑、`HalconSearchOptions`／`OpenCvSearchOptions` 的預設、`ContractTolerances.h`、opencv B1 的 `QEXPECT_FAIL`、`tokens.json`、`copy.json` 只新增 key、`budgets.json` 的 limit、`ai-project-template/`、`third_party/halcon/`、fixture JSON、`PresenterRig.h` 的視窗閘門、晶格的門檻與 ADR-013、port 契約、`perf/baseline/*.json` 只用 `-WriteBaseline` 重建。

## 已知問題與風險

- 表單的角度只顯示；只改這一個元件（實例覆寫）仍延後；子框仍只能 `Ctrl+Z` 拿掉。
- 樣本縮圖的框用固定色（藍＝主體、橙＝子框），不走畫布的主題。
- 等使用者決定的事沒有變（ADR-004、S09、S10、待決定（10））。

## 驗證（實際執行）

- `test.ps1`（Release）57／57、`test.ps1 -Config Debug` 57／57、`ste-lint` 0 errors、`check-deps` 0 violations；`-Perf` 沒有跑。
