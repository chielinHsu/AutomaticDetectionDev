# R07 狀態

> 快照，不是歷史：只寫「現在」，有實質進度時原地修改（歷史在同資料夾的 `changelog.md`，細項的狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-29
- 狀態：進行中（R07.1–R07.3、R07.5–R07.8 完成；R07.4 仍待使用者決定）
- 分支：`fix/r07-multi-sample`（比 `main` 多 R07.2／R07.3 的 `38e94de`、`970ac5f`，與 R07.8 的提交；沒有合併、沒有 push）

## 目前細項

- 無進行中的細項。R07.8（影像上驗證不到的推算格預設不回報，ADR-035）已實作；使用者還沒在程式裡看過 R07.8 之後的畫面。

## 阻礙

- 工作目錄同時有 Codex 的 R08（Socket 教學）未提交修改，建置會把它編進去；它進行中時 `AiInspTool.exe` 曾連結失敗（幾分鐘後自己好）。本計畫的提交只 stage 自己的檔案。
- R07.5–R07.7（Codex）的 `tests/domain/tst_lattice.cpp` 與文件仍未提交（使用者當時要求不 commit）。

## 待使用者決定

- R07.4（原本要改的合併規則：驗證下限相對於種子分數、同類型優先、同一種零件的樣本共用晶格）要不要做：after 的數字顯示 3 個樣本沒有比 1 個差，建議先不做。
- HALCON 的召回（2026-10-07 那張 210／約 360；LeftTopRegion 那張整框 855／1068 ≥ 0.70，點端子的遮罩 1059）要不要另開計畫調搜尋器的參數或預設用遮罩。
- 槽緣鋸齒 0.70／0.71 的「找到」：HALCON 用 `M` 點端子的遮罩就分得開（0.48）；OpenCV 沒有好辦法（遮罩反而多誤報）。要不要在畫面上建議 HALCON 用遮罩。
- `fix/r07-multi-sample` 何時合併回 `main`（`--no-ff`、只在本機）。

## 下一步

1. 使用者在程式裡看 R07.8 之後的結果（`build\qmake-release\bin\AiInspTool.exe`，或 Qt Creator 重新建置）：槽內不該再有「推算」框。
2. 依使用者的回饋決定 R07.4、HALCON 召回與合併；完成後把資料夾搬到 `completed/`。

## 接續資訊

- 使用者的影像有好幾張同名的 `0_0_HA.tif`：R07.1–R07.3 的 before／after 用 `D:\Benano\20261007_4677_AdvancePin_Proj\FovImages\`（兩條橫的長槽），使用者截圖與 R07.8 的量測用 `D:\Benano\20261014_4677_AdvancePin_LeftTopRegion_Proj\FovImages\`（整張端子、右下角一個長槽）。都是 5314×4594、16 bit 容器、值到 4095；端子交錯排列（u ＝ (0, 154)、v ＝ (133, 77)，LeftTopRegion 擬合出的基底是對角線 (133, ∓77)）。
- 量測工具：`AOI_REAL_IMAGE_DIR` 指到資料夾、`AOI_MULTI_SAMPLE="x,y,w,h"`、`AOI_MULTI_MATCH`（矩形或 `mask:cx,cy[,grow][,edge]`）、`AOI_MULTI_DUMP`（CSV），`build\qmake-release\bin\perf_multi_sample_real.exe multiSample:halcon -o log.txt,txt`（或 `test.ps1 -Perf -Filter perf_multi_sample_real`）；疊圖在執行目錄。數字在 `plan.md`。
- 主視窗沒有儲存配方的功能（〔儲存〕停用、沒有接線）；要重現使用者的樣本，從「選取資訊」表單抄中心與大小。
- 晶格區域的規則與代價：ADR-013 的「後續（2026-10-28，R07.2）」；推算格的回報：ADR-035。
