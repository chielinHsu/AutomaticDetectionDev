# S03 — 效能量測紀錄

原始結果：[`perf/baseline/casparhsu1-core.json`](../../../perf/baseline/casparhsu1-core.json)（入版控；每次執行的原始輸出在 `perf/results/core_*.json`，不入版控）。方法與預算的關係見 [docs/perf-test-plan.md](../../perf-test-plan.md)。

**一句話**：S03 計畫要量三件事——`PerfProbe` 開／關的單次成本、工作提交到開始的延遲、取消旗標生效的延遲——都量到了（Release、假 back end）。**`PerfProbe` 關閉時的成本在量測誤差內，量不出和空迴圈的差別**（目標 < 1 µs，達成）；開啟時每個計時約 257–291 ns；提交到開始約 0.04 ms；旗標生效約 0.01 ms（工作一直在看旗標時）。**另一個意外的發現**：暫代的晶格擬合（`SimpleGrid`）在約 4 900 點要 97–134 ms，是 S05 目標（T3：1 000 點 ≤ 2 ms）的好幾十倍；它是暫代品，這一步不優化，S05 取代它。**沒有任何搜尋類預算（T1–T6）能用假 back end 校準**，`perf/budgets.json` 沒有改。

## 機器指紋

| 項目 | 值 |
|---|---|
| CPU / 核心數 | 13th Gen Intel(R) Core(TM) i7-1360P；4 個 P 核＋8 個 E 核（混合架構），16 個執行緒 |
| RAM | 63.6 GB |
| OS | Windows 11 Version 23H2 (kernel 10.0.22631) |
| 編譯器 / build type | MSVC 19.44.35228，`Release`（`/O2`）；Qt 6.8.0（只有測試程式用到 Qt；被量的程式不連 Qt） |
| commit | 38bbbdf+uncommitted（量測時的工作目錄；最終 commit 見 `docs/changelog-ai.md` 本步驟那一筆） |
| 量測衛生 | process priority HIGH; power throttling off; thread pinned to logical CPU 0 (efficiency class 1 of 2 classes)——呼叫端執行緒釘核；worker 執行緒沒有釘（量的就是「喚醒另一個核心上的 worker」） |

## 方法與限制

- **執行**：`scripts/dev/test.ps1 -Perf`（label `perf`，Release 才有意義）；本檔的數字是**連續 3 次**執行，表中的「中位數」與「p95」取最後一次，「3 次範圍」是三次的中位數的最小與最大。**同一份程式在不同時間量會飄**（S02 在這台機器上看過 ±30%），所以請看範圍，不要比單一數字。
- **P1（PerfProbe 單次成本）**：固定次數的迴圈（關閉與空指標 2 000 000 次、開啟 500 000 次），重複 9 次，第 1 次熱機丟掉。為了不讓編譯器把迴圈消掉，probe 指標放在 `volatile` 變數裡每次重新讀取；「空迴圈」是同樣的迴圈但不建 `ScopedTimer`。關閉時的額外成本 ＝ P1-off 的中位數減 P1-baseline 的中位數。
- **P2（提交 → 開始）**：2 個 worker 的 `ThreadPoolExecutor`，提交空工作 300 次，每次之間睡 2 ms 讓 worker 回到睡眠（量的是「喚醒」，不是「一直在忙」），前 20 次丟掉。數字取自 `JobReport::queuedMs`；另外量 `submit()` 在呼叫端執行緒花多久（UI 執行緒付的代價）。
- **P3（取消旗標生效）**：工作迴圈等旗標；主執行緒記下 `cancel()` 前的時間，工作記下看到旗標的時間；100 次，前 10 次丟掉。兩種工作：一直看旗標（`spin`），以及每 1 ms 看一次（`poll1ms`，和假 back end 的做法相同）。
- **P4（use case 本身）**：假 back end 沒有搜尋成本，所以量到的是擬合、指派與打包，**不是真實搜尋時間**。5 MP（2592×1944）影像、約 225 個與約 4 900 個零件（各含 5% 低分、3% 缺件）；23 次，前 3 次丟掉；探針關與開各一組。
- **沒有量**：真實 back end 的任何成本（S07、S08）；`ThreadPoolExecutor` 在 worker 數 > 核心數時的行為；`PerfProbe` 在多執行緒同時寫入時的競爭成本（只驗證了正確性，沒有量成本）；記憶體用量。

## 結果

| ID | 量什麼 | 單位 | 中位數 | p95 | 3 次範圍（中位數） | n |
|---|---|---|---|---|---|---|
| P1-baseline | 空迴圈（volatile 遞增） | ns | 6.0 | 6.2 | 5.9 – 6.1 | 9 |
| P1-null | `ScopedTimer`，probe 是空指標 | ns | 6.7 | 7.1 | 6.0 – 6.7 | 9 |
| P1-off | `ScopedTimer`，PerfProbe 關閉 | ns | 6.3 | 7.3 | 5.7 – 6.3 | 9 |
| P1-on | `ScopedTimer`，PerfProbe 開啟（環狀緩衝＋互斥鎖） | ns | 257 | 289 | 257 – 291 | 9 |
| P1-clock | 兩次 `steady_clock::now()` | ns | 114 | 147 | 114 – 134 | 9 |
| P2-queue | 工作提交 → 開始執行（閒置的池，2 個 worker） | ms | 0.041 | 0.105 | 0.041 – 0.047 | 280 |
| P2-submit | `submit()` 在呼叫端執行緒花的時間 | ms | 0.025 | 0.043 | 0.025 – 0.030 | 280 |
| P3-spin | `cancel()` → 工作看到旗標（工作一直在看旗標） | ms | 0.011 | 0.019 | 0.011 – 0.014 | 90 |
| P3-poll1ms | `cancel()` → 工作看到旗標（每 1 ms 看一次） | ms | 1.44 | 16.9 | 1.44 – 8.72 | 90 |
| P4-225-off | use case，約 225 個零件（探針關） | ms | 1.33 | 1.74 | 1.33 – 2.18 | 20 |
| P4-225-fit | 　其中擬合（暫代版） | ms | 1.21 | 1.62 | 1.21 – 1.96 | 20 |
| P4-225-on | use case，約 225 個零件（探針開） | ms | 1.53 | 2.34 | 1.53 – 2.32 | 20 |
| P4-4900-off | use case，約 4 900 個零件（探針關） | ms | 148.5 | 174.2 | 106.4 – 148.5 | 20 |
| P4-4900-seed | 　其中種子搜尋（假的，幾乎沒有成本） | ms | 2.08 | 2.43 | 1.48 – 2.08 | 20 |
| P4-4900-fit | 　其中擬合（暫代版） | ms | 134.4 | 158.3 | 96.7 – 134.4 | 20 |
| P4-4900-verify | 　其中視窗驗證（假搜尋器＋指派） | ms | 7.98 | 9.58 | 6.70 – 7.98 | 20 |
| P4-4900-on | use case，約 4 900 個零件（探針開） | ms | 96.7 | 117.0 | 96.7 – 138.9 | 20 |

（`P4-225-seed`、`P4-225-verify` 在 0.02–0.04 ms，沒有列出；完整數字在 JSON。）

## 發現（事實與推測分開）

**事實（已量測）**

1. **`PerfProbe` 關閉的成本量不出來**：P1-off 與空迴圈的中位數差在 ±1 ns 內（三次執行的差分別約 −0.2、−0.15、+0.35 ns，正負都有，代表是雜訊）。關閉時 `ScopedTimer` 只做一次 `enabled()` 虛擬呼叫與一次分支，不讀時鐘。目標 < 1 µs 達成，餘裕約三個數量級。`perf_core` 在 Release 把「p95 < 1 µs」當成失敗條件。
2. **開啟的成本約 257–291 ns／個計時**，其中兩次讀時鐘約 114–134 ns，其餘約 130–170 ns 是 `record()`（再讀一次時鐘約 60 ns、取互斥鎖、寫一格環狀緩衝）。5 ms 的階段多這 0.3 µs，是 0.006%；但**不適合放在微秒等級的熱迴圈裡**。
3. **工作提交到開始約 41–47 µs（中位數）**，p95 約 0.1 ms；`submit()` 本身約 25–30 µs。UI 執行緒提交一項工作的代價是幾十微秒。
4. **取消旗標生效**：工作一直在看旗標時約 11–14 µs（旗標的可見性）。**每 1 ms 看一次旗標的工作**，中位數在三次執行裡是 8.72、1.46、1.44 ms，p95 約 16–17 ms：延遲是由 Windows 的計時器解析度決定的（`sleep_for(1ms)` 實際睡多久是**系統狀態**，預設約 15.6 ms 一格；別的行程可以把它調細），不是旗標本身。**結論**：取消延遲的上限是「工作兩次看旗標之間的最長時間」，真的 back end 要在每一次可中斷的地方看旗標，單次函式庫呼叫無法中斷（本來就是 IExecutor／CancelToken 的已知限制）。
5. **use case 本身（不含真實搜尋）**：約 225 個零件 1.3–2.2 ms；約 4 900 個零件約 100–150 ms，**其中 97–134 ms 是暫代的晶格擬合**，驗證階段（O(M × H) 的指派）約 7–8 ms，假搜尋約 1.5–2 ms。探針開與關的差距小於執行間的雜訊（P4-4900 開比關還快），不能下結論。
6. **同一份程式 3 次執行，P4-4900 的中位數是 106、約 120、148 ms**：和 S02 看到的「同一份程式不同時間飄 ±30%」一致，原因沒有查。

**推測（未驗證）**

- `SimpleGrid` 的成本是 O(抽樣點 × 全部點)：每個抽樣點把其餘點的距離都算一遍再 `partial_sort`（最多 800 個抽樣點）。4 900 點約 3.9 M 次距離計算與排序，量級與觀察到的 100 ms 相符，但沒有用 profiler 確認。S05 的晶格估計要用空間雜湊把鄰居搜尋降到 O(n)，目標是 T3（1 000 點 ≤ 2 ms）。
- 驗證階段的 O(M × H) 指派在 H、M 到數萬時會變成瓶頸；現在 4 900 點只佔約 7 ms，沒有量到問題，所以沒有改。

## 與預算的關係

- `perf/budgets.json` **沒有改動**。T1–T6 都需要真實 back end（S05、S07、S08、S10）；假 back end 的數字不能拿來校準它們。
- **T3**（晶格擬合，1 000 點 ≤ 2 ms）屬 S05；本步驟的暫代擬合在 4 900 點要 97–134 ms，**不是 T3 的量測**（點數不同、也不是 S05 的實作），只是「現在的暫代品有多慢」的紀錄。
- **T5**（端到端：放開滑鼠 → 提案就緒，≤ 300 ms）要等真實 back end 與 S10 的 harness；`PerfProbe` 的 `uc.sample.*` 切片（`template`、`seed`、`fit`、`verify`、`total`）已經就位。
- **I4**（取消 → UI 回到 Idle，≤ 100 ms）量的是 UI，屬 S04；本步驟只提供機制（`JobRunner::cancel()` 不等工作停止）與旗標生效的延遲數字。

## 變異測試（故意植入缺陷，確認測試真的會失敗）

做法：逐一在原始碼植入一個缺陷、重建、只跑相關測試，預期失敗；還原後以 SHA-256 確認檔案與原本完全相同，最後整套重跑全過。14 個變異全部被抓到：

| # | 缺陷 | 失敗的測試 |
|---|---|---|
| M01 | `JobRunner` 取代舊工作時不升起旗標（latest-wins 拿掉） | `tst_job_runner` |
| M02 | `JobReport::latest` 永遠是真 | `tst_job_runner` |
| M03 | use case 跳過視窗驗證 | `tst_generate_objects` |
| M04 | use case 接受太小的樣本 | `tst_generate_objects` |
| M05 | use case 沒有把樣本框內的匹配（自我匹配）從種子裡拿掉 | `tst_generate_objects` |
| M06 | `classify` 的建議門檻改成向上取整 | `tst_generate_objects` |
| M07 | `SimpleGrid` 的內點門檻放寬到全部都算內點 | `tst_grid`（`tst_generate_objects` 另一個項目也失敗） |
| M08 | `gridHoles` 保留影像範圍外的缺格 | `tst_grid` |
| M09 | `FakeSearcher` 忽略視窗限制 | `tst_fake_backend` |
| M10 | `PerfProbe` 環狀緩衝的快照從 0 開始 | `tst_perf_probe` |
| M11 | `CancelToken::cancelled()` 永遠回傳 false | `tst_core_types`、`tst_job_runner`、`tst_generate_objects` |
| M12 | `rotate()` 的正負號反過來 | `tst_domain`（`tst_grid`、`tst_generate_objects` **沒有**抓到：它們的標準答案用同一個 `rotate()`，自己跟自己一致；絕對的角度慣例只由 `tst_domain` 的數字鎖住） |
| M13 | `BackendRegistry` 不要求工廠函式 | `tst_backend_registry` |
| M14 | `check-deps` 讓所有 Qt 標頭都通過 | `check_deps_self_test`、`check_deps_catches_a_violation` |

## 環境驗證

- **建置**：Release 與 Debug 都用 `scripts/dev/test.ps1` 建置並執行（`/W4 /WX`，無警告）。
- **命令列**（`tst_cli`，從外面用真的行程）：`--backend=opencv`、`--backend=halcon`、`--backend=turbo` 以結束碼 2 結束，stderr 有原因與「Available back ends: fake」；預設與 `--backend=fake` 啟動後持續執行。
- **qmake 專案**（`AiInspTool/AiInspTool.pro`）：在獨立目錄用 qmake ＋ jom 命令列建置成功（Release，`AiInspTool.exe`）。**沒有在 Qt Creator 裡實際載入過**。

## 重現方式

```powershell
. .\scripts\dev\env.ps1
powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1 -Perf      # 效能（Release）
node tools/check-deps.mjs                                                # 依賴方向
node tools/check-deps.mjs --self-test                                    # 依賴檢查自己的測試
```

`perf_core` 同時把四組真的跑出來的候選（位置、分數、來源）寫進 JSON 的 `demo` 欄位，`explainer.html` 嵌入的就是它。
