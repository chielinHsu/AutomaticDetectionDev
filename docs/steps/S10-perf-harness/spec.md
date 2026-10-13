# S10 — 效能測試 harness 與回歸閘門：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs`

<!--STE-->
## Purpose
This step builds one performance test for the whole program. One command builds the program and runs every measurement. It collects the numbers into one record. It compares the record with the budget and with the baseline, and writes a report. A slow change fails the command.
<!--/STE-->

**繁中**：把 S02–S09 零散的效能量測整合成一套：一個指令（`scripts/dev/perf.ps1`）建置、跑所有量測（`test.ps1 -Perf`）、彙整成一份執行紀錄、和預算與基準線比（閘門，`tools/perf-compare.mjs`）、產生互動式報表（`explainer.html`）。新的量測：搜尋矩陣（`perf_matrix`：影像大小 × 樣本大小 × 角度範圍 × 元件數 × 難度 × back end）與 T5 端到端（`perf_e2e`：放開滑鼠 → 提案就緒，走產品的接線）。

<!--STE-->
## Reason
Each earlier step measured its own part and kept its own list of known slow items. No tool compared all the numbers with the last run. The same program on the same machine can be two to three times slower in a second run. A gate that compares one run with one number gives false alarms. A skipped test looks like a passed test. We need one record and a baseline with a range. We need one list of known items. We need a report that shows the skipped tests and the expected failures.
<!--/STE-->

**繁中**：每一步各量各的、各自維護「已知超標」的清單（`knownOver`），沒有工具把一次執行的所有數字和上一次比。這台機器同一份程式兩次可以差 2–3 倍（S04、S05、S08、2026-10-12 的提醒），單次對一個數字的閘門會誤報；跳過的測試看起來像通過。所以要：一份執行紀錄、記範圍的基準線、只在一個地方（`perf/budgets.json`）的已知清單、看得到跳過與 XFAIL 的報表。設計決定在 [ADR-023](../../adr/023-perf-regression-gate.md)。

<!--STE-->
## Effect
One command gives one verdict: PASS, FAIL or INCONCLUSIVE. The report shows each metric against its budget and against the range of the baseline. A known slow item is amber. A skipped test is "not verified". When we make the program slower on purpose, the gate fails. The measured noise and the numbers of this step are in perf.md.
<!--/STE-->

**繁中**：一個指令給一個判定（PASS／FAIL／INCONCLUSIVE）；報表上每個指標對預算、對基準線的範圍；已知的超標是琥珀色；跳過的執行檔是「未驗證」。故意放慢時閘門真的失敗（`-InjectDelayMs`）。本步驟的實測數字（T5、矩陣、雜訊、harness 的成本）在 [`perf.md`](perf.md)。

## 構成

| 部分 | 檔案 | 做什麼 |
|---|---|---|
| 一鍵執行 | `scripts/dev/perf.ps1` | 記錄量測當下的環境（電源計畫、CPU 負載與時脈、是否接電源）→ `test.ps1 -Perf` → `collect` → `compare`（→ `.html`）；`-Runs N` 連跑並算雜訊、`-WriteBaseline` 建基準線、`-Filter`、`-Window`（真機視窗）、`-Profile full`、`-InjectDelayMs`（自我測試） |
| 搜尋矩陣 | `tests/perf/perf_matrix.cpp` | 每一格 × 每個會讀像素的 back end：use case 端到端的中位數與 p95、四個階段的切片（範本、種子、晶格、驗證）、峰值記憶體、召回率與位置誤差；quick（15 格）與 full（540 格）兩種 profile；harness 的成本（`H-overhead`）與行程內的雜訊（`H-noise`） |
| T5 端到端 | `tests/perf/perf_e2e.cpp` | 放開滑鼠 → `searchApplied`（提案套到畫面）；再到第一幀畫完；`PerfProbe` 的切片（`uc.*`、`job.*`、`ui.*`）；每個 back end 一列（fake 那一列 ＝ UI 與 harness 的成本） |
| 共用 | `tests/perf/HarnessSupport.h` | 統計（和 S07–S09 同一個分位數定義）、指標、機器指紋、環境快照、峰值取樣、執行次數的環境變數、結果檔 |
| 閘門 | `tools/perf-compare.mjs` | `collect`、`compare`、`baseline`、`noise`、`html`、`--self-test`；規則見 ADR-023 |
| 報表 | `docs/steps/S10-perf-harness/explainer.html` | 說明頁 ＝ 報表的模板：嵌入實際的執行紀錄，可載入別次的 JSON，拖門檻重算 |
| 基準線 | `perf/baseline/casparhsu1-harness.json` | 這台機器幾次執行的範圍（入版控） |
| 預算 | `perf/budgets.json` | T5 校準；`knownIssues`（ADR-004 §5 的 B1–B3）；頂層 `status` |

## 怎麼解讀報表

1. 最上面的判定：**PASS** 沒有指標超標或變慢；**FAIL** 至少一個超出預算（不是已知的）、比基準線慢超過門檻、或有測試失敗；**INCONCLUSIVE** 一半以上的指標都變慢，多半是整台機器變慢，再量一次。
2. 「量測當下」那一行：CPU 負載高、時脈低、用電池、行程多的那一次，數字不可信；報表會標出來。
3. 「已知的超標」與「已知的變慢」：原因已經記錄（U3、I1、ADR-004 §5 的 B1–B3），列出但不改判定；修好之後要從 `perf/budgets.json` 拿掉。
4. 「未驗證」：有 SKIP 的執行檔（通常是 HALCON 讀不到授權）；它的指標在基準線裡會變成「缺少」。「XFAIL」：已知、待修；修好會變成 XPASS ＝ 失敗。
5. 搜尋矩陣的表：每一格一列，顏色條是四個階段的比例，S11 從最大的一塊下手；召回率 < 99、誤差 > 0.5 的格子標紅。
6. 拖「回歸的門檻」看判定怎麼變：門檻放寬到雜訊之上才不會誤報，但放太寬就抓不到真的回歸（`perf.md` 的雜訊數字是依據）。

## 穩定性（量測前的建議）

- 接電源；電源計畫用「高效能」或「平衡」（報表會顯示實際的）；筆電不要闔上。
- 關掉瀏覽器、通訊軟體、其他會吃 CPU 的程式；不要同時建置或跑另一組測試（HALCON 的 `SESSIONS=1` 也不允許）。
- 量測程式自己會把行程優先權設成 HIGH、關閉電源節流、把量測的執行緒釘在 P 核（`MeasurementHygiene.h`，S02）。
- before／after 要**前後交替量**（A、B、A、B），不要先量完 A 再量 B：機器的速度會隨時間飄。
- 一次結果的 CPU 負載 ≥ 50% 時報表會警告；基準線用 `-Runs 3 -WriteBaseline`，三次之中負載高的那一次要重跑。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | 量（`perf_*`）、彙整與比較（`perf-compare.mjs`）、流程（`perf.ps1`）、呈現（`explainer.html`）各一個檔案；既有的量測程式沒有改 | `git diff --stat` 只有新檔與 `tests.pro`、`test.ps1` |
| O | 新的量測程式只要寫同格式的結果檔並在 `PREFIX_SOURCES` 登記；矩陣新增一格 ＝ 改 `dimensions`；新的已知項目 ＝ `budgets.json` 的 `knownIssues` | 新增 `perf_matrix`、`perf_e2e` 沒有改 `collect` 的其他部分 |
| L | 矩陣與 T5 以組裝根註冊的 back end 參數化（`ContractSupport.h` 的資料列），沒編進來或沒有授權的 SKIPPED 並說原因 | log 每個 back end 一列 |
| I | harness 只用 port（`GenerateObjectsFromSample`、`IPerfProbe`）與 Presenter 的訊號，不碰 adapter 內部 | `perf_matrix.cpp`、`perf_e2e.cpp` 沒有 include 任何 `backend_*` 的標頭 |
| D | back end 由 `app::makeBackendRegistry` 給；閘門只依賴結果檔與 `budgets.json` | `--self-test` 不碰 `perf/results` |

## 如何驗證

- [x] 建置通過（`scripts/dev/test.ps1`，Release，OpenCV＋HALCON）
- [x] 測試通過（`perf_matrix`、`perf_e2e`、`perf_compare_self_test`；`test.ps1` 預設全部）
- [x] `node tools/ste-lint.mjs --strict` 無 error
- [x] `perf.md` 內的指標都已量測並與 `perf/budgets.json` 比對（T5 校準；矩陣與雜訊的數字在 `perf.md`）
- [x] 故意放慢（`perf.ps1 -InjectDelayMs`）時閘門失敗（`perf.md` 第 5 節）
