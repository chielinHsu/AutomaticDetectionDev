# S02 — Qt UI 骨架與影像畫布

| 項目 | 內容 |
|---|---|
| 層級 | 介面架構（實作） |
| 分支 | `step/02-ui-shell` |
| 依賴 | S01 |
| 產出位置 | `CMakeLists.txt`、`src/presentation/`、`tests/ui/`、`docs/steps/S02-ui-shell/` |

<!--STE-->
## Purpose
This step builds the main window and the image canvas. The canvas zooms, pans and draws boxes.

## Reason
The user works on the canvas all day. The canvas must feel fast before we add any vision code.

## Effect
We get a measured baseline for screen speed. A later step cannot make the screen slower without a failed test.
<!--/STE-->

**繁中**：先把「畫布」做到順，再加視覺程式。取得畫面速度的實測基準線，之後任何變慢都會被測試擋下。

## 任務

1. **驗證工具鏈（第一件事）**：用 `vswhere` 找 VS 2022、確認 `vcvars64.bat` 可用；以 Qt `D:\Qt\6.8.0\msvc2022_64`＋Ninja 設定 CMake 能 configure/build。**把結果更新到 `CLAUDE.md` 環境表。**
2. **專案骨架**：根 `CMakeLists.txt`、`CMakePresets.json`（debug / release / relwithdebinfo）、目標切分依 `CLAUDE.md` 分層；自家程式碼警告當錯誤（`/W4 /WX`）；選項 `AOI_WITH_OPENCV`、`AOI_WITH_HALCON`（此步預設 OFF）。
3. **MainWindow**：左工具列、右側 Objects／Params dock（先放空殼）、狀態列；S01 的 token 轉成 `QPalette`/QSS；i18n 骨架（zh_TW／en）；`QAction` 快捷鍵。
4. **`ImageCanvas : QGraphicsView`**：游標中心縮放、平移、適配視窗、高 DPI。**`ImageItem`** 以零複製包裝共享影像緩衝為 `QImage`。
5. **`RoiGraphicsItem`**（依狀態套用 S01 樣式、選取把手 ≥ 24 px）與 **`RoiLayerItem`**（大量框的批次繪製、只畫可見範圍、依縮放層級簡化）。
6. **工具**：`ITool`（≤ 5 個方法）＋ `ToolRegistry`；`SampleRoiTool`（橡皮筋框選）。
7. **`FakeScene`**：產生合成板子影像與 100／1k／5k／20k 個框，供測試與效能量測。
8. **Qt Test**（`QT_QPA_PLATFORM=offscreen`）：啟動冒煙測試、縮放/平移行為測試、基準線量測 **U1、U4、I1、I2、I5**。

## SOLID 重點

- **S**：每個工具一個類別；Item 只管繪製與把手。
- **O**：新增工具＝新增類別＋註冊，不改畫布。
- **I**：`ITool` ≤ 5 個方法。
- **D**：畫布透過 `ICanvasController`（定義在 presentation）與外界溝通，S04 才接上 use case。

## 效能檢查（寫入 `perf.md`）

U1（200／5 000 框）、U4、I1、I2、I5。與 `perf/budgets.json` 比對；**用本機實測校準 UI 類預算**，把這幾列的 `status` 標為已校準。超標就在本步驟內處理或明寫為未解決。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 畫布為什麼這樣切（Item／Layer／Tool）的 Purpose/Reason/Effect |
| 2 | diagram.md | 畫布與工具的類別圖；「單一 Item vs 批次繪製」的繪製管線圖 |
| 3 | explainer.html | 互動頁：滑桿調框數量、切換「逐一繪製／批次繪製」，並嵌入**本步驟實測**的幀時間數字（標明機器指紋） |
| 4 | storyboard.md | 45 秒「為什麼拖 5 000 個框還是很順」 |

## 完成定義

- [ ] 在目標機器上 build 成功，視窗可啟動
- [ ] 5 000 框可流暢平移縮放；測試通過
- [ ] 基準線已記錄；預算已校準或問題已記載
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(ui): add Qt shell, zoomable image canvas and ROI items

- CMake presets (Qt 6.8 / MSVC 2022 / Ninja), warnings as errors
- MainWindow with docks, theme tokens, i18n scaffolding
- ImageCanvas with zero-copy ImageItem, batched RoiLayerItem, tool registry
- UI performance baseline (U1, U4, I1, I2, I5) recorded in perf.md
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 CLAUDE.md、docs/STATUS.md、docs/plan/S02-ui-shell.md，
再依 CLAUDE.md 的 Step Protocol 完成步驟 S02（分支 step/02-ui-shell）。只做 S02。第一件事是驗證 MSVC/Qt/CMake 工具鏈。
完成後：commit、合併到 main、更新 STATUS.md、對我提示 commit 內容，並用 spawn_task 為 S03 開新 session。
```
