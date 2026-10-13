# ADR-023 效能回歸閘門：用多次執行的範圍比、已知的超標列出不擋、跳過不是通過、整台變慢不下結論

- 日期：2026-10-13
- 決定者：S10 的計畫（`docs/plan/S10-perf-harness.md`）與 `docs/perf-test-plan.md` 要求「慢超過 15% 或超出預算就失敗」；S04、S05、S08、S09 完成後的提醒要求閘門用多次執行的範圍、分辨跳過與通過、有「已知、不擋」的機制、看得出整台機器變慢；使用者 2026-10-12 要求 XFAIL 列成「已知、待修」。規則的細節、閾值與工具的切法是 Claude Code (Claude Fable 5.1) 的提案並實作。
- 狀態：採用（閾值是起點：15% 來自 `perf/budgets.json` 的 `regressionLimitPercent`，S10 實測的雜訊在 `docs/steps/S10-perf-harness/perf.md`）

## 背景

S02–S09 各自有效能量測（`perf_*`），各自寫自己格式的結果檔、各自判斷預算（有的讓測試失敗、有的只記錄）、各自維護「已知超標」的清單（`perf_ui_baseline` 的 `knownOver("I1")`、`perf_ui_s04` 的 `knownOver("U3")`）。沒有一個地方把一次執行的所有數字放在一起和上一次比。而且：

- 同一份程式在這台機器連跑幾次會差 30%（S04）、1.4 倍（S05）、2–3 倍（UI，2026-10-12 的提醒）；HALCON 的時間在幾次之間差得比 OpenCV 多，整台機器變慢時所有指標一起變慢（S08）。單次對預算、單次對上一次的 15% 都會誤報。
- HALCON 讀不到授權時整組測試跳過，`test.ps1` 算通過；`QEXPECT_FAIL` 的列（ADR-004 §5 的 B1–B3、O1）log 寫 XFAIL、測試算通過。閘門不能把這兩種當成「量過而且沒問題」。

## 決定

1. **一份執行紀錄**：`scripts/dev/perf.ps1` 跑 `test.ps1 -Perf`，`tools/perf-compare.mjs collect` 把這次寫出的每個結果檔（`perf/results/{core,lattice,storage,opencv,halcon,ab,matrix,e2e}_*.json`、`<建置>/test-work/perf-results/ui-*.json`）正規化成同一種指標列（`key ＝ 來源/id`、中位數、p95、單位、預算、方向、已知），加上每個測試執行檔的狀態（log 的 `Totals`、`SKIP`、`XFAIL`、`XPASS`），寫成 `perf/results/<日期>-<commit>-<機器>.json`。既有的量測程式**沒有改**：它們的結果檔就是輸入。
2. **基準線是範圍，不是一個數字**：`perf-compare baseline` 從同一份程式的幾次執行建 `perf/baseline/<機器>-harness.json`，每個指標記中位數與 p95 的範圍（`medianRange`、`p95Range`）。閘門的規則：這次的**中位數**比範圍的上緣還高超過 `regressionLimitPercent`（15%）才算回歸；單位是 ms 的另外要高超過 0.05 ms（微秒等級的指標不看百分比，S05 的 NMS 那類）；越大越好的指標（召回率）反過來；只看 ms、MB、px、percent 的指標。用中位數不用 p95，因為 p95 在 10–30 次裡只由 1–2 個樣本決定。
3. **預算**：p95（越大越好的看最小值）超過 `perf/budgets.json` 的 limit ＝ 超標。**已知的列出、不擋**，來源只有三個：指標自己標的 `known`（例如 `perf_matrix` 對 ADR-004 §5 的格子）、`budgets.json` 裡 `status` 是 `measured-over-limit`（U3）或 `calibrated-at-limit`（I1）的 id、`budgets.json` 的 `knownIssues`（來源 ＋ id 的樣式 ＋ 理由，例如 `perf_ab` 的 HALCON 低對比、重模糊、雜訊 12）。各量測程式自己的 `knownOver` 保留（它們決定自己的執行檔失不失敗），但閘門只看 `budgets.json`，所以「已知」只有一個地方要改。
4. **整台機器變慢**：比較過的指標有一半以上變慢時，結果是 **INCONCLUSIVE（結束碼 3）**，不是 FAIL：這台機器實測會整台變慢（S08 的第 1 次、2026-10-12 的提醒），一次改動不會讓所有東西一起慢。報表會說「再量一次」。
5. **跳過不是通過、XFAIL 是待修**：log 有 `SKIP` 的執行檔列成「未驗證」，有 `XFAIL` 的列成「已知、待修」（列名與訊息），`XPASS` 或 `FAIL!` ＝ 失敗。兩種列出都不改結束碼（跳過的原因通常是授權，不是程式）。基準線有、這次沒有的指標列成「缺少」（HALCON 跳過時就是它的指標）。
6. **結束碼**：0 通過、1 失敗（超出預算而且不是已知的、回歸、測試失敗）、2 輸入有問題、3 無法判定。`perf.ps1 -Runs N` 取最差的。
7. **自我測試**：`perf-compare.mjs --self-test` 用造的資料驗證每一條規則（`test.ps1` 的 node 檢查之一）；`perf.ps1 -InjectDelayMs`（`AOI_PERF_INJECT_DELAY_MS`）讓 harness 每次量測多等幾十毫秒，證明閘門抓得到變慢；注入延遲的結果檔記著 `injectedDelayMs`，`baseline` 子命令拒收。
8. **報表**：`docs/steps/S10-perf-harness/explainer.html` 同時是說明頁與報表的模板：`perf-compare html` 把執行紀錄、比較結果、基準線嵌進 `aoi-perf-run` 區塊，寫成 `perf/results/<同名>.html`；頁面上重算閘門的規則，所以可以拖門檻看判定怎麼變。

## 否決的替代方案

- **單次 p95 對基準線 15%**（計畫的原文）：S04、S05、S08 的數字顯示會誤報；改成「中位數對幾次執行的範圍上緣」。
- **每個量測程式自己擋回歸**：每個程式要各自讀基準線、各自算，已知超標的清單又多一份；改成程式只量、閘門統一比。
- **把 S02–S09 的量測程式改寫成同一種結果格式**：風險高、沒有收益；正規化放在 `collect`。
- **跳過或 XFAIL 讓閘門失敗**：HALCON 讀不到授權時整個閘門都紅，誰也看不出程式有沒有變慢；改成列出來，但明確寫「未驗證」。
- **用統計檢定（例如 Mann-Whitney）取代百分比**：樣本只有 10–30 次、雜訊是機器狀態而不是常態分佈，檢定給的信心是假的；百分比加範圍比較誠實，而且門檻可以拖著看。

## 後果

- 量基準線要跑 `perf.ps1 -Runs 3 -WriteBaseline`（約 30–45 分鐘），改了會影響效能的程式之後要重建基準線（基準線記著 commit 與 Qt 版本，不同時報表會警告）。
- 新的量測程式只要寫 `perf/results/<來源>_<時間>.json`（`metrics[]` 的欄位同既有的）並在 `perf-compare.mjs` 的 `PREFIX_SOURCES` 登記來源，閘門就會比它。
- 已知超標的清單在 `perf/budgets.json`，修好之後要拿掉（U3 在 S11，B1–B3 在 `fix/halcon-accuracy`），否則閘門會一直把它當已知。
