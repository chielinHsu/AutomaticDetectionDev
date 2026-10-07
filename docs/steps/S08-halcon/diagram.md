# S08 — 圖解

七張圖：HALCON adapter 的結構、搜尋管線、back end 可用性的決策（修改前後）、啟動到授權提醒的序列、位姿換算、取消、流程 B。文字說明在 [`spec.md`](spec.md)，決策在 [ADR-017](../../adr/017-halcon-shape-model-backend.md)（與 ADR-010、ADR-014 的「後續（S08）」），數字在 [`perf.md`](perf.md)。

## 1. 結構：哪個類別實作哪個 port

```mermaid
flowchart LR
  subgraph application["application（port，沒有改）"]
    ITB["ITemplateBuilder"]
    IPS["IPatternSearcher"]
    ILM["ILibraryMatcher"]
  end
  subgraph halcon["infrastructure/backend_halcon（S08 新增）"]
    TB["HalconTemplateBuilder<br/>create_shape_model<br/>角度步進自己算、範圍兩端多 2 步"]
    T["HalconTemplate<br/>HShapeModel、層數、角度範圍"]
    PS["HalconPatternSearcher<br/>find_shape_model"]
    LM["HalconLibraryMatcher<br/>流程 B：一次 find_shape_models"]
    ST["HalconSearchSteps<br/>影像與區域、結果 → 契約"]
    IM["HalconImages<br/>ImageView → HImage（零複製或計數）"]
    PO["HalconPose<br/>(Row, Column, Angle) → ADR-010"]
    CW["HalconCancelWatch<br/>interrupt_operator"]
    RT["HalconRuntime<br/>延遲載入、版本、綁定"]
    LI["HalconLicense<br/>探測、判斷、提醒"]
    BE["HalconBackend<br/>工廠、setHalconThreads"]
  end
  APP["app/CompositionRoot<br/>registerBackend halcon<br/>backendNotices"]
  SDK["third_party/halcon<br/>標頭＋halconcpp.lib"]
  TB -. 實作 .-> ITB
  PS -. 實作 .-> IPS
  LM -. 實作 .-> ILM
  TB --> T
  PS --> T
  PS --> ST
  LM --> ST
  LM --> TB
  ST --> IM
  ST --> PO
  PS --> CW
  LM --> CW
  BE --> LI
  LI --> RT
  BE --> TB
  BE --> PS
  APP --> BE
  APP --> LI
  halcon -. 編譯時 .-> SDK
```

## 2. 搜尋管線（一次 `find_shape_model`）

```mermaid
flowchart TD
  A["find(image, template, region, params, cancel)"] --> B{"範本是 HALCON 建的？<br/>影像、分數、角度範圍合法？"}
  B -- 否 --> E1["Error：BackendMismatch／InvalidArgument"]
  B -- 是 --> C{"範本有樣本問題？<br/>（建不出模型：LowContrast）"}
  C -- 是 --> R0["回空的結果（成功）"]
  C -- 否 --> D["accepts：整張影像，或裁到影像內的視窗"]
  D --> F["HalconImages：ImageView → HImage<br/>Gray8 緊密：零複製<br/>Gray16：查表換 8 bit（計數）<br/>彩色：cvtColor 的係數（計數）"]
  F --> G{"視窗清單？"}
  G -- 否 --> H["區域 ＝ 整張影像"]
  G -- 是 --> I["區域 ＝ 所有視窗的聯集<br/>（各往外 2^(層數−1) 像素）"]
  H --> J["find_shape_model<br/>角度：使用者的範圍（弧度）<br/>least_squares、greediness 0.9、NumMatches 0<br/>（HalconCancelWatch 包住）"]
  I --> J
  J --> K["每個結果：HalconPose 換算<br/>x ＝ Column ＋ 0.5(sin＋cos)<br/>y ＝ Row ＋ 0.5(cos−sin)、角度 → 度"]
  K --> L["丟掉：分數 < minScore、中心不在任何 accept<br/>角度夾回範圍"]
  L --> M["domain 的 NMS（範本大小、IoU > 0.5）<br/>排序（分數、y、x）、最多 maxMatches"]
  M --> R["回傳 Match 清單"]
```

## 3. back end 可用性的決策（修改前 → 修改後）

修改前（S03–S07）：`halcon` 永遠登記成不可用，原因是「S08 才會有」。

```mermaid
flowchart LR
  A0["makeBackendRegistry"] --> B0["registerUnavailable('halcon',<br/>'arrives in step S08')"]
  B0 --> C0["--backend=halcon → 結束碼 2"]
```

修改後（S08）：粗框是新增或改變的節點。

```mermaid
flowchart TD
  A["makeBackendRegistry"] --> B{"AOI_WITH_HALCON？"}
  B -- 否 --> U1["registerUnavailable：<br/>沒有編進程式，用 preset release-halcon"]
  B -- 是 --> C{"PATH 上有 halconcpp.dll？<br/>（SearchPath，不載入）"}
  C -- 否 --> U2["registerUnavailable：<br/>沒有安裝 HALCON runtime"]
  C -- 是 --> D["registerBackend('halcon', 工廠)"]
  D --> E["使用者選 halcon（--backend）<br/>→ 工廠"]
  E --> F{"HalconRuntime：載入、版本、<br/>綁定所有函式（受保護）"}
  F -- 失敗 --> X["BackendUnavailable（原因）<br/>→ 結束碼 2，不當掉"]
  F -- 成功 --> G{"HalconLicense：第一個運算子<br/>（約 1.6 秒，只做一次）"}
  G -- 無效 --> X
  G -- 有效 --> H{"含 shape matching？"}
  H -- 否 --> X
  H -- 是 --> I["set_system('thread_num', 核心數 − 1)<br/>建立範本建立器與搜尋器"]
  I --> J{"runtime 授權，而且<br/>SDK 授權檔的 dongle 不在？"}
  J -- 是 --> N["提醒：stderr、吐司、選單提示"]
  J -- 否 --> OK["正常啟動"]
  N --> OK
  classDef changed stroke-width:3px
  class B,C,D,E,F,G,H,I,J,N,U1,U2 changed
```

## 4. 啟動到授權提醒（序列）

```mermaid
sequenceDiagram
  participant M as main.cpp
  participant R as CompositionRoot
  participant B as HalconBackend
  participant RT as HalconRuntime
  participant L as HalconLicense
  participant H as HALCON（DLL）
  participant W as MainWindow
  M->>R: makeBackendRegistry()
  R->>RT: halconRuntimeOnPath()
  M->>R: composeCore("halcon")
  R->>B: 工廠：makeHalconBackend()
  B->>L: halconLicense()（第一次才探測）
  L->>RT: halconRuntimeProblem()
  RT->>H: LoadLibrary、版本、__HrLoadAllImportsForDll
  L->>H: gen_image_const（初始化＋授權）
  L->>H: get_system：current_license_info、hostids、available_license_files、unlicensed_operators
  L-->>B: 探測結果
  B->>B: judgeHalconLicense → 能用
  B-->>R: Backend（範本建立器＋搜尋器）
  R->>B: setHalconThreads(核心數 − 1)
  M->>R: backendNotices("halcon")
  R-->>M: SdkDongleMissing
  M->>M: stderr 一行（fflush）
  M->>W: setBackends（提示含提醒）、showToast
```

## 5. 位姿換算（HALCON → ADR-010）

```mermaid
flowchart LR
  A["find_shape_model：<br/>Row、Column（像素中心在整數，y 在前）<br/>Angle（弧度）"] --> B["參考點真正的位置（HALCON 座標）<br/>(Row, Column) ＋ R(Angle)·(0.5, 0.5) − (0.5, 0.5)<br/>R ＝ [[cos, −sin], [sin, cos]] 作用在 (row, column)"]
  B --> C["＋0.5（ADR-010 的像素中心）<br/>＋ 子影像的原點"]
  C --> D["x ＝ Column ＋ 0.5(sin ＋ cos)<br/>y ＝ Row ＋ 0.5(cos − sin)<br/>角度 ＝ Angle × 180／π（不改正負號）"]
```

## 6. 取消：interrupt_operator

```mermaid
sequenceDiagram
  participant J as worker（搜尋）
  participant W as HalconCancelWatch（小執行緒）
  participant H as HALCON
  participant U as UI（latest-wins）
  J->>W: 建立（記下 HThreadID）
  J->>H: find_shape_model（可能幾百毫秒）
  loop 每 1 ms
    W->>W: token.cancelled()？
  end
  U->>U: 新的工作 → 舊工作的旗標升起
  W->>H: interrupt_operator(HThreadID, 'cancel')
  H-->>J: 例外 H_ERR_CANCEL（22）
  J->>W: 解構（停掉小執行緒）
  J-->>U: Error：Cancelled（結果由工作編號丟棄）
```

## 7. 流程 B：一次 find_shape_models

```mermaid
flowchart TD
  A["候選類型 1..N"] --> B{"有參考影像、比區域小？"}
  B -- 否 --> S["略過（不算比對過）"]
  B -- 是 --> C["校正值不同：zoom_image_factor 到板子的比例"]
  C --> D["HalconTemplateBuilder：整張參考影像當樣本<br/>→ 一個 shape model"]
  D --> E["收集模型"]
  E --> F["find_shape_models（一次）<br/>MaxOverlap、NumMatches：每個模型一個值<br/>→ Row、Column、Angle、Score、Model"]
  F --> G["依 Model 分組：每組換算、過濾、NMS（契約）"]
  G --> H["所有匹配依分數排序，合併成元件<br/>（交集 ÷ 較小的框 > 0.5 ＝ 同一個元件）"]
  H --> I["每個元件附每個類型的分數（由高到低）"]
```
