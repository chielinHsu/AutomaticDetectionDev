# S11 — 圖解：U3 的管線改前與改後

U3 ＝ 分數滑桿變動到重繪。三張圖：改前的管線（標出三個慢的節點）、改後的管線（標出改了什麼）、字形快取的細節。

## 1. 改前（S10 的程式，5 832 個提案物件，中位數）

```mermaid
flowchart LR
  S["QSlider::valueChanged<br/>ParamsPanel"] --> P["AutoRoiPresenter::applyThreshold<br/>session.setThreshold（重新過濾 0.5 ms）"]
  P --> B["SceneBuilder::buildScene<br/>每個物件：QString::number(分數)、<br/>QStringLiteral(&quot;#%1&quot;).arg(編號)<br/><b>3.5–5.5 ms</b>"]
  B --> C["ImageCanvas::showComponents<br/>RoiLayerItem::setSnapshot（分桶 0.8 ms）"]
  B --> T["ObjectsModel::setRows<br/>dataChanged 0.7 ms"]
  C --> PAINT["QGraphicsView 重繪整個視埠 14.2 ms"]
  PAINT --> IMG["ImageItem::paint<br/>影像縮放 1.1 ms"]
  PAINT --> LAYER["RoiLayerItem::paint<br/>5 832 框 × 8 個填色矩形<br/><b>12.5 ms</b>"]
  T --> TP["物件樹重繪 1.5 ms"]
  style B fill:#fde68a,stroke:#b45309,color:#000
  style LAYER fill:#fecaca,stroke:#b91c1c,color:#000
```

這張圖要讓你看到：U3 的 18.9 ms（中位數）裡，框層的繪製佔 12.5 ms、場景重建 3.5–5.5 ms；影像的縮放只有 1 ms（S04 的推測「畫布重繪是主因」對，但原因不是影像）。背景有已接受的元件時（U3-bg），`buildScene` 還要把文件裡的元件全部重算（展開框、名稱、列），多 9 ms（圖裡沒有畫，見第 2 圖的虛線節點）。

## 2. 改後（S11，同一個流程，標出不同的節點）

```mermaid
flowchart LR
  S["QSlider::valueChanged<br/>ParamsPanel"] --> P["AutoRoiPresenter::applyThreshold<br/>session.setThreshold"]
  P --> B["SceneBuilder::buildScene(session, labels, <b>&amp;cache</b>)<br/>提案：text ＋ score（不格式化）、列名留空<br/><b>0.9 ms</b>"]
  B -. 命中：文件的修訂號與標籤沒變 .-> CACHE["DocumentSceneCache<br/>文件元件的快照、列、計數<br/>（新增：優化 3）"]
  CACHE -. 共用，不重算 .-> B
  B --> C["ImageCanvas::showComponents<br/>RoiLayerItem::setSnapshot（分桶）"]
  B --> T["ObjectsModel::setRows<br/>displayName() 顯示時才組「#編號」<br/>（改：優化 2）"]
  C --> PAINT["QGraphicsView 重繪整個視埠 2.6 ms"]
  PAINT --> IMG["ImageItem::paint 0.9 ms"]
  PAINT --> LAYER["RoiLayerItem::paint<br/>每個框一次 drawImage（字形）<br/><b>1.15 ms</b>（改：優化 1）"]
  LAYER --> G["glyphFor(state, kind, w, h)<br/>快取沒有就用 BoxFrames 畫一張<br/>（新增：優化 1）"]
  LAYER -. 只對真的要畫的標籤 .-> L["ComponentView::label()<br/>畫的時候才格式化（改：優化 2）"]
  T --> TP["物件樹重繪"]
  style B fill:#bbf7d0,stroke:#15803d,color:#000
  style LAYER fill:#bbf7d0,stroke:#15803d,color:#000
  style G fill:#dbeafe,stroke:#1d4ed8,color:#000
  style CACHE fill:#dbeafe,stroke:#1d4ed8,color:#000
  style L fill:#dbeafe,stroke:#1d4ed8,color:#000
  style T fill:#bbf7d0,stroke:#15803d,color:#000
```

這張圖要讓你看到：節點的順序沒有變，只有三個地方不同——框層的畫法（字形）、場景重建不再做字串（延後到畫的時候與顯示的時候）、文件那一部分走快取。U3 中位數 18.9 → 6.5–8.8 ms、p95 23.5 → 7.6–11.2 ms（預算 16）。

## 3. 字形快取（優化 1）：一個桶怎麼畫

```mermaid
flowchart TB
  subgraph bucket["一個桶（狀態 × 種類）的可見框"]
    R1["框 1：螢幕矩形 → BoxFrames::snap → (x, y, w, h)"]
    R2["框 2 … 框 n（同一個樣本：w、h 幾乎都一樣）"]
  end
  R1 --> Q{"dpr ＝ 1 且 w、h ≤ 96？"}
  Q -- 是 --> K["key ＝ (state, kind, w, h)"]
  K --> H{"glyphs_ 有？"}
  H -- 沒有 --> MK["畫一張 (w+2)×(h+2) 的 ARGB 小圖：<br/>BoxFrames::append／appendDashed ＋ draw<br/>（外框、描邊；內部透明）"]
  MK --> H2["放進 glyphs_（最多 64 張，超過下次 paint 開始時清）"]
  H -- 有 --> BL["GlyphBlit{origin ＝ (x−1, y−1), &amp;image}"]
  H2 --> BL
  BL --> DRAW["resetTransform 之後：<br/>painter-&gt;drawImage(origin, image)<br/>Qt 的 fast_images 路徑（alpha 混合的 blit）"]
  Q -- 否 --> OLD["照舊：BoxFrames 的填色矩形（S02／ADR-012）"]
```

這張圖要讓你看到：字形與填色矩形用**同一套** `BoxFrames` 產生像素，差別只在「畫一次、貼 n 次」；不符合條件（高 DPI、太大的框）的框走原本的路。`tst_items` 對四種狀態 × 三種框、三種縮放逐像素比對兩條路的輸出相同。

## 4. 文件快取（優化 3）：什麼時候重算

```mermaid
sequenceDiagram
  participant U as 使用者
  participant P as AutoRoiPresenter
  participant B as buildScene
  participant D as InspectionDocument
  U->>P: 拖門檻
  P->>B: buildScene(session, labels, &cache)
  B->>D: revision()
  Note over B: cache.valid 且 revision 相同 且 labels 相同 → 命中
  B-->>P: 文件的快照與列共用上一次的；只重算提案
  U->>P: 接受／刪除／復原（Command）
  P->>D: accept／remove／undo
  D->>D: notify：revision ＋ 1
  P->>B: buildScene(…, &cache)
  B->>D: revision()
  Note over B: 不同 → 重算文件那一部分，更新 cache
  U->>P: 換語言
  P->>P: rebuildLabels()
  P->>B: buildScene(…, &cache)
  Note over B: labels 不同 → 重算（板子標題、「手動」的字）
```

這張圖要讓你看到：快取的鍵來自 application 層（修訂號只在變更通知時加 1），presentation 不自己猜文件有沒有變；語言切換也會讓它失效。
