# AOI UX 規格（S01）

> 這份是 S01 的設計本體：人物、概念模型、流程、線框、狀態語言、互動規則、回饋時間、文案、主題、Presenter 介面名單。
> 機器可讀的部分在同資料夾：[`tokens.json`](tokens.json)（顏色、線型、尺寸、時間）、[`copy.json`](copy.json)（繁中＋英文文案，i18n key）。
> 可點擊原型：[`../steps/S01-ux-spec/explainer.html`](../steps/S01-ux-spec/explainer.html)（本規格的每條規則都在原型裡能操作）。
> 決策理由：[ADR-001 UI 工具集](../adr/001-ui-toolkit.md)、[ADR-002 復原模型](../adr/002-undo-model.md)、[ADR-006 樣本庫與階層](../adr/006-sample-library-and-hierarchy.md)。

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-14 | Claude Code (Claude Fable 5.1) | 使用者回報「編輯只能拉框，要能用數字改樣本的長寬、讓幾個樣本一致；建立樣本時要看得到樣本的圖」（ADR-025 的「後續（2026-10-14）」）：第 4 節的**選取資訊**表單實作了（物件樹下方：名稱與類型、中心 X／Y、寬 × 高（有校正值時旁邊顯示 µm）、角度、框數；中心與寬高可以改，按 Enter 或離開欄位才生效，一次一個 Command；〔套用大小到所有樣本〕、〔鎖定寬高比〕；多選時停用、預覽中只能改位置），**樣本庫 dock** 列出板子上的樣本（縮圖疊上框、名稱、寬 × 高、框數；點一筆＝選取它；資料只來自板子，不讀寫樣本庫檔案）。第 8 節新增三列。`copy.json` 新增 20 個 key |
| 2026-10-15 | Claude Code (Claude Opus 5.5) | 比對區（[ADR-026](../adr/026-match-region-separate-from-cad-box.md)）：第 8 節新增 `M`（拖框＝矩形比對區、點一下＝自動擷取）；選取資訊表單多「比對區」一組（狀態、特徵、外擴、清除）。狀態機沒有新事件（和 `B` 一樣是 `Idle` 的 Command）。`copy.json` 新增 20 個 key |
| 2026-10-14 | Claude Code (Claude Fable 5.1) | 使用者決定（[ADR-025](../adr/025-sample-creation-separate-from-search.md)）：**樣本的建立與搜尋分開**——`S` 放開只建立樣本（一個類型＋一個樣本元件，回到 `Idle`），〔搜尋〕（`F5`）用板子上全部樣本各搜一次整張板子、結果依位置合併（同位置留分數最高的），接受時上一次自動產生的元件整批換掉；第 3 節的流程 A、第 6 節新增 `ReleaseSample`、`SearchRequested` 兩個事件、第 8 節新增 `F5`、`B`（子框）、把手改大小（改類型、同類型一起）、框選、`Ctrl+A`、`Ctrl+Shift+A`。`copy.json` 新增 17 個 key、改 `ui.idle.hint`、`ui.sample.hint` |
| 2026-10-09 | Claude Code (Claude Opus 5.5) | 使用者要求修掉 session B 的已知問題：（1）搜尋時 back end 無法使用的錯誤卡片：標題是 back end 的名稱、原因在做法清單，加上第 10 節原本就寫的〔改用 …〕（建議可用的、最完整的另一個，HALCON 不能用時是 OpenCV），換好之後用新的 back end 再試一次；（2）切換失敗的卡片多〔再試一次〕（插上 dongle 之後不必重新啟動程式）；（3）`copy.json` 刪掉不再使用的 `ui.status.summary`、`ui.status.none`，新增 `ui.backend.switchTo`（[ADR-021](../adr/021-runtime-backend-switching.md) 的「後續（2026-10-09）」） |
| 2026-10-08 | Claude Code (Claude Opus 5.5) | 使用者決定（S09 之前的 session B，第 2 件）：上方的 back end 選單可以在執行中切換（[ADR-021](../adr/021-runtime-backend-switching.md)）。切換在背景建立（HALCON 第一次約 1.5 秒），期間選單停用、狀態列「切換到 HALCON…」；搜尋中先取消；**畫面上有提案時用新的 back end 重新搜尋同一個區域**；失敗時回到原本的 back end，訊息列的卡片（`ui.error.backend.*`＋原因＋「仍使用 …」）按〔關閉〕才收起。**啟動時用上次用的**（沒有記錄時 fake；`--backend` 照舊優先）。第 12.3 節的 `IBackendController::selectBackend` 實作了；`copy.json` 新增 `ui.backend.switching`、`ui.backend.reverted`。換 back end 不進復原堆疊（ADR-002 規則 6） |
| 2026-10-08 | Claude Code (Claude Opus 5.5) | 使用者決定（S09 之前的 session B，第 1 件）：back end 的提醒（例如 HALCON 用的是 runtime 授權）改成訊息列的**提醒卡片**（不會自己消失，按〔關閉〕才收起；搜尋的卡片與進度列優先，它們收起之後再出現）與**狀態列最左邊的 back end 標示**（「HALCON · runtime 授權」，滑鼠移上去看完整說明，搜尋中也看得到）；原本的 4 秒吐司拿掉，stderr 那一行保留（第 5、9 節）。狀態列的摘要因此不再重複 back end 名稱。`copy.json` 新增 5 個 key（`ui.backend.halcon.runtimeLicense`、`ui.backend.notice.title`、`ui.backend.status.tip`、`ui.status.counts`、`ui.status.noObjects`）；`ui.status.summary`、`ui.status.none` 不再使用（只新增 key，沒有刪）。第 12.2 節多了 `IBackendView`；第 12.3 節的 `IBackendController` 由新的 `BackendPresenter` 實作（第 12.4 節原本放在 `ParamsPresenter`），多一個 `applyNoticeAction`（提醒卡片上的按鈕） |
| 2026-10-06 | Claude Code (Claude Opus 5.5) | 使用者要求：新增〔適配寬度〕（`W`，「檢視」選單與上方工具列），規則見第 8 節；`copy.json` 新增 `action.fitWidth`。載入影像的預設仍是適配視窗 |
| 2026-10-06 | Claude Code (Claude Sonnet 5.5) | S04 實作回饋：`copy.json` 新增 12 個 key（`ui.history.*`、`ui.preview.nothingToAccept`、`ui.editor.unavailable`、`ui.objects.col.*`），移除 5 個 S02 的暫代 key（`ui.shell.regionDrawn`、`addRequested`、`openEditor`、`deleteRequested`、`backendNone`）。第 12.2、12.3 節的介面名單與實作不同之處：卡片統一成 `IMessageView::showCard／hideCard`（預覽的動作列、無匹配、錯誤是同一種卡片，沒有 `showEmpty`）；多了 `ICanvasInputGate`（搜尋中畫布暫停）與 `IKeyboardController`；沒有 `IBackendController`（S04 只有一個可用的 back end，執行中不能切換）；第 12.4 節的六個 Presenter 在 S04 只做 `AutoRoiPresenter` 與 `ParamsPresenter`（[ADR-011](../adr/011-interaction-logic-in-application-layer.md)）。第 9 節「忙碌標示一旦出現至少停留 400 ms」只適用於**結果回來**；使用者自己取消時標示馬上收起（否則 Idle 的畫面上會留著「搜尋中…」）。第 7 節的虛線與點線改用整數像素的填色矩形畫（[ADR-012](../adr/012-dashed-frames-as-fill-rects.md)，外觀不變，快約 17 倍）。第 8 節「拖曳元件」與方向鍵微調（S02 沒做的兩項）在 S04 做了 |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | S02 實作回饋：訊息列改為**固定高度**（`tokens.json` 新增 `sizes.messageBarHeight` ＝ 96 px，放不下在列內捲動），否則「畫布大小不變」在桌面程式做不到（第 5 節；[ADR-008](../adr/008-message-bar-fixed-height.md)）。`copy.json` 新增殼層用的 key（`menu.*`、`action.quit`、`ui.shell.*`、`ui.status.cursor`；[ADR-007](../adr/007-copy-json-as-translation-source.md)）。框的粗細在整數像素下取整：Pin 的「狀態寬度 − 0.5」在畫面上是狀態寬度向下取整（Found：主體 2、Pin 1、窗 1），第 7 節的寬度是設計值（`docs/steps/S02-ui-shell/spec.md`） |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | 建立（S01）。同日依使用者回饋與 TRI 參考文件大幅修訂：新增概念模型（Board › Component › Box）、流程 B（區域掃描）、元件類型編輯器、樣本庫；狀態機由 7 個改為 8 個 |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | 使用者回饋：無匹配訊息蓋在板子上會擋到料件。所有要閱讀的訊息改放在畫布下方的**訊息列**（新規則，第 5 節） |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | 使用者問「樣本庫很多不是要比很久」：新增**比對範圍**（先用板子已有的類型，找不到再擴大）、狀態列顯示比對了幾個樣本；預覽時加上〔接受〕〔接受並存入樣本庫…〕〔捨棄〕按鈕（原本只有 Enter 鍵）；第 8、9、12、14 節隨之修訂 |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | 使用者決定：庫與板子是**複本**關係（板子保存自己的類型複本）、庫存 µm；四項 UX 建議採用。編輯器、樣本庫、選取資訊與介面名單隨之修訂（第 2、3、8、12、14 節） |

## 1. 人物與主要任務

AOI 配方工程師。每天在畫布上工作，重複性高，要求快、可預期、可反悔。四件事：

| # | 任務 | 成功的樣子 |
|---|---|---|
| 1 | 教一個新元件 | 框一個樣本，3 個動作內得到整排元件，並能存入樣本庫 |
| 2 | 用樣本庫處理一塊區域 | 框任意區域，程式辨識出裡面所有認得的元件，列出每個元件的**全部**檢測框 |
| 3 | 維護元件類型 | 一次建立 Pin 矩陣，改一個參數，這個類型的所有元件一起更新 |
| 4 | 快速驗證整批結果 | 狀態列與物件樹一眼看出數量與信心，低分的有提示；能匯出 CAD 清單 |

## 2. 概念模型：Board › Component › Box

```
Board 板子          影像、名稱
 └─ Component 元件   一筆 CAD 記錄：名稱 R12／U1、位置、角度、正反面、元件類型、比對狀態與分數
     └─ Box 框       由「元件類型」與「元件位姿」推導出來的矩形

ComponentType 元件類型（= 樣本庫條目）   料號 8371、別名、名稱、類別（Chip／IC／Other）、參考影像
 └─ BoxGroup 框群組   Body（每個類型恰好一個）｜Pin（矩陣）｜Window（連結到父群組）
```

| 名詞 | 定義 | 例子（原型的預設樣本庫） |
|---|---|---|
| **Component 元件** | 板上的一個零件實例；它有名稱與位姿，**框不屬於它自己**，而是從它的類型推導 | `U1`，類型 8371，位置 (210, 675)，0° |
| **ComponentType 元件類型** | 一種料號的定義；樣本庫裡的條目；改它，所有同類型元件一起更新 | `8371` SOIC-20，別名 SOP20、20-SOIC，類別 IC |
| **Body** | 元件本體框，每個類型恰好一個，定義元件的大小與中心；其他框的座標都相對於它的中心 | 200 × 110 |
| **Pin** | 腳位框的**矩陣**：列數、欄數、水平／垂直間距。改列數／欄數＝一次建立或刪除一整排框 | 2 列 × 10 欄 ＝ 20 支腳 |
| **Window 檢測窗** | 連結到一個父群組（Body 或 Pin）；大小與位置用**父框的比例**表示；每個父框的格子各有一個窗。父框改，窗跟著變 | Solder 窗：寬 140%、高 200% 的 Pin ＝ 20 個窗 |

「8371 的檢測框是主體 ＋ 一堆 Pin 腳」就是：類型 8371 ＝ 1 個 Body ＋ 1 個 Pin 矩陣 ＋ 1 個 Solder 窗群組＝ 41 個框。

**庫與板子是複本關係**（使用者決定）：樣本庫是類型的母版；板子保存自己用到的類型的**複本**，並記得來源（`libId`）。接受提案時把用到的庫類型複製進板子；之後改庫不會改到舊板子。同步只透過兩個明確動作：〔從樣本庫更新〕與〔存入樣本庫〕。刪除庫條目不影響已有的板子。

這個模型和 TRI 的做法對照見 ADR-006。**狀態**（Sample／Found／Inferred／Rejected）屬於**元件**（比對結果），元件的所有框用同一個狀態的樣式畫出來。

## 3. 三個流程

| 流程 | 工具 | 使用者做什麼 | 程式做什麼 |
|---|---|---|---|
| **A 教樣本** | `S`，再按〔搜尋〕（`F5`） | 在一個元件（或一塊區域）上框一個樣本；可以框好幾個；可用 `B` 在樣本上畫子框、拖把手改大小 | **2026-10-14 起（ADR-025）**：放開只**建立樣本**（一個類型＋一個樣本元件，不搜尋）。按〔搜尋〕時板子上的**每個樣本各搜一次**整張板子（再用晶格補漏），結果**依位置合併**（同一個位置留分數最高的）→ Preview。接受後上一次自動產生的元件整批換成這一次的，每個元件指向它的樣本的類型（框由類型展開，子框一起貼上）；可〔接受並存入樣本庫…〕 |
| **B 區域掃描** | `R` | 框任意區域（可含很多元件） | 把區域跟樣本庫的類型比對（**預設先比板子已有的類型，找不到再擴大到整個庫**，見第 9 節）→ 每個認得的元件成為一個元件實例，附候選類型分數。庫裡沒有的元件只計數，並建議〔教一個新樣本〕 |
| **C 編輯元件類型** | 雙擊元件，或 `E`（編輯**板子的複本**）；樣本庫〔編輯〕（編輯**庫條目**） | 畫 Body、Pin 矩陣、加檢測窗；可選「只改這一個矩陣」或「此類型中所有相同種類的框」 | 即時預覽。儲存板子的複本＝板上所有同類型元件一起更新（可同時存入樣本庫）；儲存庫條目＝板上已有的複本不變 |

「三個動作教一個元件」（2026-10-14 起）：**拖一個樣本框、按 F5、按 Enter**（流程 A；開啟影像之後）。「框一下就出現全部框」：**R 拖一個區域、按 Enter**（流程 B，前提是庫裡已有樣本）。

流程圖在 [`diagram.md`](../steps/S01-ux-spec/diagram.md) §1。

## 4. 版面

左側工具列、中央畫布、右側 dock、底部狀態列。工具只有 **V（選取／移動）、S（教樣本）、R（區域掃描）、＋（補一個元件）**；刪除用「選取後按 Del」，所以沒有「−」。畫布有兩個場景：**板子**與**元件類型**（編輯器）。

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Open Save  Back end:[OpenCV ▾]               ↶ Undo  ↷ Redo  ⤢ Fit  Export │
├────┬───────────────────────────────────────────────────────┬─────────────┤
│ V  │                                                       │ Sample      │
│ S  │                    image canvas                       │  library    │
│ R  │         (board scene  /  component-type scene)        │ ───────────  │
│ +  ├───────────────────────────────────────────────────────┤ Board       │
│    │ message bar: hint · progress · no-match · error · toast │  ▸ R1 5102  │
│    │ (always under the image, never on top of it)          │  ▾ U1 8371  │
│    │                                                       │     Body …  │
│    │                                                       │ ───────────  │
│    │                                                       │ Match params│
├────┴───────────────────────────────────────────────────────┴─────────────┤
│ OpenCV · 143 ms · Found 118 · Inferred 2 · 410 boxes        x y   Zoom 62% │
└──────────────────────────────────────────────────────────────────────────┘
```

畫布正下方是**訊息列**：所有要閱讀的訊息（提示、進度、無匹配、錯誤、吐司）都在這裡，不蓋在影像上（見第 5 節的規則）。右側 dock 有三塊：**樣本庫**（縮圖、料號、別名、框數、板子上的複本數、〔編輯〕〔刪除〕、篩選）、**物件樹**（Board › Component › 框群組；篩選晶片；選取資訊與辨識候選）、**比對參數**。進入類型編輯器時，三塊換成**編輯器面板**（料號、名稱、別名、類別、框群組清單、表單、儲存按鈕、〔取消〕）。選到一個元件時，物件樹下方的**選取資訊**顯示它的類型與同步狀態（「與樣本庫相同／不同／只在這塊板子上」）和對應的〔從樣本庫更新〕〔存入樣本庫〕。

**2026-10-14 起實作的部分**（ADR-025 的「後續（2026-10-14）」；同步狀態與樣本庫檔案仍是 P2）：

- **選取資訊**（`ObjectsPanel` 的表單，物件樹下方）：恰好一個元件被選取時顯示名稱與類型（只顯示）、中心 X、Y（像素，可改＝移動這一個元件）、寬 × 高（像素，可改＝改類型的主體框、中心不動、同類型全部一起；影像有校正值時旁邊同時顯示 µm）、角度與框數（只顯示）。**按 Enter 或離開欄位才生效**，一次輸入一個 Command（ADR-002 規則 7）；小於 8 像素不收、吐司提示，表單的數字回到實際的值。〔套用大小到所有樣本〕把目前的寬高套到板子上每一個樣本的類型（一個 Command、可復原；已經一樣的類型略過）。〔鎖定寬高比〕勾選時改寬高跟著變，只是表單的行為。多選時表單停用、只顯示「已選 N 個」；預覽中的物件只能改 X、Y（改大小只在 `Idle`，ADR-025 §4），表單下方說明原因。
- **樣本庫 dock**（`SamplePanel`）：列出**板子上的樣本**（`InspectionDocument::samples`）：每一筆是從目前影像裁下來的縮圖（主體框外加 25% 的邊界、縮到 96 px 寬、疊上主體框與子框）、名稱、類型、寬 × 高（像素；有校正值時加 µm）、框數。點一筆＝在畫布上選取它（和點畫布相同）。建立、刪除、改大小、加子框、復原時即時更新（跟著文件的修訂號走；門檻拖動不重做）。沒有樣本時照舊顯示「樣本庫是空的」；篩選欄仍是停用的佔位。資料只來自板子，不讀寫樣本庫檔案（存檔是 P2）。

## 5. 畫面與線框

互動狀態機有 8 個狀態；「無匹配」與「錯誤」共用一個線框（畫布上的訊息卡），所以是 7 個畫面。原型的「快速跳到」按鈕可直接看到每一個。

| # | 畫面 | FSM 狀態 | 畫布內容 | 其他區域 |
|---|---|---|---|---|
| 1 | 空白 | `Empty` | 置中卡片：標題、說明、〔開啟影像…〕 | 工具與 Fit 停用 |
| 2 | 已載入影像 | `Idle` | 影像（適配視窗）；已接受的元件顯示在影像上 | 訊息列的提示「S 教一個新樣本 · R 框選區域，用樣本庫找出元件」；樣本庫與物件樹可用 |
| 3 | 拖曳區域中 | `DrawingRegion` | 橡皮筋：S＝藍色樣本框，R＝白色細線區域（淡填色）；尺寸標籤 `60 × 32 px` | 其他操作暫停 |
| 4 | 搜尋中 | `Searching` | 保留拖曳的框；> 200 ms 狀態列「搜尋中…」；> 1 s **訊息列**出現進度條＋〔取消 (Esc)〕 | 進階參數可改（重新搜尋，最新的優先） |
| 5 | 預覽 | `Preview` | 元件的全部框，依狀態樣式；選取的元件有外框；已掃描的區域留著細線 | 物件樹列出元件；選取元件時顯示辨識候選；低信心提示；若區域內有未辨識的元件，顯示〔教一個新樣本〕 |
| 6 | 無匹配／錯誤 | `NoMatch`、`Error` | 畫布上保留區域與被拒絕的候選，**訊息不蓋在影像上** | 訊息列：無匹配＝卡片（做法清單＋動作按鈕）；錯誤＝橫幅（〔關閉〕〔再試一次〕〔改用 OpenCV〕）；錯誤時畫布暫停 |
| 7 | 編輯元件類型 | `EditingType` | 參考影像（放大）＋框群組（白色定義樣式）；選取的群組高亮＋把手；窗與父框之間畫點線 | 右側換成編輯器面板；工具列 V、B；狀態列「編輯 8371 · 41 個框」 |

```
 1 Empty                    3 DrawingRegion             5 Preview (scan)
┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────────┐
│                     │    │ ░░░░░░░░░░░░░░░░    │    │ ┌region─────────┐   │
│  ┌───────────────┐  │    │ ░ ▭ ▭ ▭ ▭ ▭ ▭ ░    │    │ │ ▭▭▭▭▭▭ ▭▭▭▭▭ │   │
│  │ No image open │  │    │ ░ 60×32 px     ░    │    │ │ ▓▓▓▓▓▓▓▓▓▓▓▓ │   │
│  │ [Open image…] │  │    │ Release to search   │    │ └───────────────┘   │
│  └───────────────┘  │    └─────────────────────┘    │ Enter accept · Esc  │
└─────────────────────┘                               └─────────────────────┘
 4 Searching                6 NoMatch                    7 EditingType
┌─────────────────────┐    ┌─────────────────────┐    ┌──────────────────────────┐
│ ┏━┓ ▭ ▭ ▭ ▭ ▭ ▭    │    │ ┏━┓ ▭ ▭ ▭ ▭ ▭ ▭    │    │      ▯▯▯▯▯▯▯▯▯▯  ← Pin   │
│ ▭ ▭ ▭ ▭ ▭ ▭ ▭ ▭    │    │ ▭ ▭ ▭ ▭ ▭ ▭ ▭ ▭    │    │  ┌────────────────────┐  │
├─────────────────────┤    ├─────────────────────┤    │  │       Body         │  │
│ ▓▓▓░░░░ 62% [Cancel]│    │ No similar part     │    │  └────────────────────┘  │
│ (message bar, >1 s) │    │ • teach new [Teach] │    │      ▯▯▯▯▯▯▯▯▯▯          │
└─────────────────────┘    └─────────────────────┘    └──────────────────────────┘
```

**訊息位置規則**（使用者回饋後新增）：所有需要閱讀的訊息——提示、進度與取消、無匹配、錯誤、吐司——都放在畫布**下方的訊息列**，**不蓋在影像上**。理由：檢測員要看著料件才能判斷，訊息蓋住的那一塊就是他要看的東西。訊息列的高度會隨內容變化，但**畫布大小不變**，所以板子不會被擠動。原型驗證：每個狀態的畫布大小都相同；所有無匹配原因、錯誤橫幅、進度條與吐司的方塊都不與畫布相交；無匹配時畫布四角與中心點上的元素都是畫布本身。唯一留在畫布中央的是「空白」卡片，因為那時畫布上沒有東西可擋。

**提醒卡片**（2026-10-08，使用者決定）：back end 可以用、但使用者應該知道的事（例如 HALCON 用的是 runtime 授權、SDK 的 dongle 沒插）也在訊息列，是一張不會自己消失的卡片，按〔關閉〕才收起。訊息列同一時間只顯示一種內容，優先順序：進度列 ＞ 搜尋的卡片（預覽、無匹配、錯誤）＞ 提醒卡片 ＞ 提示；搜尋的卡片收起之後，還沒關掉的提醒卡片再出現。狀態列最左邊一直標示目前的 back end 與提醒的短標示（「HALCON · runtime 授權」），滑鼠移上去看完整說明。

## 6. 互動狀態機（修訂 S00 `diagram.md` §4）

S04 的狀態機照這張表實作。原型使用的就是這張表（原型的「規則與快捷鍵」分頁會列出，每個轉移都經過測試）。

| 狀態 | 事件 → 下一狀態 |
|---|---|
| `Empty` | `ImageOpened` → `Idle`；`ImageFailed` → `Error` |
| `Idle` | `PressRegion` → `DrawingRegion`；`SearchRequested` → `Searching`；`EditType` → `EditingType`；`UndoToPreview` → `Preview`；`ImageOpened` → `Idle`；`ImageFailed` → `Error` |
| `DrawingRegion` | `Release` → `Searching`（區域掃描）；`ReleaseSample` → `Idle`（樣本建立好了，是文件堆疊上的 Command）；`ReleaseTooSmall`、`Esc` → 進入前的狀態（`Idle`／`Preview`／`NoMatch`） |
| `Searching` | `ResultFound` → `Preview`；`ResultEmpty` → `NoMatch`；`Failure` → `Error`；`Cancel` → `Idle`；`SearchRequested`、`ParamsChanged` → `Searching`（重新搜尋，最新的優先）；`ImageOpened` → `Idle` |
| `Preview` | `ThresholdChanged` → `Preview`；`SearchRequested`、`ParamsChanged` → `Searching`；`Accept`、`Discard` → `Idle`；`PressRegion` → `DrawingRegion`；`ImageOpened` → `Idle`；`ImageFailed` → `Error` |
| `NoMatch` | `ThresholdChanged` → `Preview`（有 ≥ 1 個找到）或 `NoMatch`；`SearchRequested`、`ParamsChanged` → `Searching`；`Dismiss` → `Idle`；`PressRegion` → `DrawingRegion`；`ImageOpened` → `Idle`；`ImageFailed` → `Error` |
| `Error` | `Dismiss` → `Idle`（影像開啟失敗且沒有舊影像時 → `Empty`）；`Retry` → `Searching`；`ImageOpened` → `Idle` |
| `EditingType` | `Save` → `Idle`；`Cancel` → `Idle` |

與 S00 §4 的差異（S04 要照新的做）：

1. **新增 `Empty`、`NoMatch`、`EditingType`**。`NoMatch` 是搜尋完成但沒有結果，是正常結果，不是故障，所以與 `Error` 分開。
2. **移除 `Committed`**：接受是一個動作（Command），不是停留的狀態。
3. **`DrawingSample` 改名 `DrawingRegion`**：流程 A（樣本）與流程 B（區域）共用，差別只是工具。
4. **`ThresholdChanged` 與 `ParamsChanged` 分開**：分數門檻只是對快取候選重新過濾（不搜尋，U3 ≤ 16 ms）；角度範圍等參數需要重新搜尋（防抖＋最新的優先，T5）。
5. **`DrawingRegion` 記得來源**：從 `Preview` 或 `NoMatch` 開始畫新的，按 Esc 回到原狀態，不丟掉提案。
6. **`UndoToPreview`**：接受或捨棄之後復原，回到 `Preview` 並還原提案（ADR-002）。
7. **`EditingType` 只能從 `Idle` 進入**：預覽中要編輯類型，先接受（或〔接受並存入樣本庫…〕，它依序觸發 `Accept` 與 `EditType`）。
8. **2026-10-14（ADR-025）新增 `ReleaseSample` 與 `SearchRequested`**：`S` 放開只建立樣本（`DrawingRegion` → `Idle`，一個 Command），搜尋另外按〔搜尋〕（`F5`：`Idle`、`Preview`、`NoMatch` → `Searching`，用板子上全部樣本）。`Release` 只剩區域掃描用。改大小、畫子框、框選都在 `Idle` 內完成，不經過狀態機。

## 7. 視覺狀態語言

完整數值在 [`tokens.json`](tokens.json)；對比驗算執行 `node tools/check-contrast.mjs`（淺色 18 組、深色 18 組、疊圖 24 組，全部通過；最緊的是疊圖 Found 對綠板 3.14:1）。

| 狀態 | 顏色（疊圖） | 線型 | 寬度 | 意義 |
|---|---|---|---|---|
| Sample 樣本 | `#58A6FF` 藍 | 實線 | 3 | 流程 A 使用者框的那個元件；存入庫後仍標示「這是教的那一個」 |
| Found 找到 | `#3FB950` 綠 | 實線 | 2 | 比對分數 ≥ 門檻；手動補的元件也是 Found（沒有分數，標示「手動」） |
| Inferred 推算 | `#D29922` 琥珀 | 虛線 8-4 | 2 | 晶格預測有元件、影像上沒有找到（可能缺件，要人看） |
| Rejected 拒絕 | `#FF7B72` 紅 | 點線（圓點，間距 4） | 2 | 候選分數 < 門檻 |
| 選取 | 同狀態色 | 同狀態線型 | +1 | 加粗；選取的元件另外畫一圈細外框（強調色） |

階層怎麼畫：

- **元件的所有框用元件自己的狀態樣式**。框的種類用寬度區分：Body ＝ 狀態寬度、Pin ＝ 狀態寬度 − 0.5、Window ＝ 1。
- **層級簡化（LOD）**：螢幕上邊長 < 3 px 的 Pin／Window 不畫（Body 永遠畫）；元件外框的螢幕寬度 ≥ 48 px 時顯示標籤「料號 分數」。5 000 個框時才不會一團。
- **區域**（流程 B 拖出的範圍）：白色細實線（1.5 px）＋ 10% 淡填色，不是狀態框；Searching／Preview／NoMatch 時保留，讓使用者知道掃了哪裡。
- **類型編輯器**中的框是「定義」，不是比對結果，所以用中性的白色：Body 2 px、Pin 1.5 px（淡填色）、Window 1 px；選取＝強調色；窗與父框之間畫點線。

其他規則：

- **不只靠顏色**：四種線型互不相同；物件樹與圖例用同樣的線型小圖示；`check-contrast` 會檢查線型不重複。原型的「灰階」開關可驗證。
- **外框（casing）**：每條線底下再畫一條寬 +2 的對比色線，讓框在任何影像上都看得到。
- **疊圖配色獨立於主題**（待使用者確認）：影像不會跟著主題變，多數板子是深色。淺色主題的描邊色是為淺色面板選的，疊在深綠板上只有 1.05–1.6:1（全靠白色外框）；深色主題的亮色在綠／藍／黑三種合成板色上都 ≥ 3.1:1。所以畫布上的框在兩個主題都用 `tokens.json` 的 `overlay` 組（亮色＋深色外框）；淺色主題的描邊色只用在 dock 的圖例與文字。原型的「疊圖配色」下拉可並排比較。
- 描邊是「cosmetic」（不隨縮放變粗）；高 DPI 下用裝置無關像素。
- 風險：綠色 Found 疊在綠色 PCB 上，靠外框區隔（3.14:1，剛好過線）。若板子多為綠色，要不要換 Found 的顏色？見第 14 節。

## 8. 互動規則（每條都有「為什麼」）

| 操作 | 行為 | 為什麼 |
|---|---|---|
| 滾輪 | 以游標為中心縮放（每格 ×1.1，5%–3200%） | 游標下的元件不會移位，不會跟丟。原型已驗證：游標下的世界座標縮放前後不變 |
| Space＋拖曳、中鍵拖曳 | 平移 | 左鍵留給畫框與移動，不需要另一個「平移模式」 |
| `F` | 適配視窗（邊界 24 px）；在類型編輯器是適配該類型 | 放大後一個鍵回到已知視圖 |
| `W` | 適配寬度：影像寬度撐滿畫布（左右邊界 24 px），上下超出的部分靠平移看；保留畫布中央那一列，但上緣、下緣不離開邊界；影像比畫布扁時垂直置中。視窗大小改變時照樣適配寬度，縮放或平移後就不再跟著 | 正方形或直式影像在橫長的畫布上，適配視窗會在左右留下大片空白；載入影像仍是適配視窗 |
| `S` | 建立樣本：框**一個**元件（或一塊區域）；放開只建立樣本、不搜尋，建完回到選取工具（2026-10-14，ADR-025） | 流程 A；「樣本建立是建立、搜尋是搜尋」（使用者）；框到多個元件時 back end 會提示改用 `R`，區域樣本是刻意的用法時可以忽略 |
| 〔搜尋〕、`F5` | 用板子上**全部樣本**各搜一次整張板子，結果依位置合併（同一個位置留分數最高的、過門檻的）→ Preview；沒有樣本時吐司說明 | 一次搜尋得到全部結果、同一個位置不會有兩筆（ADR-025） |
| `B` | 畫子框：先選取一個元件，再在它上面框一個框；存成類型的 Pin 群組（1×1），同類型全部元件一起多出這個框 | 「區域上有 N 個 CAD 框，找到就整組貼上」（ADR-025 §6） |
| `M` | 比對區（2026-10-15，ADR-026）：先選取一個元件，再在它上面拖一個框＝建範本只用這一塊；點一下＝自動擷取（Otsu 取離點擊最近的亮塊、外擴 4 px 成遮罩）。存在類型上（同類型一起），CAD 框不變；選取資訊表單可以改特徵（灰階／邊緣）、調外擴、清除；`Ctrl+Z` 復原 | 「整框比對的是焊墊，端子只佔 2%」：只比要的特徵（ADR-026） |
| `R` | 區域掃描：框任意區域 | 流程 B；用庫辨識，一次得到全部元件與它們的全部框 |
| `Enter`，或畫布下方的〔接受 (Enter)〕 | 接受提案。預覽時訊息列另有〔接受並存入樣本庫…〕（只有教樣本的提案才有）和〔捨棄 (Esc)〕 | 流程的目的，路徑最短；**不能只靠鍵盤**，只用滑鼠也要走得完（使用者實測時找不到存入樣本庫的按鈕後補上） |
| `Esc` | 退回一層：取消畫框／取消搜尋 → 清除選取 → 捨棄提案；在類型編輯器：清除選取 → 回到選取工具 → 取消（有未儲存的修改時**不**取消） | 一個鍵永遠是「往回一步」；捨棄也能復原，所以不必跳確認；但丟掉未存的類型草稿不能復原，所以要明確按〔取消〕 |
| 點擊元件 | 選取**整個元件**（所有框）；Shift＋點擊加入或移除 | 板上的編輯單位是元件，框是它的類型的一部分 |
| 雙擊元件，或 `E` | 開啟它的類型編輯器；雙擊時**預選點到的那個框的群組** | 從「我看到這個 Pin 不對」直達「改這個 Pin」 |
| 拖曳元件 | 移動元件（位姿），整組框一起走；按在已選取的元件上拖＝整個選取一起走 | — |
| 在空白處拖曳（`V`） | 框選：外框和框相交的元件；Shift＋框選＝加入；沒拖動就是點空白處（清除選取） | 「整組一起拖」要先能一次選很多個（ADR-025 §5） |
| `Ctrl+A`、`Ctrl+Shift+A` | 全選；選取同類型（所選元件的類型） | 同上 |
| 拖把手（單一選取的元件） | 改**類型的主體框**大小：同類型全部元件一起變；拖單邊時被拖的那個元件的中心跟著移，其他不動。一次拖曳一個 Command；只在板子（`Idle`） | ADR-006 §3「此類型全部一起」；ADR-025 §4 |
| 選取資訊的寬、高（2026-10-14） | 用數字改**類型的主體框**：中心不動、同類型全部一起；按 Enter 或離開欄位才生效，一次一個 Command；< 8 px 不收（吐司）；〔鎖定寬高比〕只是表單的行為；只在 `Idle`（預覽中欄位停用並說明） | 「編輯只能拉框，要能明確看到並用數字改長寬」（使用者 2026-10-14）；拖把手拖不準 |
| 選取資訊的中心 X、Y（2026-10-14） | 移動**這一個**元件到新的中心（一個 MoveComponents）；`Idle` 作用在板子、預覽作用在提案 | 同上；和方向鍵微調一樣的路徑 |
| 〔套用大小到所有樣本〕（2026-10-14） | 把所選元件目前的寬 × 高套到板子上每一個樣本的類型；一個 Command（`ResizeTypes`）、可復原；已經一樣的略過，全部一樣時吐司說明、不進堆疊 | 「讓多筆樣本的尺寸可以調成一致」（使用者 2026-10-14） |
| 樣本庫 dock 的一筆（2026-10-14） | 點一筆＝在畫布上選取那個樣本（和點畫布相同） | 「建立樣本時要有地方看到樣本的圖」（使用者 2026-10-14）；清單就是板子上的樣本 |
| `Del`、`Backspace` | 板上：刪除所選元件；類型編輯器：刪除所選群組（連同依附它的窗）；在矩陣中選單一格再刪＝只略過那一格 | 先選、再刪，按鍵順序與思考順序一致；主體框不能刪 |
| `A`、`＋` | 補一個元件（用目前選取元件的類型，沒有就用最近一次的類型）；來源標示「手動」 | 缺件時直接補，不必重新搜尋 |
| `B`（編輯器） | 畫框：第一個是 Body，之後是 Pin | 先主體、後腳位，和 TRI 的順序一致 |
| 列數、欄數（Pin） | 改數字就一次建立或刪除整個矩陣；增加時間距預設為「框寬／高 ＋ 4」 | 「同時建立多個框」 |
| 套用範圍（編輯器表單） | 〔只改這個矩陣〕或〔此類型中所有相同種類的框〕（Pin 對 Pin；Solder 窗對 Solder 窗） | 「同時修改相同類型的框」；對應 TRI 的「套用範圍」對話框 |
| 〔＋檢測窗〕 | 在所選的 Body／Pin 上加一組窗，預設 80% × 60%，可改比例與演算法標籤 | 窗連結到父框，父框改窗跟著變 |
| 〔儲存〕按鈕（板子的複本） | 〔只儲存在這塊板子〕：只更新板上的複本；〔儲存並存入樣本庫〕：同時寫進樣本庫（要料號；同料號會覆寫）。從〔接受並存入樣本庫…〕進來時，後者是主要按鈕 | 庫與板子是複本關係，存入庫是明確的動作 |
| 〔儲存到樣本庫〕（庫條目） | 從樣本庫〔編輯〕進來時只有這一個按鈕；面板提醒「板子上已有的複本不會自動更新（目前 N 個複本）」 | 改庫不該偷偷改到舊程式 |
| 〔從樣本庫更新〕、〔存入樣本庫〕（選取資訊） | 只在複本與庫不同時出現；各是文件堆疊上的一個 Command，可復原 | 同步是明確的、可反悔的 |
| 〔一次產生標準檢測框〕 | Body 上一組 Void 窗、每個 Pin 群組一組 Solder 窗（已有的不重複建立） | 對應 TRI 的「所有檢測框 (Add All)」 |
| `Ctrl+Z`、`Ctrl+Y`（`Ctrl+Shift+Z`） | 復原、重做；對象依狀態：類型編輯器→草稿；預覽→提案；已載入→文件 | 自動結果一定有錯，修正要便宜（ADR-002） |
| `V`、`S`、`R`、`A` | 選取／移動、教樣本、區域掃描、補一個 | — |
| 把手 | 看起來 8 px，**點擊區 24 px**（WCAG 2.2 目標尺寸 24×24）；板子上恰好一個元件被選取時出現在它的外框上（2026-10-14 起），類型編輯器也用同一套 | 好點中又不遮住框 |
| 小框 | 框的螢幕邊長 < 72 px：只放 4 個角的把手，並往外推 12 px；≥ 72 px：8 個把手在邊上 | 小框的把手點擊區會互相重疊、蓋住框內部，內部要留給拖曳移動 |
| 最小樣本 | 邊長 < 8 影像像素：不搜尋，提示太小 | 太小的樣本不可能比對 |
| 比對範圍（參數 dock） | 〔先用板子已有的類型，找不到再擴大（建議）〕、〔只用板子已有的類型〕、〔整個樣本庫〕；改了會重新搜尋 | 樣本庫變大時，不要每次都拿整個庫去比（第 9 節） |
| 辨識候選 | 預覽中選一個元件，列出每個樣本庫類型的比對分數，點另一個＝改選（一個 Command） | 多類型比對一定會有誤判，修正要便宜 |

原型驗證過：把手點擊區的正反兩面（距中心 10 px 命中、13 px 沒命中）。

已知缺口：畫框與移動目前只能用指標裝置完成（沒有鍵盤替代）。S02 前要決定是否補方向鍵微調；本步驟不假設答案。

## 9. 回饋時間規則

| 時間 | 行為 | 預算 |
|---|---|---|
| ≤ 50 ms | 點擊、拖曳都有視覺回饋 | I2 |
| > 200 ms | 狀態列顯示忙碌（轉圈＋「搜尋中…」），游標變 `progress`；一旦顯示至少停留 400 ms，避免閃爍 | — |
| > 1 s | 訊息列顯示進度條＋〔取消 (Esc)〕 | — |
| 取消 | **UI 在 100 ms 內回到 Idle**；worker 不必立刻停止（單次比對呼叫無法中途中斷），它的結果在回來時丟棄 | I4 |
| 永遠 | 狀態列顯示「back end · 耗時 ms · 數量」，數量含元件與框的總數 | — |

T5（端到端 300 ms）代表多數搜尋會跨過 200 ms 門檻，所以忙碌標示必須低調（狀態列，不擋畫布）。

**樣本庫很大時怎麼辦**（使用者提問）：區域掃描的成本隨「實際比對的樣本數」成長，所以設計是**少比**，而不是只靠「讓它跑快」：

1. 預設只比**板子已有的類型**（板子保存自己用到的類型複本，通常只是庫的一小部分），區域裡還有東西沒比到才擴大到整個庫；可改成只比板子類型或整個庫。
2. 狀態列永遠顯示「比對 N 個樣本」，使用者看得到這次比了多少。
3. 進度與取消照第 9 節的規則；取消的粒度是「每個樣本之間」。
4. 預篩選（先量零件外形，只比尺寸相近的樣本）、模型快取、由粗到細、平行比對，是 S07／S08 要實作並量測的選項（ADR-006 §7）。

**這些都還沒有量測。** 原型的耗時是公式模擬（基準 ×（0.6 ＋ 0.2 × 比對的樣本數）），只有「比對了幾個樣本」是真的邏輯。

## 10. 文案與 i18n

全部在 [`copy.json`](copy.json)，key 形如 `ui.noMatch.advice.lowContrast`，每個 key 有 `zh_TW` 與 `en`，佔位字用 `{name}`（S02 轉成 Qt 的 `%1`／`%n`）。

無匹配時一定告訴使用者下一步。原因與對應文案：

| 原因（use case 回傳） | 文案 key | 動作 |
|---|---|---|
| 樣本太小 | `sampleTooSmall` | 〔重畫樣本〕 |
| 樣本比元件大很多（只含一個元件） | `sampleTooLarge` | 〔重畫樣本〕 |
| 樣本框到不只一個元件 | `sampleCoversMany`（建議改用區域掃描） | 〔重畫樣本〕 |
| 樣本沒框到元件 | `sampleOffPart` | 〔重畫樣本〕 |
| 板上只有這一個相似元件 | `onlyOne` | 〔重畫樣本〕 |
| 對比太低 | `lowContrast` | — |
| 分數門檻太高（有候選但都 < 門檻） | `lowerThreshold`（附最高分與建議值） | 〔降低門檻到 {suggested}〕：不重新搜尋，直接過濾快取 |
| 樣本庫是空的 | `libraryEmpty` | 〔教一個新樣本〕 |
| 區域內沒有元件 | `regionEmpty` | 〔重畫樣本〕 |
| 區域內有元件但庫裡沒有符合的 | `noLibraryMatch`（附元件數） | 〔教一個新樣本〕 |
| 區域內的元件都已在板上 | `alreadyOnBoard` | — |
| 比對範圍只有板子已有的類型，但區域內的元件可能符合庫裡其他樣本 | `scopeBoard`（附元件數） | 〔改用整個樣本庫〕：重新搜尋 |

錯誤訊息：影像無法解碼（`ui.error.imageOpen.*`）、back end 無法使用（`ui.error.backend.*`，提供〔改用 OpenCV〕）、搜尋失敗（`ui.error.searchFailed.*`，保留區域、可再試）。類型編輯器的驗證訊息：缺料號、料號重複、沒有主體框、主體框不能刪除。

## 11. 主題與縮放

- 淺色、深色、跟隨系統。文字對比 ≥ 4.5:1、圖形對比 ≥ 3:1（WCAG 2.1 AA），已驗算。
- 字級縮放 100%、125%、150%（只放大文字；把手與點擊區不縮放）。
- 高 DPI：Qt 6 預設的縮放；描邊用 cosmetic pen。
- 最小視窗 1024×640。窄畫面（原型在 ≤ 900 px 與 ≤ 560 px 有堆疊版面）只是原型的便利，桌面程式不要求。
- 減少動態：忙碌轉圈在 `prefers-reduced-motion` 下停止動畫（原型已做；Qt 端 S02 處理）。

## 12. 給 S03／S04 的需求與 Presenter 介面名單

### 12.1 對核心的需求（S03～S12 要照做；對各步驟的影響見 ADR-006「後果」）

1. **domain 型別**：`Board`、`ComponentInstance`（名稱、位置、角度、正反面、類型參照、狀態、分數、`ObjectOrigin{Auto, Manual}`）、`ComponentType`（料號、別名、名稱、類別、參考影像、框群組）、`BoxGroup`（`Body`／`Pin`／`Window`；Window 的父群組參照與比例）。框是**推導**的（`expand(type, pose)`），不單獨儲存。
2. **新 port**：`ILibraryStore`（載入、儲存、查找、別名索引；使用者層，之後可加專案層與系統層）、`ILibraryMatcher`（區域＋樣本庫 → 元件候選：類型、位姿、分數、候選類型的分數）。
3. use case 回傳**所有驗證過的候選與分數**（分數 ≥ 搜尋下限 0.30），由 Presenter 套門檻。否則分數門檻變動就得重新搜尋，U3（16 ms）做不到。
4. 空結果要帶原因 `NoMatchReason`（第 10 節那張表的前 11 項）加上最高分，才能算出「降低到 {suggested}」。判斷「框到幾個元件」的依據：元件至少一半面積在樣本框內才算（原型的假比對器這樣做；真正怎麼判斷留給 S07）。
5. 手動補的元件沒有分數，不能偽造；`ObjectOrigin` 標示來源。
6. 取消的語意（I4）：`IExecutor` 的工作可以取消，但 UI 不等工作停止；過期結果以工作編號丟棄。
7. 「推算」元件只在找到 ≥ 2 個之後才有意義（晶格需要至少兩個匹配）；找到太少時 Presenter 不顯示它們。晶格推算屬於**元件層級**（S05）。
8. 元件**命名**：開頭字元（依類別：R、C、U…）＋流水號，接受時指派（TRI Non CAD 的做法）。
9. **CAD 清單**：元件名稱、X、Y、角度、元件類型（料號），再加正反面——和 TRI 的 `*.aoi` 欄位一致。樣本庫的座標單位是 **µm ＋ 影像校正值**（使用者決定，ADR-006 §6）；CAD 清單的原點與單位在 S12 決定。原型用影像像素、原點在左上角，是簡化。
10. 對效能計畫：區域掃描的成本隨樣本庫大小 N 成長，S10 的矩陣要加 N；**新的預算（暫稱 T7）等 S07 實測後再提**，本步驟不訂數字。
11. `ILibraryMatcher::match(image, region, candidates)` 吃的是**候選清單**：範圍（板子類型 → 整個庫）與預篩選由呼叫端（use case）決定，matcher 不自己掃整個庫；結果附「實際比對了幾個樣本」；取消在樣本之間生效；進度可回報「第幾個樣本」。

### 12.2 View 端介面（由 Qt 元件實作；每個 ≤ 5 個方法）

| 介面 | 責任 | 方法 |
|---|---|---|
| `ICanvasView` | 畫布顯示 | `showImage(image)`、`showComponents(snapshot)`、`showRegionDraft(rect, mode)`、`setSelection(ids)`、`setInteraction(mode)` |
| `IComponentTreeView` | 物件樹（Board › Component › 框群組） | `setTree(snapshot)`、`setSelection(ids)`、`setFilter(filter)` |
| `ILibraryView` | 樣本庫清單 | `setEntries(entries)`、`setFilterText(text)`、`setEnabled(on)` |
| `ITypeEditorView` | 類型編輯器面板與畫布場景 | `showType(typeSnapshot)`、`showSelection(groupId)`、`showValidation(errors)`、`setDirty(on)`、`close()` |
| `IParamsView` | 參數 dock | `setParams(params)`、`setScoreRange(floor, max)`、`setEnabled(on)` |
| `IStatusView` | 狀態列與吐司 | `showSummary(summary)`、`showBusy(level)`、`showProgress(fraction)`、`showToast(key, args)` |
| `IMessageView` | 空白、無匹配、錯誤卡片 | `showEmpty()`、`showNoMatch(advice)`、`showError(error)`、`hideMessage()` |
| `IHistoryView` | 復原／重做按鈕 | `setUndo(enabled, label)`、`setRedo(enabled, label)` |

### 12.3 Controller 端介面（View 呼叫；由 Presenter 實作）

| 介面 | 責任 | 方法 |
|---|---|---|
| `ICanvasController`（S02 已預告此名） | 畫布上的指標操作 | `onRegionDrawn(rect, mode)`、`onRegionAborted()`、`onAddRequested(point)`、`onComponentsMoved(ids, delta)`、`onOpenTypeEditor(componentId, boxHint)` |
| `ISelectionController` | 選取、刪除、篩選 | `select(ids)`、`deleteSelected()`、`setFilter(filter)` |
| `IProposalController` | 提案生命週期 | `accept()`、`acceptAndSave()`、`discard()`、`cancelSearch()`、`chooseAlternative(componentId, typeId)` |
| `IMessageController` | 訊息卡上的動作 | `retry()`、`dismiss()`、`applyAdvice(actionId)` |
| `IParamsController` | 比對參數 | `setScoreThreshold(value)`、`setAngleRange(range)`、`setScanScope(scope)` |
| `IBackendController` | back end 切換 | `availableBackends()`、`selectBackend(id)` |
| `ILibraryController` | 樣本庫與同步 | `openEditor(typeId)`、`deleteType(typeId)`、`setFilterText(text)`、`pullFromLibrary(typeId)`、`pushToLibrary(typeId)` |
| `ITypeEditorController` | 類型編輯器的編輯 | `addGroup(kind, rect)`、`updateGroup(groupId, change, scope)`、`addWindow(parentId)`、`addStandardWindows()`、`deleteSelection()` |
| `ITypeEditorSession` | 類型編輯器的結束 | `save(identity, alsoAddToLibrary)`、`cancel()` |
| `IHistoryController` | 復原／重做 | `undo()`、`redo()` |

### 12.4 Presenter 與其他 S04 類別（只列名稱與責任）

| 類別 | 層 | 責任 |
|---|---|---|
| `AutoRoiPresenter` | presentation | 實作 `ICanvasController`、`IProposalController`、`IMessageController`；驅動狀態機與 use case（流程 A、B） |
| `ComponentTreePresenter` | presentation | 實作 `ISelectionController`；與畫布雙向選取同步 |
| `ParamsPresenter` | presentation | 實作 `IParamsController`、`IBackendController`；門檻即時過濾、其他參數防抖＋最新的優先 |
| `LibraryPresenter` | presentation | 實作 `ILibraryController` |
| `TypeEditorPresenter` | presentation | 實作 `ITypeEditorController`、`ITypeEditorSession`；套用範圍、父子連結的比例換算 |
| `HistoryPresenter` | presentation | 實作 `IHistoryController`；依狀態決定 `Ctrl+Z` 作用在哪個堆疊 |
| `InteractionStateMachine` | application（無 Qt） | 第 6 節的轉移表；每個轉移一個單元測試 |
| `InspectionDocument`、`ProposalSession`、`TypeEditSession`、`UndoStack` | application（無 Qt） | ADR-002 |
| `BoxExpander` | domain | `BoxGroup`＋位姿 → 具體的框；純幾何，可用精確數字測試 |

縮放、平移、適配視窗**不經過 Presenter**：它們是畫布自己的檢視狀態（不進復原、不需要 use case），狀態列的縮放百分比由 MainWindow 把畫布的 `zoomChanged` 接到 `IStatusView`。理由：I1 的幀時間不能多一層間接。

### 12.5 與 SOLID 的對照

`I`：View 與 Controller 介面各 ≤ 5 個方法，每個 View 只看到自己需要的介面（不是整個 MainWindow）。`D`：Presenter 只依賴這些介面與 use case，不認識 `QWidget`。`S`：每個 Presenter 一個責任。`O`：新的框種類（例如之後的 Pad、Align）＝新增 `BoxGroup` 種類與表單，不改 Presenter 的流程。

## 13. UI 效能預算（已與使用者確認）

使用者於 2026-10-05 確認 `perf/budgets.json` 的 U1–U4、I1–I6 數字，並接受下列三點定義。數字仍是「提議值」，S02 實測校準。

1. **U3** 只適用「分數門檻」這類用快取候選重新過濾的參數。需要重新搜尋的參數走防抖（120 ms）＋T5。
2. **I4** 量「UI 回到 Idle」，不等 worker 真的停止；舊結果丟棄。
3. **U4** 拆成「解碼」與「首次繪製」兩段記錄；S02 依 BMP／PNG／JPEG 實測後再校準整體 200 ms。

## 14. 待使用者回饋的設計選擇

| # | 問題 | 我的建議 | 影響 |
|---|---|---|---|
| 1 | 疊圖配色：固定亮色，還是跟隨主題 | **已確認：固定亮色**（2026-10-05） | `tokens.json` 的 `overlay` 組；S02 的框繪製 |
| 2 | Found 的綠色疊在綠色板子上（3.14:1）要不要換色 | 先維持計畫的綠色；若實拍圖多為綠板，S07 前再議 | Found 的 token |
| 3 | 接受或捨棄後 `Ctrl+Z` 回到 Preview（提案還在） | **已確認：採用**（ADR-002 轉為「採用」） | ADR-002、狀態機的 `UndoToPreview` |
| 4 | `Esc` 捨棄提案不跳確認（因為可復原） | **已確認：採用** | 流程 |
| 5 | 手動「補一個」的元件沒有分數，標示「手動」 | **已確認：採用** `ObjectOrigin` | S03 的型別 |
| 6 | 樣本框要不要貼齊零件外形（原型為了簡化是貼齊的；真實做法是樣本框就是模板） | 這是 S05／S07 的問題；原型的行為不代表決定 | 樣本主體框的大小 |
| 7 | **樣本庫的單位**：存物理單位（µm）還是像素 | **已確認：µm ＋ 影像校正值**（ADR-006 §6） | S03 座標慣例、S06 |
| 8 | **庫與程式的關係**：複製進板子（TRI 的 Merge）還是即時連結 | **已確認：複製進板子**；原型已改成這個語意（ADR-006 §5） | S06、類型編輯器的語意 |
| 9 | 編輯類型時**預設影響此類型的所有元件**；是否需要「只改這一個元件」 | 先不做（ADR-006 未決 #3） | 編輯器的套用範圍 |
| 10 | 之後要不要做 Panel（多連板 M×N） | v0.1 單板（ADR-006 未決 #4） | Board 上層 |
| 11 | 檢測窗的演算法與參數（TRI：Missing、Void、Color、Lead、Solder 與比對條件、光源、權重）何時進來 | 超出 S01；先只有標籤（ADR-006 未決 #6） | 窗的屬性 |
| 13 | 掃描的預篩選要不要做（先量零件外形，只比尺寸相近的樣本）、預設範圍是不是「先板子、後整庫」 | 預設範圍採用原型的做法；預篩選留給 S07 量了再決定（ADR-006 §7） | S03 介面、S07 |
| 12 | 樣本庫要不要有 System（原廠）與 Project（專案）層（TRI 有三層） | v0.1 先做使用者層一層；板子的複本相當於專案層（ADR-006 §4） | S06 |
