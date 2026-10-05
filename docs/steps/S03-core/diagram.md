# S03 — 圖解

圖在 GitHub、VS Code（Markdown Preview Mermaid）與 Qt Creator 的 Markdown 預覽中可渲染。互動版見 [explainer.html](explainer.html)。圖裡的名稱都是程式裡真的存在的類別與函式（`src/` 下可搜尋）。

## 1. 現在有哪些層、誰依賴誰（S03 之後）

實線＝「可以依賴」（CMake 的 `target_link_libraries` 與 `#include` 方向一致）；虛線＝「實作 port」。`application` 與 `domain` 不連任何 adapter，也不連 Qt。

```mermaid
flowchart TB
  subgraph APP["app: composition root"]
    CR["CompositionRoot.cpp: makeBackendRegistry, composeCore"]
    MAIN["main.cpp: --backend"]
  end
  PRES["presentation: Qt Widgets, canvas (S02)"]
  subgraph APPL["application: use cases and ports"]
    UC["GenerateObjectsFromSample"]
    JR["JobRunner"]
    REG["BackendRegistry"]
    PORTS["ports: ITemplateBuilder, IPatternSearcher, IExecutor, IPerfProbe, IProgressSink, ..."]
  end
  DOM["domain: geometry, Candidate, Board, ComponentType (std only)"]
  subgraph INFRA["infrastructure: adapters"]
    FK["backend_fake"]
    PF["perf: PerfProbe"]
    TH["threading: ThreadPoolExecutor"]
    CV["backend_opencv (S07, not built yet)"]
    HL["backend_halcon (S08, not built yet)"]
  end
  MAIN --> CR
  CR -. creates .-> FK
  CR -. creates .-> PF
  CR -. creates .-> TH
  MAIN --> PRES
  UC --> PORTS
  JR --> PORTS
  REG --> PORTS
  APPL --> DOM
  PRES --> DOM
  FK -. implements .-> PORTS
  PF -. implements .-> PORTS
  TH -. implements .-> PORTS
  CV -. will implement .-> PORTS
  HL -. will implement .-> PORTS
```

## 2. Ports 與 adapter（為什麼換 back end 不必改 use case）

```mermaid
classDiagram
  class GenerateObjectsFromSample {
    +run(request, cancel, progress) Result
  }
  class ITemplateBuilder {
    <<interface>>
    +backendId() string_view
    +build(image, sample, params, cancel) Result
  }
  class IPatternSearcher {
    <<interface>>
    +backendId() string_view
    +find(image, tmpl, region, params, cancel) Result
  }
  class ITemplate {
    <<interface>>
    +size() Size2D
    +backendId() string_view
    +sampleProblem() optional
  }
  class FakeTemplateBuilder
  class FakeSearcher
  class FakeTemplate
  class ScriptedBuilder
  class ScriptedSearcher
  class OpenCvTemplateBuilder
  class OpenCvPatternSearcher
  GenerateObjectsFromSample --> ITemplateBuilder
  GenerateObjectsFromSample --> IPatternSearcher
  ITemplateBuilder <|.. FakeTemplateBuilder
  IPatternSearcher <|.. FakeSearcher
  ITemplate <|.. FakeTemplate
  FakeTemplateBuilder ..> FakeTemplate : builds
  ITemplateBuilder <|.. ScriptedBuilder
  IPatternSearcher <|.. ScriptedSearcher
  ITemplateBuilder <|.. OpenCvTemplateBuilder
  IPatternSearcher <|.. OpenCvPatternSearcher
```

`FakeTemplateBuilder`、`FakeSearcher` 在 `src/infrastructure/backend_fake/`。`ScriptedBuilder`、`ScriptedSearcher` 只存在於 `tests/core/tst_backend_registry.cpp`：它們證明「全新的 back end」不必動 use case。`OpenCv*` 是 S07 才會有的類別（虛線代表還沒有）。

## 3. 流程 A：use case 的階段（每個階段之間都檢查取消）

```mermaid
sequenceDiagram
  participant W as Worker (JobRunner)
  participant UC as GenerateObjectsFromSample
  participant TB as ITemplateBuilder
  participant PS as IPatternSearcher
  participant G as SimpleGrid (stand-in)
  W->>UC: run(request, cancel token)
  UC->>UC: check sample (size, contrast)
  alt sample problem
    UC-->>W: ok, sampleProblem set, no candidates
  else sample looks fine
    UC->>TB: build(image, sample)
    TB-->>UC: template (or sampleProblem)
    UC->>PS: find(whole image, minScore 0.70)
    PS-->>UC: seed matches
    UC->>UC: first match inside the sample is the Sample
    UC->>G: fitGrid(centers)
    G-->>UC: grid model (or none)
    UC->>G: gridHoles(model, image bounds)
    G-->>UC: predicted positions
    UC->>PS: find(one window per hole, minScore 0.30)
    PS-->>UC: matches in windows
    UC->>UC: best match per hole is Verified, none is Predicted
    UC-->>W: SampleProposal (all candidates with scores)
  end
```

## 4. 工作生命週期：提交、取代、取消、完成

```mermaid
sequenceDiagram
  participant P as Presenter (UI thread)
  participant R as JobRunner
  participant E as IExecutor (worker pool)
  participant J1 as Job 1
  participant J2 as Job 2
  P->>R: submit(work1, done1)
  R->>E: post(task 1)
  E->>J1: start, token 1
  P->>R: submit(work2, done2)
  Note over R: latest becomes 2, token 1 is raised
  R->>E: post(task 2)
  E->>J2: start, token 2
  J1-->>R: returns after it sees token 1
  R-->>P: done1(Cancelled, latest false) - drop the result
  J2-->>R: returns
  R-->>P: done2(Completed, latest true) - show the result
  P->>R: cancel() (Esc)
  Note over R: latest becomes 0, token of the latest is raised
  R-->>P: done(Cancelled, latest false)
```

## 5. 一項工作的狀態

```mermaid
stateDiagram-v2
  [*] --> Queued: submit
  Queued --> Cancelled: flag raised before it starts (work never runs)
  Queued --> Running: a worker takes it
  Running --> Completed: work returns, flag still down
  Running --> Cancelled: work returns, flag is up
  Running --> Failed: work throws
  Completed --> [*]
  Cancelled --> [*]
  Failed --> [*]
```

`done` 在三種終點都會被呼叫恰好一次；`JobReport::usable()` 只有 `Completed` 且 `latest` 為真時才成立。

## 6. 候選怎麼變成畫面上的物件（門檻只是重新過濾）

```mermaid
flowchart LR
  S["Sample: the box the user drew"] --> C
  A["Seed: found in the whole image, score 0.70 or more"] --> C
  B["Verified: found in a window, score 0.30 or more"] --> C
  D["Predicted: the grid says a part belongs here, nothing found"] --> C
  C["SampleProposal.candidates (immutable snapshot)"] --> K["classify(proposal, threshold)"]
  T(["threshold slider"]) --> K
  K --> O1["Sample"]
  K --> O2["Found: score at or above threshold"]
  K --> O3["Rejected: score below threshold"]
  K --> O4["Inferred: Predicted, no score"]
  K --> N["noMatch: OnlyOne or ThresholdTooHigh with a suggested value"]
```

## 7. 組裝與解構順序（`CoreServices`）

```mermaid
flowchart LR
  subgraph create["create (declaration order)"]
    direction LR
    c1["probe"] --> c2["executor"] --> c3["backend"] --> c4["generateObjects"] --> c5["runner"]
  end
  subgraph destroy["destroy (reverse order)"]
    direction LR
    d1["runner: cancel, wait for pending jobs"] --> d2["generateObjects"] --> d3["backend"] --> d4["executor: finish queued tasks, join"] --> d5["probe"]
  end
  create --> destroy
```

## 8. 修改前後：`main` 的啟動流程

**修改前（S02）**：`main` 只認識畫面。沒有 back end 的概念，`--backend` 不存在。

```mermaid
flowchart TD
  M0["main.cpp"] --> P0["parse --lang, --theme, --demo"]
  P0 --> T0["ThemeManager, Translations"]
  T0 --> W0["MainWindow.show()"]
  W0 --> L0["app.exec()"]
```

**修改後（S03）**：多了兩個節點（深色）：解析 `--backend`、`composeCore`。back end 不可用時在顯示視窗之前就以結束碼 2 離開。

```mermaid
flowchart TD
  M1["main.cpp"] --> P1["parse --lang, --theme, --demo, --backend"]
  P1 --> T1["ThemeManager, Translations"]
  T1 --> R1["makeBackendRegistry()"]
  R1 --> C1["composeCore(registry, id)"]
  C1 -->|"Error: unknown or unavailable"| X1["stderr message + exit code 2"]
  C1 -->|"CoreServices"| W1["MainWindow.show()"]
  W1 --> L1["app.exec() - S04 gives CoreServices to the presenters"]
  style R1 fill:#1d4ed8,color:#ffffff
  style C1 fill:#1d4ed8,color:#ffffff
  style X1 fill:#b91c1c,color:#ffffff
```

## 9. 依賴檢查怎麼運作（以及怎麼證明它會失敗）

```mermaid
flowchart TD
  F["each file under src/"] --> L["which layer or adapter? (path)"]
  L -->|"unknown folder"| V0["violation: unknown layer or adapter"]
  L --> I["each include (comments removed)"]
  I --> K["classify: std, project layer, Qt core, Qt other, OpenCV, HALCON, other"]
  K --> J{"does the rule table allow it?"}
  J -->|yes| OK["next include"]
  J -->|no| V1["violation: file, line, include, reason"]
  V0 --> EXIT["exit code 1"]
  V1 --> EXIT
  subgraph proof["proof that the check can fail"]
    FX["13 fixtures, each lists its expected violations"] --> ST["--self-test: reported set equals expected set"]
    WF["fixture domain-qt run as a real test"] --> WL["WILL_FAIL: exit code must be non-zero"]
  end
```
