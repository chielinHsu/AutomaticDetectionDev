# S07 — 圖解

六張圖：OpenCV adapter 的結構、搜尋管線（由粗到細）、use case 與搜尋器的序列、金字塔的座標換算、有效位元數的修改前後、執行緒。文字說明在 [`spec.md`](spec.md)，決策在 [ADR-016](../../adr/016-opencv-coarse-to-fine-search.md) 與 [ADR-014 的 S07 後續](../../adr/014-image-ownership-and-copies.md)，數字在 [`perf.md`](perf.md)。

## 1. 結構：哪個類別實作哪個 port

```mermaid
flowchart LR
  subgraph application["application（port，沒有改）"]
    ITB["ITemplateBuilder"]
    IPS["IPatternSearcher"]
    ILM["ILibraryMatcher"]
  end
  subgraph opencv["infrastructure/backend_opencv（S07 新增的部分）"]
    TB["OpenCvTemplateBuilder<br/>裁樣本、金字塔、旋轉範本"]
    T["OpenCvTemplate<br/>每層 × 每個角度的範本<br/>ΣT、ΣT²"]
    PS["OpenCvPatternSearcher<br/>由粗到細、視窗清單"]
    LM["OpenCvLibraryMatcher<br/>流程 B：每個類型建範本 → 找 → 合併"]
    CO["OpenCvCorrelation<br/>小的分數圖自己算"]
    PY["OpenCvPyramid<br/>金字塔、座標換算"]
    IM["OpenCvImages::toGray8<br/>轉 8 bit 灰階（計數）"]
    BE["OpenCvBackend<br/>工廠、setOpenCvThreads"]
  end
  APP["app/CompositionRoot<br/>registerBackend opencv"]
  TB -. 實作 .-> ITB
  PS -. 實作 .-> IPS
  LM -. 實作 .-> ILM
  TB --> T
  PS --> T
  PS --> CO
  PS --> PY
  TB --> PY
  PS --> IM
  TB --> IM
  LM --> TB
  LM --> PS
  BE --> TB
  BE --> PS
  APP --> BE
```

讀這張圖要看到：S07 只在 `infrastructure/backend_opencv` 加類別，並在組裝根多註冊一個 back end；port、use case、domain 沒有為了 OpenCV 改（開放封閉，`check-deps` 0 違規）。範本建立與搜尋是兩個類別（單一職責），共用的只有範本的資料、金字塔與換算。`OpenCvLibraryMatcher` 由建立器與搜尋器組成，UI 還沒有接（使用者決定）。

## 2. 搜尋管線：由粗到細

```mermaid
flowchart TD
  IN["find(image, template, region, params)"] --> AREA{"region"}
  AREA -- "整張影像" --> A1["一塊區域：整張"]
  AREA -- "視窗清單" --> A2["每個視窗一塊：視窗外加範本的一半<br/>每塊最多 4 個候選（大的區域依面積放寬）"]
  A1 --> G["toGray8：Gray8 零複製<br/>Gray16 ×255／(2^位元數−1)、彩色轉灰階（各算 1 次複製）"]
  A2 --> G
  G --> P["金字塔：pyrDown 到頂層<br/>（64×32 的範本 → 3 層）"]
  P --> TOP["頂層：每個角度一次 matchTemplate（TM_CCOEFF_NORMED）<br/>橫帶平行；每個位置取最好的角度"]
  TOP --> PEAK["局部最大值 ≥ minScore − 0.2 → 候選<br/>NMS（範本大小、IoU 超過 0.5）"]
  PEAK --> REF["往下每一層、每個候選（平行）：<br/>位置 ± 2 像素 × 角度 ± 一步<br/>分數圖小 → 自己算（積分影像）<br/>最好的點在邊上 → 移過去再比（爬坡）"]
  REF --> L0{"第 0 層？"}
  L0 -- 否 --> REF
  L0 -- 是 --> SUB["次像素：x、y 各三點拋物線<br/>角度：相鄰兩個角度的分數拋物線"]
  SUB --> FIL["分數 ≥ minScore、中心在區域內、角度夾回範圍"]
  FIL --> NMS["domain 的 NMS → 依分數排序（同分依 y、x）→ 最多 maxMatches"]
```

讀這張圖要看到：昂貴的「每個角度掃整張影像」只發生在頂層（縮小 4 倍的 64×32 範本是 16×8、影像是 1/16）；往下每一層只在候選附近比幾十個位置。精修的分數圖很小，`matchTemplate` 對它仍走 DFT，固定成本主宰，所以自己算（ADR-016 §2，T2a 從 193 降到 64 ms）。

## 3. use case 與搜尋器的序列（流程 A，沒有改 use case）

```mermaid
sequenceDiagram
  participant P as AutoRoiPresenter（UI 執行緒）
  participant J as JobRunner worker
  participant U as GenerateObjectsFromSample
  participant B as OpenCvTemplateBuilder
  participant S as OpenCvPatternSearcher
  participant CV as cv::parallel_for_（OpenCV 的池）
  P->>J: submit（SampleRequest：影像 view、樣本框、角度）
  J->>U: run(request, cancel)
  U->>U: 對比檢查（effectiveBits 換到 0..255）
  U->>B: build(image, sample, params)
  B-->>U: OpenCvTemplate（3 層 × 角度）
  U->>S: find(整張影像, minScore 0.70)
  S->>CV: 頂層的橫帶、精修的候選
  CV-->>S: 匹配
  S-->>U: 匹配（已排序）
  U->>U: NMS → 晶格（domain）→ 缺格
  U->>S: find(視窗清單, minScore 0.30)
  S->>CV: 每個視窗
  S-->>U: 匹配
  U->>U: 每個缺格留最好的、O(1) 重新擬合
  U-->>J: SampleProposal（不可變）
  J-->>P: UiPoster 排回 UI 執行緒
```

讀這張圖要看到：use case（S03–S05）一行都沒改，換成 OpenCV 只是組裝根給的 builder、searcher 不同。搜尋器內部的平行用 OpenCV 自己的池；JobRunner 的 worker 只是發起的執行緒（它也會參與 `parallel_for_` 的工作）。

## 4. 金字塔的座標換算

```mermaid
flowchart LR
  L0["第 0 層的連續座標 x₀<br/>（ADR-010：像素中心在 +0.5）"] -- "x₁ ＝ (x₀ ＋ 0.5) ／ 2" --> L1["第 1 層 x₁"]
  L1 -- "x₂ ＝ (x₁ ＋ 0.5) ／ 2" --> L2["第 2 層 x₂"]
  L2 -- "x₁ ＝ 2·x₂ − 0.5" --> L1
  L1 -- "x₀ ＝ 2·x₁ − 0.5" --> L0
  L2 --> MT["matchTemplate 給左上角的整數索引 i<br/>範本中心 ＝ i ＋ w／2（連續座標）"]
```

讀這張圖要看到：`cv::pyrDown` 的第 l＋1 層像素 i 以第 l 層像素 2i 為中心，所以每往上一層是「加 0.5、除以 2」。這個換算與 OpenCV 的「像素中心在整數」都在 adapter 裡處理（ADR-010 的要求）；`tst_opencv_backend` 用已知位置的合成元件鎖住（誤差 < 0.25 像素），`tst_opencv_end_to_end` 的位置誤差最大 0.21 像素。

## 5. 影像的有效位元數：修改前與修改後

修改前（S06）：

```mermaid
flowchart TD
  F1["影像來源 load()<br/>significantBits ＝ detectSignificantBits（看最大值）"] --> W1["MainWindow::setImage → 畫布 showImage(view)"]
  W1 --> P1["AutoRoiPresenter::onImageOpened<br/>image_ ＝ view（推測的位元數）"]
  P1 --> U1["搜尋：SampleRequest.image ＝ image_<br/>對比檢查用推測的位元數"]
  W1 --> C1["ImageItem：右移（推測 − 8）"]
  classDef bad fill:#fde2dd,stroke:#c0392b,color:#5a1a12
  class P1,U1,C1 bad
```

修改後（S07）：

```mermaid
flowchart TD
  F2["影像來源 load()<br/>significantBits ＝ 推測（沒有改）"] --> W2["MainWindow::setImage → 畫布 showImage(view)"]
  W2 --> P2["AutoRoiPresenter::onImageOpened<br/>sourceImage_ ＝ view"]
  P2 --> D2["decideSignificantBits(格式, 推測, Board::significantBits)<br/>domain/image_depth：0 ＝ 自動、比推測小 → 不收"]
  D2 -- "和推測不同" --> R2["image_ ＝ withSignificantBits（零複製）<br/>畫布 showImage(image_) 重畫"]
  D2 -- "一樣" --> K2["image_ ＝ view"]
  R2 --> U2["搜尋：對比檢查、OpenCV 轉 8 bit 都用 image_ 的位元數"]
  K2 --> U2
  PP["參數面板「影像位元數」<br/>→ ParamsPresenter → applySignificantBits"] --> SET["InspectionDocument::setSignificantBits<br/>（不是 Command；reset 時保留）"]
  SET --> D2
  SET --> RC["配方 schema v2：board.significantBits"]
  classDef changed fill:#dff3e4,stroke:#1e8449,color:#0b3d1c
  class P2,D2,R2,K2,U2,PP,SET,RC changed
```

不同的地方（綠色）：Presenter 保留來源給的原樣（`sourceImage_`），依板子的設定決定 `image_` 的位元數；畫布、use case、OpenCV 搜尋器都讀 `image_` 的欄位，所以它們自己不必改。設定存在 `Board`，開新影像（`InspectionDocument::reset`）時保留，存配方時一起存（v2）。影像來源與 `MainWindow::setImage` 沒有改（設定和推測一樣時不重畫）。

## 6. 執行緒

```mermaid
flowchart LR
  UI["UI 執行緒<br/>（保留一個核心）"]
  subgraph pool1["JobRunner 的池：核心數 − 1 個 worker"]
    J1["目前的搜尋"]
    J2["被取代的舊搜尋<br/>（下一個檢查點就停）"]
  end
  subgraph pool2["OpenCV 的池：cv::setNumThreads(核心數 − 1)"]
    C1["頂層的橫帶"]
    C2["精修的候選"]
    C3["視窗"]
  end
  UI -- "submit" --> J1
  J1 -- "parallel_for_" --> pool2
  J2 -. "cancel 之後很快結束" .-> pool2
  ROOT["組裝根 app::workerThreads()<br/>一個函式決定兩個數字"] --> pool1
  ROOT --> pool2
```

讀這張圖要看到：同一時間通常只有一個搜尋在跑（latest-wins），它的平行部分在 OpenCV 的池裡；兩個池的大小由同一個函式決定，改一處就好（ADR-016 §4）。實測 T2a（中位數）：1 個執行緒 126–133 ms、15 個 50–57 ms（`perf.md`）。
