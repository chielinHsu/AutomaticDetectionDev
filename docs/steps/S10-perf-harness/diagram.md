# S10 — 圖解

## 1. harness 的結構：誰量、誰彙整、誰判定

```mermaid
flowchart LR
  subgraph measure["量（Qt Test，Release，offscreen）"]
    M1["perf_core / perf_lattice / perf_storage<br/>（S03、S05、S06）"]
    M2["perf_opencv / perf_halcon<br/>（S07、S08）"]
    M3["perf_ui_baseline / perf_ui_s04 /<br/>perf_compare_ui（S02、S04、S09）"]
    M4["perf_ab（S09）"]
    M5["perf_matrix（S10）<br/>搜尋矩陣 × back end"]
    M6["perf_e2e（S10）<br/>T5：放開滑鼠 → 提案就緒"]
  end
  R1[("perf/results/&lt;來源&gt;_&lt;時間&gt;.json")]
  R2[("&lt;建置&gt;/test-work/perf-results/ui-*.json")]
  L[("&lt;建置&gt;/test-logs/*.txt<br/>Totals、SKIP、XFAIL")]
  M1 --> R1
  M2 --> R1
  M4 --> R1
  M5 --> R1
  M6 --> R1
  M3 --> R2
  measure --> L
  P["scripts/dev/perf.ps1<br/>環境快照 → test.ps1 -Perf → collect → compare"]
  C["tools/perf-compare.mjs collect<br/>正規化成同一種指標列"]
  RUN[("perf/results/&lt;日期&gt;-&lt;commit&gt;-&lt;機器&gt;.json<br/>一份執行紀錄")]
  G["tools/perf-compare.mjs compare<br/>閘門（ADR-023）"]
  B[("perf/baseline/&lt;機器&gt;-harness.json<br/>幾次執行的範圍")]
  BU[("perf/budgets.json<br/>limit、status、knownIssues")]
  H["perf/results/&lt;同名&gt;.html<br/>互動式報表（explainer.html 的模板）"]
  P --> C
  R1 --> C
  R2 --> C
  L --> C
  C --> RUN
  RUN --> G
  B --> G
  BU --> G
  G --> H
  G -->|"結束碼 0／1／2／3"| P
```

這張圖要讓你看到：S02–S09 的量測程式一個都沒有改，它們的結果檔就是輸入；新的只有兩個量測（矩陣、T5）、一個彙整與判定的工具、一個流程腳本。閘門的三個輸入（執行紀錄、基準線、預算）都是檔案，所以可以離線重算、也可以在報表上拖門檻重算。

## 2. 執行紀錄的 schema（`aoi-perf-run/1`）

```mermaid
classDiagram
  class Run {
    schema = "aoi-perf-run/1"
    collectedAt
    since
    machine : Machine
    environment : Environment
    injectedDelayMs
    sources : Source[]
    tests : TestStatus[]
    metrics : Metric[]
    extra : map~source, object~
  }
  class Machine {
    machineName; cpu; logicalCores; ramGB
    os; qt; compiler; buildType; commit
    measurementHygiene; qpaPlatform
  }
  class Environment {
    sampledAt; powerScheme; acPower; batteryPercent
    cpuLoadPercent; cpuMhz; cpuMaxMhz; processCount
    qpaPlatform; profile; injectedDelayMs
  }
  class Source {
    source; file; kind; step; measuredAt; metrics; injectedDelayMs
  }
  class Metric {
    key = "來源/id"
    source; id; label; unit
    n; median; p95; min; max; p05
    budget; budgetId; direction = max | min
    known; note
  }
  class TestStatus {
    name; status = pass | pass-with-skips | fail | incomplete
    passed; failed; skipped
    skips : where, reason
    xfails : where, message
    fails : where, message
  }
  Run --> Machine
  Run --> Environment
  Run --> "*" Source
  Run --> "*" Metric
  Run --> "*" TestStatus
```

基準線（`aoi-perf-baseline/1`）：`metrics` 是 `key → { label, unit, direction, n, medianRange[lo, hi], p95Range[lo, hi], medians[], p95s[] }`，加上 `machineName`、`buildType`、`qt`、`commit`、`runs[]`（來源的執行紀錄檔名）、`limitPercent`。比較的結果（`aoi-perf-compare/1`）：`verdict`、`exitCode`、`overBudget[]`、`knownOverBudget[]`、`regressions[]`、`knownRegressions[]`、`improvements[]`、`unchanged[]`、`missing[]`、`newMetrics[]`、`notVerified[]`、`xfails[]`、`failedTests[]`、`warnings[]`。

## 3. 閘門的決策流程（每一個指標）

```mermaid
flowchart TD
  S([指標 m]) --> B{有預算？}
  B -->|有| BV["值 ＝ p95（越大越好：最小值）"]
  BV --> BO{超過 limit？}
  BO -->|否| W[withinBudget]
  BO -->|是| K1{已知？<br/>m.known／budgets 的 status／knownIssues}
  K1 -->|是| KO[knownOverBudget：列出、不擋]
  K1 -->|否| OB[overBudget：失敗]
  B -->|沒有| BL
  W --> BL
  KO --> BL
  OB --> BL
  BL{基準線有這個 key？<br/>而且單位是 ms／MB／px／percent}
  BL -->|沒有| NEW[newMetrics／只比預算]
  BL -->|有| RG{"中位數 &gt; 範圍上緣 × (1 + 15%)<br/>而且差 &gt; 絕對下限（ms：0.05）"}
  RG -->|是| K2{已知？}
  K2 -->|是| KR[knownRegressions：列出、不擋]
  K2 -->|否| REG[regressions：失敗]
  RG -->|否| FS{"中位數 &lt; 範圍下緣 × (1 − 15%)"}
  FS -->|是| IMP[improvements]
  FS -->|否| UN[unchanged]
```

```mermaid
flowchart TD
  A([所有指標比完]) --> T["測試 log：SKIP → notVerified<br/>XFAIL → xfails，XPASS／FAIL! → failedTests"]
  T --> N{"變慢的 ÷ 比較過的 ≥ 1/2？"}
  N -->|是| INC["INCONCLUSIVE（結束碼 3）<br/>整台機器變慢，再量一次"]
  N -->|否| F{"overBudget 或 regressions<br/>或 failedTests 非空？"}
  F -->|是| FAIL["FAIL（結束碼 1）"]
  F -->|否| PASS["PASS（結束碼 0）"]
```

這兩張圖要讓你看到：「已知」只擋失敗、不擋列出；跳過與 XFAIL 一定出現在報表上，但不改結束碼；一半以上變慢時不下回歸的結論。

## 4. T5 的路徑（`perf_e2e` 量的是哪一段）

```mermaid
sequenceDiagram
  participant T as perf_e2e（UI 執行緒）
  participant C as ImageCanvas／SampleTool
  participant P as AutoRoiPresenter
  participant J as JobRunner（worker）
  participant U as GenerateObjectsFromSample（use case）
  T->>C: QTest press／move／release（樣本框）
  Note over T: t0 ＝ release 之前
  C->>P: onRegionDrawn(rect, Sample)
  P->>J: submit（latest-wins）
  J->>U: run(request)（範本 → 種子 → 晶格 → 驗證）
  U-->>J: SampleProposal
  J-->>P: onJobFinished（UiPoster 回到 UI 執行緒）
  P->>P: 套用：classify、場景、畫布、物件樹、訊息卡
  P-->>T: searchApplied(generation)
  Note over T: T5 ＝ now − t0
  T->>C: paintPending（UpdateRequest）
  Note over T: T5-paint ＝ 第一幀畫完
```

T5 不含影像載入、不含 HALCON 第一次建立 back end 的授權檢查（熱機 3 次之後才開始量）；`PerfProbe` 的切片（`uc.sample.*`、`job.queue`、`job.run`、`ui.*`）另外寫進結果，S11 用它找最大的一塊。

## 5. 搜尋矩陣的格子（quick 與 full）

```mermaid
flowchart LR
  BASE["基準格<br/>5 MP · 64×32 · ±5° · 200 個 · noise4<br/>熱機 5、量 30"]
  BASE --> S["影像 1／5／12／25 MP"]
  BASE --> P["樣本 32×16／64×32／128×128"]
  BASE --> A["角度 0／±5／±180°"]
  BASE --> N["元件 10／100／1 000"]
  BASE --> D["難度 clean／noise4／noise12／missing5／glare10／lowContrast"]
  S --> Q["quick：15 格 × 每個 back end<br/>其餘的格子熱機 2、量 10"]
  P --> Q
  A --> Q
  N --> Q
  D --> Q
  Q -.AOI_PERF_PROFILE=full.-> F["full：4 × 3 × 3 × 3 × 6 ＝ 648 格<br/>每格熱機 1、量 3"]
```

每一格記：總時間（中位數、p95）、四個階段、峰值記憶體（熱機之後）、召回率、位置誤差、誤判、有沒有晶格。1 000 個元件的那一格就是 T2 的條件（種子搜尋帶 T2a／T2b 的預算）；HALCON 在 lowContrast 與 noise12 的格子標 known（ADR-004 §5 的 B1、B3）。
