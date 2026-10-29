# R07 狀態

> 快照，不是歷史：只寫「現在」，有實質進度時原地修改（歷史在同資料夾的 `changelog.md`，細項的狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-29
- 狀態：✅ 完成（R07.1–R07.3、R07.5–R07.9 完成；R07.4 取消）
- 分支：`fix/r07-multi-sample`，2026-10-29 以 `--no-ff` 合併回 `main`（本機，沒有 push）

## 目前細項

- 無。計畫完成，資料夾搬到 `completed/`。

## 阻礙

- 無。工作目錄裡 Codex 的 R08（Socket 教學）未提交修改與本計畫無關，沒有動；`tasks/regression-repair.md`（Codex 的 R07.5–R07.7 診斷）隨資料夾搬到這裡，仍未提交。

## 待使用者決定（計畫之外的後續）

- HALCON 的召回（LeftTopRegion 整框 855／1068 ≥ 0.70，點端子的遮罩 1059）：要不要在畫面上建議 HALCON 用 `M` 點端子的遮罩、或另開計畫調搜尋器。
- 產品方向：通用的樣本比對（主視窗）與 Codex 的 Socket 結構偵測（R08）要並存還是整合（主視窗接受結構偵測器當另一種來源）；問題 3（影片裡不用參數的檢測）是不是最終目標。
- 問題 2（右側 dock 重疊）還沒開計畫。

## 接續資訊

- 使用者的影像有好幾張同名的 `0_0_HA.tif`：R07.1–R07.3 的 before／after 用 `D:\Benano\20261007_4677_AdvancePin_Proj\FovImages\`（兩條橫的長槽），R07.8／R07.9 的量測用 `D:\Benano\20261014_4677_AdvancePin_LeftTopRegion_Proj\FovImages\`（整張端子、右下角一個長槽）。都是 5314×4594、16 bit 容器、值到 4095；端子交錯排列（u ＝ (0, 154)、v ＝ (133, 77)，LeftTopRegion 擬合出的基底是對角線 (133, ∓77)）。
- 量測工具：`AOI_REAL_IMAGE_DIR`、`AOI_MULTI_SAMPLE="x,y,w,h"`、`AOI_MULTI_MATCH`（矩形或 `mask:cx,cy[,grow][,edge]`）、`AOI_MULTI_DUMP`（CSV），`build\qmake-release\bin\perf_multi_sample_real.exe multiSample:halcon -o log.txt,txt`；log 的 `holes N (unverified a, unsupported b)` 是沒驗證到、鄰居不夠的缺格數。數字在 `plan.md`、ADR-035。
- 主視窗沒有儲存配方的功能（〔儲存〕停用）；要重現使用者的樣本，從「選取資訊」表單抄中心與大小。
- 規則的出處：晶格區域 ADR-013 的「後續（R07.2）」；補漏的回報 ADR-035（R07.8 沒驗證的不回報、R07.9 低分的要有 4 個鄰居）。
