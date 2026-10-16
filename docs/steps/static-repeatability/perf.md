# 靜態重複性驗證與效能

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-16 | Codex (GPT-6.1-sol) | 補入建置、真實 CSV、原生畫面及效能實測結果 |

## 預算

既有 `perf/budgets.json` 沒有重複性分析的指標，本次不新增或放寬預算，也不重建既有基準線。新增量測只報告原始結果，不宣稱達到未定義的效能標準。

## 環境

Windows 11 23H2（22631）、Intel Core i7-1360P、16 logical cores、63.6 GB RAM；Qt 6.12.0、MSVC 19.44.35229、Release、offscreen。工作狀態為 `5f8959c+uncommitted`。

## 建置與測試

- `scripts/dev/env.ps1` 後前置 `VCToolsInstallDir/bin/Hostx64/x64`；`jom` Release／Debug 最終增量建置均成功。
- `scripts/dev/test.ps1 -NoBuild`：61／61 通過，含新功能兩個執行檔。五個既有執行檔含 skip；HALCON 測試有實際執行。
- 既有兩個 XFAIL 保留：`tst_contract_end_to_end` 的 opencv B1 low contrast、`tst_image_sources_agree` 的 palette BMP。沒有新增 XFAIL。
- 第一輪 `scripts/dev/test.ps1 -NoBuild -Config Debug`：59／61；`tst_cli::severalProgramsCanUseHalconAtTheSameTime` 失敗，`tst_presenter` 忙碌狀態等待失敗且整組逾時。後者是既有紀錄中的時間敏感案例；本次未調整它的時間或斷言。建置結束後完整重跑為 60／61（結束碼1）；CLI恢復通過，Presenter為85 passed、1 failed、1 skipped，155.248秒；唯一失敗仍是相同2秒Busy等待。新增折線功能之後的建置與測試另見下方補驗紀錄。
- `tst_repeatability`：Release／Debug 均 8 passed、0 failed、0 skipped；包含使用者五份 CSV（703 Box），逐一核對四個指標的平均、樣本標準差、P/T；CO 有 93 個 Box 超過 20%。
- `tst_repeatability_ui`：Release／Debug 均 5 passed、0 failed、0 skipped；驗證一份全圖、二至四份各一圖、最大四份、SPEC 加倍後 P/T 減半，以及失敗保留舊資料。
- `node tools/check-deps.mjs`：249 files、0 violations；`node tools/ste-lint.mjs`：984 sentences、0 errors、0 warnings；`node tools/check-contrast.mjs`：0 failures。
- `scripts/ai/check-ai-structure.ps1`：OK；`current-status.md` 超過 80 行的警告仍在，沒有為本功能重排歷史。

## 效能結果

執行 `scripts/dev/test.ps1 -NoBuild -Perf -Filter perf_repeatability`，1／1 通過（Qt Test 4 passed）。`RS-ANALYZE-4x30x703` 用可重現的合成資料：4 組 × 30 次 × 703 Box × 4 指標；暖機 5 次、正式 30 次，量測計算及資料對齊，不包含磁碟讀取與繪圖。

| 指標 | 中位數 | p95 | 最小 | 最大 |
|---|---:|---:|---:|---:|
| RS-ANALYZE-4x30x703 | 951.503 ms | 1754.735 ms | 530.153 ms | 2079.164 ms |

原始結果：`perf/results/repeatability_20261016-052239.json`（本機產物，不入版控）。沒有本功能預算或基準線，這組數字只作資訊用途。沒有執行整套既有效能閘門或關閉 back end 的建置矩陣。

## 畫面驗證

`perf_repeatability::nativeScreenshots` 以真實五份 CSV 產生並人工核對 `build/repeatability-screenshots/` 的 `repeatability-single.png`、`repeatability-settings.png`、`repeatability-four-copies.png`。單一資料夾的四個指標同時顯示；多份各有指標 ComboBox；設定表的 SPEC、倍率及門檻可以編輯。後三份資料夾是同一份 CSV 的暫存複本，只驗證版面，不能當成四次獨立實驗。

## 建置快取處理

qmake 曾引用 source-side 舊 `.moc`，使 `tst_tools` 仍指向已改名的測試。核對為 Qt 產生檔後，將 66 份快取備份至 `build/repeatability-cache-backup`，再重產兩種 build 的 moc 與 qmake 依賴。沒有修改既有測試原始碼。最後建置已包含 CSV 空白列與圖表標題等修正。

## 追加：資料夾折線圖與指定 Box

使用者選擇橫軸為資料夾，各組摘要連線，並可輸入單一 Box 名稱。新增獨立 Designer 視窗，以指標 ComboBox 同時控制 P/T、Range；摘要沿用共同完整 Box 的最大值，指定框時只查該框。未知、同名未消歧、缺少或不完整資料不補零。

- 最新完整 Release 回歸仍為61／61；Debug完整回歸的60／61限制如上。追加功能後以兩種組態的應用程式與受影響測試目標建置；未重編譯未修改的全部測試。
- `test.ps1 -NoBuild -Filter 'tst_repeatability*'` 與 `-Config Debug` 均2／2；計算8 passed、UI5 passed，包含folderSummaryAndSelectedBoxShowBothStatistics。
- 兩份新增測試的source-side舊moc使新slot沒有執行；核對為Qt產生檔後另備份至build/repeatability-cache-backup/new-feature-moc，重產這兩個專案的qmake及moc依賴，確認兩種組態的meta-object清單確實含新slot。沒有修改既有測試條件。
- `perf_repeatability.exe nativeScreenshots` 只重跑畫面驗證，沒有重跑或改寫上述計算效能量測。新增repeatability-folder-lines-summary.png與repeatability-folder-lines-box.png；同資料複本的點相同是預期結果。
- 比較圖持有開啟時的不可變快照；目前主視窗PNG匯出不包含這個獨立比較視窗。
