# R09.14 概念與 UX 審查

- 日期：2026-10-30；Codex (gpt-6.1-sol)。
- 狀態：PARTIALLY VERIFIED。概念設計與文件檢查，不是執行介面的功能驗收。

## 分開檢視的結果

| 面向 | 決策與證據 | 驗證及限制 |
|---|---|---|
| 任務配置 | 使用者參考三欄工作台；既有 guided_ui.py 已有五步與類別清單 | 概念保留五步，第二步高亮、中央影像最大；未在 PySide6 實作 |
| 選取一致性 | 概念統一 #02／電阻／R22／101 電阻裁切；原參考多個選取編號與顏色易混淆 | 人工檢視深色結果的清單、標籤、右側與預覽一致；示意值非實際使用者標註 |
| 雙主題 | 使用者指定預設深色；現有 ui_theme.py 僅依 palette 決定 | 本次交付雙主題視覺；明確切換與偏好儲存仍未實作 |
| 資料與歷史 | 名稱、幾何、矩形／多邊形沿用 ADR-039 | 不改 model／workflow／canvas，不重開或操作現有視窗；不驗證本次圖內的虛構數值與照片比例 |
| 存檔與審核 | 原程式明確存檔，不得聲稱自動儲存；幾何檢查無法判斷漏標 | 概念顯示已儲存、人工審核未勾；規格要求錯誤／取消後保持原內容，互動待測 |
| 色彩與可讀性 | 同類同色、選取另有把手；對比公式對 theme-tokens.json 計算 | 19 組指定對比全部通過；正文最低 5.17:1、邊界最低 3.25:1。不是生成圖逐像素檢查 |
| 相容與效能 | 僅新增 PNG／Markdown／JSON，不改程式與依賴 | 未跑 Python／C++／Rust 測試、訓練或效能測試，不能引用前次 32 項為本次驗證 |

## 實際執行

- 內建 imagegen：生成深色概念，並以深色結果製作淺色。
- 兩張生成 PNG 已逐張檢視：相同三欄、第二步、#02／R22、類別數量與幾何值；深淺主題選擇狀態正確。兩張實際為 1536×1024；深色提示中的 1920×1280 未由工具採用，未上採樣冒充原生解析度。
- 已知生成差異：深色 #04 IC 的角點仍有白色方塊，容易被看作另一組選取把手；淺色轉換已移除。兩圖並非逐像素相同，不能直接擷取作為產品 UI；實作必須遵守只有 #02 顯示編輯把手的規格。
- Node.js 直接讀取 theme-tokens.json，依 sRGB 線性化及相對亮度公式計算指定文字／背景、焦點／面板與邊界／欄位的對比；19 組 PASS，exit code 0。
- `node tools/ste-lint.mjs`：1049 sentences、0 errors、0 warnings，exit code 0。
- `git -c safe.directory=D:/AOI_Project/Qt/AutoCadCreated diff --check`：exit code 0。既有其他修改保留。
- `scripts/ai/check-ai-structure.ps1`：第一次因 sandbox 身分與 repo 擁有者不同，內部 Git HEAD 查詢被 dubious ownership 阻擋，exit code 1；只對當次程序設定 GIT_CONFIG_COUNT／safe.directory 後重跑，exit code 0、model-routing 68 passed，9 個既有 architecture.md 路徑警告。沒有改全域 Git 設定。

## 後續實作才可驗收

主題切換立即生效與持久化、焦點與 Delete 保護、草稿保存、失敗存檔、矩形／多邊形與 Undo 交易、無選取的屬性清空、折疊與搜尋、縮放／minimap 同步、人工審核、1366×768 與高 DPI。生成圖可能有細微字形／把手差異；操作規則以 DESIGN.md 為準，圖像不是可執行規格。
