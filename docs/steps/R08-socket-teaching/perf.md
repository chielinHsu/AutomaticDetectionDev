> 目前主要流程已改為 [區域 CAD 註冊](region-workflow.md)，見 [ADR-036](../../adr/036-socket-region-cad.md)。下列為原版歷史。

# R08 效能與驗證紀錄

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-28 | Codex (gpt-6.1-sol) | 補入實際建置、Socket smoke 與回歸結果。 |

- 機器：Intel Core i7-1360P，16 logical processors；Windows 11 10.0.22631 x64。
- 工具：MSVC 19.44.35229、Qt 6.12.0、OpenCV 4.9.0、Rust 1.99.1、jom 1.1.7。
- 影像：5314×4594、Gray16，原始值 52..4095；選區 (1100,120,900,550)。
- 最終預設：Otsu、亮物件、面積 40..3000 px²、外擴 2 px；28 個候選。
- 最終驗證截圖的 JobReport.runMs：6.0 ms；本次測試 log 的 UI 往返：86.1 ms。兩者涵蓋的階段不同，且都是單次 smoke，非 p95。
- 本步驟沒有量產節拍預算，未改 perf/budgets.json。不能由候選數或單次延遲推論準確率。

## 實際指令與結果

1. scripts/dev/build-socket-mvp.ps1 -BuildDir build/qmake-r08：Release 建置 exit 0。
2. scripts/dev/test.ps1 -BuildDir build/qmake-r08 -NoBuild -Filter tst_solder_teaching：exit 0，10 passed、0 failed、0 skipped。
3. Debug＋AOI_WITH_OPENCV=OFF＋AOI_WITH_HALCON=OFF：qmake／jom exit 0；最終來源增量重建 exit 0。
4. scripts/dev/test.ps1 -BuildDir build/qmake-r08 -NoBuild：最終 63/67；失敗：tst_generate_objects、tst_lattice、tst_presenter、tst_domain_rs_diff。狀態 PARTIALLY VERIFIED。
5. node tools/check-deps.mjs：302 files、0 violations；node tools/ste-lint.mjs：0 errors、0 warnings。
6. scripts/ai/check-ai-structure.ps1：以單次程序的 safe.directory 環境值執行，exit 0；8 個既有 extensionless-path warnings。
7. 執行包移除 Qt 開發 PATH：--help exit 0、--socket 示範圖存活超過 2 秒；測試行程由驗證腳本自行結束，未編輯資料。

配方與框的功能已實測；沒有焊點標準答案、缺陷樣本或 3D 量測資料。Qt Creator GUI 與量產機台未操作。

## R08.5–R08.7 樣本搜尋版（2026-10-29）

- 固定角度、比例，Otsu／面積控制不再屬於主 UI。R 教樣本，F5 比對整張 5314×4594 原圖。
- 3 個樣本：(1248,135,44,24)、(1513,140,45,25)、(3755,120,44,24)；門檻 90%。
- 632 個去重結果，全部採第一樣本 44×24；另有 3 個 Sample 元件。沒有完整標準答案，不以數量宣稱辨識率。
- 共享版隔離程序 15/0/0；單次真實圖 UI 往返 629.2 ms，截圖背景工作 486.0 ms。兩者範圍不同，不是 p95。
- 同一測試版本保留 R08.8 的表格修復，1017／5000 框選 5 個移動 20 次並放開，分別 136.70／910.20 ms；非本聊天的優化，未修改其預算。
- 全量重連結後 67/67；既有 skip／XFAIL 保留。Debug 無 OpenCV／HALCON 成功。
- 執行包 exe SHA-256：cc646147f4fe4ae4efab249b5a71c4a0f823b237276a9ec778ac8cfe2724bdc9；--help exit 0、--socket 示範圖啟動通過，Qt 開發 bin 不在 PATH。
- 完整 log：build/r08-samples/verified-tests.txt、verified-durations.xml、regression-final.log、debug-final.log、package-smoke.json。

## 區域 CAD 版

5314×4594 真實圖：全圖 686 個候選、2 個共享 profile、47×26 px，畫面工作耗時 813.2 ms（單次，非 p95）。Socket 專項 19/0/0。Release 與 Debug 無 CV/HALCON 建置成功；執行包 smoke 與 ZIP CRC 成功。Log 在 build/r08-region。無完整 GT，不宣稱辨識率；未跑完整 perf harness。
