# Agent Handoff

- 最後一位 agent：Codex (GPT-6.1-sol)
- 日期：2026-10-16
- 分支：codex/static-repeatability；使用者於2026-10-16明確授權commit並push到origin同名分支。

## 本次交付

- 使用者核准把 Excel 靜態重複性整合到 Qt；入口為〔檢視 › 靜態重複性分析…〕（ADR-029）。
- 一個資料夾顯示全部勾選指標；兩至四份同時並排，每份以 ComboBox 選一圖；比較表顯示勾選指標。
- 樣本標準差 n−1、Range、P/T＝6s/SPEC；按 Component／Box 對齊，只對共同且完整的 Box 作摘要比較。
- 指標設定在載入前可開啟，能改單位／倍率／SPEC／P/T 門檻，6 固定。CO350、XO250、YO250，其餘已知指標200 µm。
- 追加〔跨資料夾折線圖…〕：資料夾為橫軸，下拉選指標，同時看P/T、Range；Box留空為共同完整Box最大值摘要，輸入Box或Component/Box只看該框，缺值不補零。
- CSV 只讀 BOX LEVEL，忽略 Decision；新數值欄位自動加入，沒有 SPEC 時 P/T 不可用。
- 背景載入與計算；失敗或取消保留舊快照；JSON 保存原始量測及指標定義；CSV／PNG 匯出。
- 原始 Excel／五份 CSV 沒有修改。完整規格、前後流程、互動說明及分鏡在 docs/steps/static-repeatability/。

## 實際驗證

- Release／Debug 最終建置成功（Qt6.12.0、MSVC19.44、qmake＋jom）。
- Release：scripts/dev/test.ps1 -NoBuild，61／61 通過。
- Debug 首輪59／61；CLI HALCON 多程式案例失敗，Presenter 忙碌等待失敗且整組300秒逾時。完整重跑60／61，CLI恢復通過，Presenter85 passed／1 failed／1 skipped（155.248秒），唯一失敗仍為2秒Busy等待。
- 新功能兩種組態均通過：tst_repeatability 8、tst_repeatability_ui 5（含追加指定Box案例）；含真實五份 CSV、703 Box、CO93個超過20%，及 SPEC 加倍後 P/T 減半。
- 既有兩列 XFAIL：contract_end_to_end 的 opencv B1、image_sources_agree 的 palette BMP；沒有新增。
- check-deps 249 files／0 violations；ste-lint 984 sentences／0 errors、warnings；contrast 0 failures；git diff --check 通過。
- AI 結構 OK，current-status.md 超過80行的警告保留。
- perf_repeatability 1／1，4×30×703×4 計算中位數951.503ms／p95 1754.735ms；資訊量測，無預算、沒有重建基準線。
- 人工核對 build/repeatability-screenshots 的 single／settings／four-copies及folder-lines摘要／單Box PNG；後三組是同資料暫存複本，只驗證版面。
- 追加折線功能後兩種組態的程式及受影響測試建置成功；兩種組態功能測試各2／2。Release完整回歸再跑61／61；最後版面調整後只補跑受影響測試與畫面。
- 詳細指令、環境、原始結果及限制在 docs/steps/static-repeatability/perf.md；沒有跑 back end 關閉矩陣或整套既有效能閘門。

## 建置注意

- 沙箱 env.ps1 曾選到MSVC19.00；建置使用本機權限並前置 VCToolsInstallDir/bin/Hostx64/x64，確認19.44。
- source-side 66份舊.moc污染qmake依賴，已確認全為Qt產生檔，備份至 build/repeatability-cache-backup；重產兩種build的moc及qmake依賴。不要回填舊快取。
- 兩份新功能source-side舊moc另備份至build/repeatability-cache-backup/new-feature-moc；只重產對應qmake／moc，確認UI新slot實際執行。
- AOI_REPEATABILITY_DATA 指向 D:/Benano/20261014_20261014_4677_AdvancePin_Proj，可執行真實資料測試。
- AOI_REPEATABILITY_SCREENSHOTS 設定輸出目錄，可由 perf_repeatability 的 nativeScreenshots 重產畫面。
- build/repeatability-*-tests.log、build/repeatability-debug-final.log 保留測試摘要；個別Qt log在 build/qmake-*/test-logs/。

## 本次提交範圍

- 全部是本次功能：domain／application／storage／presentation模組，MainWindow與main入口，qmake清單，三個測試執行檔及功能／架構／交接文件。
- 沒有整理來源不明的使用者變更；不提交build、perf/results或原始量測。

## 不要動與界線

- 不接續S12；ADR-028整框契約、ContractTolerances、fixture JSON及ADR-026／027不動。
- copy.json只新增key；效能預算與基準線要等使用者指示才重建。
- BENANO BOX LEVEL格式；新衍生公式需新增計算規則，不執行任意腳本。
- JSON不保存暫時勾選、ComboBox及比較基準；多檔匯出不是交易，部分失敗可能已有檔案寫出。
- 既有Presenter Debug時間敏感案例沒有修改；其他待決定事項仍在docs/current-status.md。
