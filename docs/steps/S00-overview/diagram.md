# S00 — 圖解

圖比文字更快傳達結構。以下圖在 GitHub、VS Code（Markdown Preview Mermaid）與 Qt Creator 的 Markdown 預覽中可渲染；互動版見 [explainer.html](explainer.html)。

## 1. 分層與依賴方向

實線 = 「可以依賴」。虛線 = 「實作 port / 負責組裝」。**依賴只由外往內。**

```mermaid
flowchart TB
  subgraph APP["app — Composition root"]
    MAIN["main.cpp: create objects, pick back end"]
  end
  PRES["presentation — Qt Widgets, presenters, canvas"]
  APPL["application — use cases + ports"]
  DOM["domain — geometry, lattice, types (std only)"]
  subgraph INFRA["infrastructure — adapters"]
    CV["backend_opencv"]
    HL["backend_halcon"]
    ST["storage_json"]
    PF["perf_probe"]
  end
  MAIN -. creates .-> PRES
  MAIN -. creates .-> APPL
  MAIN -. creates .-> INFRA
  PRES --> APPL
  APPL --> DOM
  INFRA -. implements ports .-> APPL
  INFRA --> DOM
```

## 2. Port 與 Adapter（為什麼能換 back end）

```mermaid
classDiagram
  class GenerateObjectsFromSample {
    +run(image, sampleRect, params, cancel) Proposals
  }
  class ITemplateBuilder {
    <<interface>>
    +build(image, rect, params) Template
  }
  class IPatternSearcher {
    <<interface>>
    +find(image, tmpl, region, params, cancel) Matches
  }
  class LatticeEstimator {
    +fit(matches) LatticeModel
    +predict(model, bounds) Positions
  }
  class OpenCvTemplateBuilder
  class OpenCvPatternSearcher
  class HalconTemplateBuilder
  class HalconPatternSearcher
  GenerateObjectsFromSample --> ITemplateBuilder
  GenerateObjectsFromSample --> IPatternSearcher
  GenerateObjectsFromSample --> LatticeEstimator
  ITemplateBuilder <|.. OpenCvTemplateBuilder
  IPatternSearcher <|.. OpenCvPatternSearcher
  ITemplateBuilder <|.. HalconTemplateBuilder
  IPatternSearcher <|.. HalconPatternSearcher
```

`LatticeEstimator` 在 domain：兩個 back end 共用同一份、已測試的幾何邏輯。

## 3. 「框一個 → 全部出現」的執行序列

```mermaid
sequenceDiagram
  actor U as User
  participant V as Canvas
  participant P as AutoRoiPresenter
  participant J as JobRunner (worker)
  participant UC as GenerateObjectsFromSample
  participant TB as ITemplateBuilder
  participant PS as IPatternSearcher
  participant D as LatticeEstimator
  U->>V: drag sample box
  V->>P: sampleDrawn(rect)
  P->>J: submit(job) - latest wins
  P-->>V: show progress after 200 ms
  J->>UC: run(image, rect, params, cancel)
  UC->>TB: build(image, rect)
  TB-->>UC: template
  UC->>PS: find(seed region)
  PS-->>UC: seed matches
  UC->>D: fit(seed matches)
  D-->>UC: lattice model
  loop each predicted position, near to far
    UC->>PS: find(small window)
    PS-->>UC: match or none
    UC->>D: refit(model, match)
  end
  UC-->>J: proposals (immutable)
  J-->>P: result (queued signal)
  P-->>V: draw proposals and timing
```

## 4. 介面互動狀態機

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> DrawingSample: press on image with Sample tool
  DrawingSample --> Idle: Esc
  DrawingSample --> Searching: release
  Searching --> Preview: result ready
  Searching --> Idle: cancel
  Searching --> Error: failure
  Preview --> Searching: parameters changed, latest wins
  Preview --> Committed: Enter or Accept
  Preview --> Idle: Esc or Discard
  Committed --> Idle
  Error --> Idle: dismiss
```

## 5. 執行緒與資料流

```mermaid
flowchart LR
  subgraph UI["UI thread (Qt event loop)"]
    EV["mouse / keyboard"] --> PR["Presenter"]
    PR --> CV2["Canvas + tables"]
  end
  subgraph WK["Worker thread pool"]
    UCW["Use case"] --> PORTS["ports: template / search"]
  end
  IMG[("ImageView: shared, read-only")]
  PR -- "submit job + CancelToken" --> UCW
  UCW -- "immutable result snapshot (queued signal)" --> PR
  IMG --- UCW
  IMG --- CV2
```

重點：UI 與 worker 之間只傳**不可變的快照**與**取消權杖**，不共享可變狀態，所以不需要鎖。
