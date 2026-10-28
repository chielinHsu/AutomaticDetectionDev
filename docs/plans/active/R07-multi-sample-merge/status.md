# R07 狀態

> 快照，不是歷史：只寫「現在」，有實質進度時原地修改（歷史在同資料夾的 `changelog.md`，細項的狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-28
- 狀態：進行中（R07.1–R07.3 完成；R07.4 待使用者決定要不要做）
- 分支：`fix/r07-multi-sample`

## 目前細項

- 無進行中的細項。R07.2、R07.3 完成，等使用者在程式裡確認（HALCON、同一張板子、幾個樣本）。

## 阻礙

- 工作目錄裡同時有 Codex 的 R08（Socket 教學）未提交的修改；它的 `tests/ui/tst_solder_teaching.cpp` 目前編不過（`QCOMPARE` 一個沒有 `toString` 的列舉），`test.ps1` 的 jom 會在那裡停下。本計畫的驗證用 `jom /K` 建完其餘目標再 `-NoBuild` 跑。提交時只 stage 本計畫的檔案。

## 待使用者決定

- R07.4（原本要改的合併規則：驗證下限相對於種子分數、同類型優先、同一種零件的樣本共用晶格）要不要做：after 的數字顯示 3 個樣本沒有比 1 個差，建議先不做。
- HALCON 的召回（同一張板子 210／約 360 個端子，OpenCV 360）要不要另開計畫調搜尋器的參數。

## 下一步

1. 使用者用程式驗證（HALCON／OpenCV，3 個樣本）。
2. 依使用者的回饋決定 R07.4 與 HALCON 召回的計畫；完成後把資料夾搬到 `completed/`。

## 接續資訊

- 使用者的影像：`D:\Benano\20261007_4677_AdvancePin_Proj\FovImages\0_0_HA.tif`（5314×4594、16 bit 容器、值到 4095；同一張在 `20261014_4677_AdvancePin_Proj`、`Skew_Proj`、`AdvancePinInspect\4_Pin_59_29` 也有）。板子是密排端子：每格一塊大的白色焊墊加一根亮的端子（約 45×25 px），交錯排列（u ＝ (0, 154)、v ＝ (133, 77)；同一列相鄰的端子差 266），兩個深色的長槽，右側兩個區塊是鏡像的（搜尋不到是對的）。
- 量測工具：`AOI_REAL_IMAGE_DIR` 指到那個資料夾，`test.ps1 -Perf -Filter perf_multi_sample_real`；疊圖在 `build/qmake-release/test-work/multi-0_0_HA-<back end>-<n>.png`，晶格的診斷印在 log 的 `lattice[...]` 列。before／after 的數字在 `plan.md`。
- 晶格區域的規則與代價：ADR-013 的「後續（2026-10-28，R07.2）」；說明頁 `docs/steps/S05-lattice/explainer.html` 的資料已重新產生（150 種情境 11 種變了，都在範圍外）。
