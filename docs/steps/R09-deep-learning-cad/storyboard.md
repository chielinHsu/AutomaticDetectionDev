# 操作分鏡

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-30 | Codex (gpt-6.1-sol) | 新增通用元件工作台與 Qt Creator 建置分鏡。 |

## Vision CAD Studio／Qt Creator

| 時間 | 畫面 | STE 旁白 | 繁中字幕 |
|---|---|---|---|
| 0–5 s | Qt Creator 開 AiInspTool.pro | Build both tools from this project. | 同一專案建置原工具與元件深度學習工作台。 |
| 5–10 s | Run 選 VisionCadWorkbench | Select the workbench target. | 選工作台執行目標；參數可切換 OBB／實例分割。 |
| 10–16 s | 三欄影像／標註／選取屬性 | Select an object to inspect its properties. | 圖片與清單在左，畫布中央，選取資訊在右。 |
| 16–22 s | 深淺主題切換，草稿與選取保留 | Change the theme. Keep the current view. | 首次深色，偏好會記住，不改動原圖或標註。 |
| 22–30 s | 輸入中心／長寬，套用後 Undo | Apply the rectangle size. Undo the change. | 數值修改需通過範圍檢查，可復原。 |

本次只更新分鏡，沒有生成影片／語音。

| 畫面 | 使用者操作 | 可觀察的結果 |
|---|---|---|
| 1 人工標註 | 載圖、選類別、逐點畫輪廓並 Enter | 輪廓、類別、可拖動黃色頂點；尚未審核 |
| 2 整理資料 | 設來源 group、split、CAD 尺寸，勾審核後匯出 | 兩份資料、source-project.json、split-audit.json |
| 3 學習 | 分別選 obb／segment 訓練 | 日誌、test 指標與 deployment/model.json |
| 4 生成 | 匯入相同 task 模型，選 ROI 或全圖 | 綠色固定 CAD、青色子窗、粉色模型輪廓 |
| 5 修正 | 拖移、填 x/y/w/h/angle、復原重做 | 單物件人工覆寫，重新推論保留 |
| 6 配方 | 儲存／開啟 PCB 深度學習 CAD JSON | 類型規格、旋轉實例、影像路徑及模型來源還原 |

程式實際驗證截圖在 build/r09-dl-ui.png；模型品質不能由示範畫面推定。

## 新手 UX 分鏡（R09.9–R09.10）

| 時間 | 畫面 | STE 旁白 | 繁中字幕 |
|---|---|---|---|
| 0–5 s | 第 1 步空狀態與載圖主按鈕 | Load a PCB image. | 先載入 PCB 圖片，或接續標註專案。 |
| 5–12 s | 第 2 步中文類別與主按鈕 | Select a class. Drag a box around the component. | 選類別，再按住左鍵框住元件本體。 |
| 12–18 s | 固定像素橘黃框、編號、白色選取框 | Check the new annotation. | 畫完即顯示編號；白框表示目前選取。 |
| 18–25 s | 改錯誤類別、帶入尺寸 | Correct the class. Set the CAD size. | 類別選錯可以改；選取標註可帶入檢測框尺寸。 |
| 25–32 s | 第 4 步未完成條件與收合設定 | Review the data before training. | 缺少審核或獨立來源時，先處理提示。 |
| 32–40 s | 第 5 步模型、CAD 與儲存回饋 | Review the CAD objects. Save the recipe. | 載入模型後產生 CAD，檢查再儲存。 |

本次沒有製作實際影片；新版畫面及驗證在 build/r09-ux-backup/。

## 大量元件查詢與形狀編輯

| 時間 | 畫面 | STE 旁白 | 繁中字幕 |
|---|---|---|---|
| 0–5 s | 左側類別與數量，所有群組收合 | Expand a class to view its objects. | 只展開要看的 IC／電容／電阻。 |
| 5–12 s | 搜尋 U1 或 #12，畫面同步篩選 | Search by class, name, or number. | 查詢會同步篩選清單與框線。 |
| 12–18 s | 雙擊子項，放大定位並顯示完整標籤 | Open an object to inspect its position. | 雙擊放大定位，只顯示選取項的完整標籤。 |
| 18–25 s | 矩形角點移動，同步調整鄰角 | Resize the rectangle. Keep the right angles. | 拉角點仍保持矩形，長寬可各自調整。 |
| 25–32 s | 明確切換多邊形，拖任一頂點 | Select polygon mode for free vertex edits. | 需要自由輪廓才切換多邊形，可復原。 |

只有分鏡與實際 Qt 畫面，未製作影片；證據 build/r09-browser-preview.png、r09-browser-selected.png。

## 本體與導腳連動（R09.21–R09.23）

| 時間 | 畫面 | STE 旁白 | 繁中字幕 |
|---|---|---|---|
| 0–5 s | 第 3 步選 IC 參考元件 | Select the reference component. | 外框供辨識定位；紅框才是檢測區。 |
| 5–12 s | 建立雙排八腳，中央與八腳均紅框 | Create the body and eight pin frames. | 版型先建立本體及八個導腳，需依影像調整。 |
| 12–20 s | 樹清單選 body，修改 X 與角度 | Move the body. Its children follow. | 本體移動或旋轉時，所有導腳依相對座標跟隨。 |
| 20–28 s | 選某腳，增加 window，再 Undo | Add a window to one pin. | 檢測窗可依附該腳，修改與刪除均可復原。 |
| 28–36 s | 模型生成多個元件，各有整組框，存讀 | Generate complete frame groups. Save the recipe. | OBB／分割都套同封裝規格，儲存後保留父子關係。 |

只有分鏡與 Qt 實圖示範，沒有合成影片或宣稱實際模型品質。
