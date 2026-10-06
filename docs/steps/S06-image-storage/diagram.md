# S06 — 圖解

七張圖：影像的所有權、開圖流程的修改前後、轉換與複製次數、影像來源的組合、配方的檔案格式、背景存檔、樣本庫的分層。文字說明在 [`spec.md`](spec.md)，決策在 [ADR-014](../../adr/014-image-ownership-and-copies.md)、[ADR-015](../../adr/015-recipe-and-library-storage.md)。

## 1. 影像資料的所有權：誰持有、誰借用

```mermaid
flowchart LR
  FILE["影像檔"] --> DEC["QImageReader 解碼<br/>（QtImageSource）"]
  DEC --> QIMG["QImage<br/>擁有像素"]
  QIMG -- "shared_ptr＜const QImage＞<br/>只加參照計數" --> OWNER(("owner"))
  OWNER --> V1["ImageView<br/>（Presenter 的 image_）"]
  OWNER --> V2["ImageView<br/>（交給 worker 的 SampleRequest）"]
  OWNER --> SUB["subView<br/>（樣本的子影像）"]
  OWNER --> WRAP["QImage 包裝<br/>（畫布 ImageItem::wrap，唯讀）<br/>清理函式持有 owner"]
  V1 -. "讀" .-> PX[("同一塊像素<br/>交出去之後不再改")]
  V2 -. "讀（worker 執行緒）" .-> PX
  SUB -. "讀" .-> PX
  WRAP -. "畫" .-> PX
```

讀這張圖要看到：像素只有一塊，擁有它的是解碼出來的 QImage；所有的 view 與包裝只持有 owner 的參照計數（ImageView 規則 2、3、5）。像素交出去之後沒有人再改（規則 1），所以 UI 執行緒畫圖、worker 執行緒搜尋可以同時讀，不加鎖。最後一個 view 或包裝消失時，像素才釋放。

## 2. 開啟影像：修改前與修改後

修改前（S02–S05）：

```mermaid
flowchart TD
  A1["MainWindow::openImage(path)"] --> B1["loadImageFile → QImage<br/>shell/ImageLoader.cpp：畫面層解碼、沒有 port"]
  B1 --> C1["canvas_->setImage(QImage)"]
  C1 --> D1["emit imageOpened(QImage, name)"]
  D1 --> E1["AutoRoiPresenter → toImageView(QImage)<br/>shell/ImageViewAdapter.cpp：暫代"]
  E1 --> F1["session_.imageOpened(name, w, h, 0.0)<br/>校正值固定 0"]
  classDef old fill:#fde2dd,stroke:#c0392b,color:#5a1a12
  class B1,E1,F1 old
```

修改後（S06）：

```mermaid
flowchart TD
  A2["MainWindow::openImage(path)"] --> B2["imageSource_->load(UTF-8 路徑)<br/>IImageSource（port，app 組裝）"]
  B2 --> CH{"ImageSourceChain<br/>只問認得檔名的來源"}
  CH -- "png、jpg、bmp…" --> QT["QtImageSource<br/>→ 正規格式，0 次複製"]
  CH -- "tif（Qt 讀不了）" --> CV["OpenCvImageSource<br/>（AOI_WITH_OPENCV=ON）"]
  QT --> VIEW["ImageView（有 owner）"]
  CV --> VIEW
  VIEW --> C2["canvas_->showImage(view)<br/>ImageItem::wrap：零複製"]
  C2 --> D2["emit imageOpened(ImageView, name)"]
  D2 --> F2["AutoRoiPresenter：image_ = view<br/>session_.imageOpened(name, w, h, view.umPerPixel)"]
  classDef changed fill:#dff3e4,stroke:#1e8449,color:#0b3d1c
  class B2,CH,QT,CV,VIEW,C2,D2,F2 changed
```

不同的地方（綠色）：解碼從畫面層移到 port 後面（`ImageLoader` 刪除）；訊號帶的是 `ImageView`，Presenter 不必再轉（`ImageViewAdapter` 刪除），拿到的就是畫布畫的那一塊像素；校正值從 view 來（影像來源給 0，配方或使用者之後補）。`imageOpened`／`imageOpenFailed` 這兩條路沒有變（S04 的提醒）。

## 3. 轉換路徑與複製次數

```mermaid
flowchart LR
  subgraph QT["Qt（infrastructure/image_qt）"]
    Q1["Grayscale8、Grayscale16、<br/>RGB32、ARGB32、RGB888、BGR888"] -- "fromQImage：0" --> VQ["ImageView"]
    Q2["索引色、單色、RGB16、<br/>16 bit 彩色、預先乘 alpha"] -- "fromQImage：1（計數）" --> VQ
  end
  subgraph CVG["OpenCV（backend_opencv）"]
    M1["8UC1、16UC1、8UC3、8UC4"] -- "fromMat：0" --> VC["ImageView"]
    VC2["ImageView（五種格式）"] -- "borrowAsMat：0<br/>借用、不可以寫" --> M2["cv::Mat"]
  end
  subgraph SRC["影像來源（正規格式）"]
    F1["彩色 PNG／JPEG／BMP"] -- "Qt：0" --> C1["Bgrx32"]
    F1 -- "OpenCV：1（BGR → BGRx）" --> C1
    F2["16 bit 彩色"] -- "Qt：1、OpenCV：1–2" --> C1
    F3["灰階、16 bit 灰階、12 bit 灰階、TIFF 灰階"] -- "0" --> C2["Gray8／Gray16<br/>（附有效位元數）"]
  end
  subgraph UI["畫面（presentation）"]
    VU["ImageView（五種格式）"] -- "ImageItem::wrap：0" --> QU["QImage（唯讀）"]
    V12["Gray16，12 bit<br/>（值只到 4095）"] -- "ImageItem::wrap：1<br/>轉 8 bit 顯示" --> Q8["QImage Grayscale8"]
  end
  subgraph HL["HALCON（S08，未驗證）"]
    H1["Gray8／Gray16 緊密排列"] -. "預期 0" .-> HI["HImage"]
    H2["Bgrx32（交錯）"] -. "預期 1（分平面）" .-> HI
  end
```

讀這張圖要看到：「0」是測試用指標相等與計數斷言的；「1」的每一個都呼叫 `countImageCopy`。12 bit 的影像（使用者的影像有這種）像素照原樣交給搜尋，只有畫面顯示前轉一次 8 bit。OpenCV 讀彩色檔要多一次複製，是組裝時 Qt 排在前面的理由（ADR-014）。HALCON 那一格是虛線：S06 沒有 HALCON 的開發檔，只是預期。

## 4. 影像來源的組合（`ImageSourceChain::load`）

```mermaid
flowchart TD
  START["load(path)"] --> ANY{"有來源的 canRead(path) 是 true？<br/>（只看副檔名，不碰檔案）"}
  ANY -- "有" --> TRY["依順序問下一個認得的來源"]
  ANY -- "沒有" --> LAST["交給最後一個來源試一次<br/>（看內容決定格式）"]
  TRY --> R{"結果"}
  LAST --> R
  R -- "成功" --> OK["回傳 ImageView"]
  R -- "NotFound、IoFailed、其他" --> STOP["立刻回傳：換解碼器也讀不到檔案"]
  R -- "FormatInvalid" --> MORE{"還有認得的來源？"}
  MORE -- "有" --> TRY
  MORE -- "沒有" --> ALL["FormatInvalid<br/>訊息依序列出每個來源的原因"]
```

## 5. 配方的檔案格式（schema v1）

```mermaid
classDiagram
  class RecipeFile {
    format = "aoi-recipe"
    schemaVersion = 1
    board
    params
  }
  class Board {
    name : 影像檔名
    imageWidth, imageHeight : 像素
    umPerPixel : 0 = 未校正
    types[]
    components[]
  }
  class ComponentType {
    id、libId、partNumber
    aliases[]、name
    category : chip | ic | other
    groups[]
    referenceImage : null 或 base64
  }
  class BoxGroup {
    id
    kind : body | pin | window
    widthUm, heightUm, centerXUm, centerYUm
    matrix : rows, cols, pitch, skipped[]
    window : parentGroupId, 比例, algo
  }
  class ComponentInstance {
    id、name
    pose : x, y, angleDeg（像素、度）
    side : top | bottom
    typeId
    state : sample | found | inferred | rejected
    score : 數字或 null
    origin : auto | manual
  }
  class SearchParams {
    minScore
    angleMinDeg, angleMaxDeg
    maxMatches
  }
  RecipeFile --> Board
  RecipeFile --> SearchParams
  Board "1" --> "*" ComponentType : 板子自己的複本
  Board "1" --> "*" ComponentInstance
  ComponentInstance ..> ComponentType : typeId
  ComponentType "1" --> "*" BoxGroup
```

讀這張圖要看到：每個欄位都寫進檔案（用不到的也寫），所以讀回來逐欄位相同；元件的 `typeId` 必須指到同一塊板子的類型（讀寫都檢查）；類型與框群組是 µm，元件位姿是像素。樣本庫的檔案是 `{format: "aoi-library", schemaVersion, nextEntryNumber, entries: [ComponentType…]}`。

## 6. 背景存檔（`RecipeSaver`）：同一個檔案依序寫

```mermaid
sequenceDiagram
  participant UI as UI 執行緒
  participant RS as RecipeSaver
  participant W as worker（IExecutor）
  participant ST as JsonRecipeStore
  UI->>RS: save(快照 v1, a.json)
  RS-->>UI: 編號 1（立刻回來）
  RS->>W: post(寫 a.json)
  W->>ST: save(v1)
  UI->>RS: save(v2, a.json)
  RS-->>UI: 編號 2（排在後面）
  UI->>RS: save(v3, a.json)
  RS-->>UI: 編號 3（取代還沒開始的 v2）
  RS->>W: post(通知 2：Cancelled)
  ST-->>W: v1 寫完（暫存檔 → 換名）
  W->>UI: done(1, 成功)（呼叫端自己 post 回 UI）
  W->>ST: save(v3)
  ST-->>W: v3 寫完
  W->>UI: done(3, 成功)
  Note over ST: 磁碟上最後是 v3：最後交出的那一份
```

## 7. 樣本庫的分層（TRI；使用者 2026-10-06 決定）

```mermaid
flowchart TD
  subgraph LAYERS["LayeredLibraryStore：查找順序由上而下"]
    P["專案層 proj-*<br/>（和配方放在一起，可寫）"]
    U["使用者層 user-*<br/>（使用者自己的，可寫，可複製到別台）"]
    S["系統層 sys-*<br/>（原廠，唯讀）"]
    P --> U --> S
  end
  FIND["find(料號或別名)"] --> P
  SAVE["save(類型)"] --> Q1{"哪一個可寫層<br/>已經有這個料號？"}
  Q1 -- "最高的那一層" --> TGT["存到那一層（同料號覆寫、保留 id）"]
  Q1 -- "都沒有" --> Q2{"libId 指到的那一層可寫？<br/>（從專案層載入的）"}
  Q2 -- "是" --> TGT2["存回那一層"]
  Q2 -- "否（新的、或從系統層來的）" --> TGT3["存到使用者層"]
  BOARD["板子（配方）<br/>自己的類型複本，記得 libId"] -. "〔存入樣本庫〕" .-> SAVE
  FIND -. "〔從樣本庫更新〕" .-> BOARD
```

讀這張圖要看到：查找從專案層往下，第一個找到的就是答案（上層遮住下層同料號的條目）；存檔的三條規則照 TRI（「從 Project 載入的存回 Project，其他存到 User」），再加一條「已經有這個料號的最高可寫層優先」，存完 `find(料號)` 才一定是剛存的內容。板子的類型複本不屬於任何一層（在配方裡，ADR-006 §5）。
