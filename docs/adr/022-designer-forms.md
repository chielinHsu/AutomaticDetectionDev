# ADR-022 介面的版面改用 Qt Designer 的 `.ui`：外框與面板在表單裡，畫布與 model 留在程式碼，文字與主題照舊

- 日期：2026-10-08
- 決定者：使用者在 S09 之前的 session B 決定要做與範圍：主視窗外框（選單、工具列、左側工具按鈕、停靠面板的配置）、樣本庫面板、物件面板、比對參數面板、空白卡片、訊息列改成 `.ui`；畫布（`QGraphicsView`、框的繪製）與物件表的 model 維持程式碼，Designer 裡用提升的佔位元件；文案仍來自 `copy.json`、主題仍來自 `tokens.json`；測試依賴的 objectName 保留。表單怎麼切、哪些留在程式碼是 Claude Code (Claude Opus 5.5) 的提案並實作。
- 狀態：採用

## 背景

S02–S08 的版面全部寫在程式碼裡（`MainWindow::build*`、各面板的建構子）。使用者要在 Qt Creator 的 Designer 裡看到並調整版面（ADR-018 改用 qmake 的理由之一）。限制：

- 文案的唯一來源是 `copy.json`（ADR-007，`JsonTranslator`＋`qtTrId`），不產生 `.ts`。Designer 的字串預設會進 `retranslateUi` 用 `tr()` 翻譯，兩套翻譯會打架。
- 畫布要求零複製的繪製與批次的框（S02、ADR-012），I1、U1、U3 有預算；不能為了 Designer 改它的結構。
- `check-deps` 不掃 `.ui`；presentation 的分層規則（只能用 Qt Widgets 與 presentation 的類別）要靠表單本身遵守。

## 決定

1. **五個表單，和類別放在一起**：`shell/MainWindow.ui`、`shell/MessageBar.ui`、`shell/EmptyCard.ui`、`objects/ObjectsPanel.ui`、`params/ParamsPanel.ui`。樣本庫面板目前只有篩選欄與空狀態文字，沒有自己的類別，放在 `MainWindow.ui` 的 dock 裡。`presentation.pro` 的 `FORMS` 明確列出；uic 的輸出在建置資料夾（`.uic/aoi_presentation`），只有各類別的 `.cpp` include `ui_*.h`（標頭只前置宣告 `Ui::` 類別，用 `unique_ptr` 持有），所以別的目標不必看得到 uic 的資料夾。
2. **類別名稱帶命名空間**（`<class>aoi::ui::MainWindow</class>`，提升元件 `aoi::ui::ImageCanvas`）：uic 產生 `aoi::ui::Ui::MainWindow`，不會和精靈留下的 `AiInspTool/mainwindow.ui`（`Ui::MainWindow`，沒有使用、不在建置裡）同名（實測 uic 6.12.0）。
3. **提升的佔位元件**：畫布（`QGraphicsView` → `ImageCanvas`）、訊息列（`QFrame` → `MessageBar`）、物件面板與參數面板（dock 的內容 → `ObjectsPanel`、`ParamsPanel`，不多包一層 widget）。提升元件由 uic 以 `new 類別(parent)` 建立，所以 `ImageCanvas` 與 `MessageBar` 多了只收 parent 的建構子，tokens 用內建的 `ThemeTokens::builtin()`（程式只有這一份；測試照舊可以傳別的）。畫布內部（Item、Layer、Tool）、物件樹的 model（`ObjectsModel`）與欄寬都沒有動。
4. **文字**：表單裡的字串一律 `notr="true"`，只是讓 Designer 看得懂的佔位；uic 的 `retranslateUi` 因此是空的。執行時各類別自己的 `retranslate`／`retranslateUi` 照舊從 `copy.json` 設定（語言名稱「繁體中文」「English」是不翻譯的，表單裡的就是最後的文字）。**主題**：照舊由 `ThemeManager`（調色盤與 QSS）套用；表單沒有寫顏色與字型大小（只有粗體），尺寸的 token（視窗最小、dock 最小寬度、訊息列高度、狀態列高度）照舊在程式碼裡設。
5. **留在程式碼裡的**（Designer 表達不了，或是行為不是版面）：快捷鍵（標準快捷鍵跟著平台、重做有兩個）、互斥的 `QActionGroup`（工具、主題、字級、語言）、字級選單的項目（來自 `tokens.json`）、dock 的開關（`toggleViewAction`）、工具列上的 back end 標籤與選單與彈性空間（Designer 放不進 `QToolBar`；插在〔復原〕前面）、狀態列的標籤、訊息列卡片的按鈕（依內容建立）、影像位元數選單的項目（帶 data）、`foregroundRole`／`backgroundRole`、空白卡片標題的字級倍數（跟著字級縮放）。
6. **objectName**：測試與程式碼用到的都保留（`topBar`、`toolRail`、`libraryDock`、`objectsDock`、`paramsDock`、`messageBar`、`messageHint`、`messageCard`、`messageNotice`、`messageProgress`、`messageCancel`、`emptyCard`、`stateChip0`–`3`；`backendCombo`、`backendStatus`、`statusSummary` 在程式碼裡設）。

## 證據

- uic：五個表單在 qmake 的建置裡都由 uic 編譯（Release、Debug、兩個選項都關的組合），自家程式碼 `/W4 /WX` 沒有警告。
- 所有 UI 測試照舊通過（`tst_shell`、`tst_presenter`、`tst_params`、`tst_objects_model`、`tst_canvas`、`tst_tools`、`tst_items`、`tst_messages`、`tst_theme_i18n`），數字見 `docs/changelog-ai.md` 的這一筆。
- 效能（`test.ps1 -Perf -Filter 'perf_ui*'`，Release，改之前在 `main` 量一次、改之後量一次）：見下面「驗證」。

## 否決的替代方案

- **表單裡的字串用 Designer 的翻譯（`tr()`、`.ts`）**：和 ADR-007 的 `copy.json` 衝突，同一段文字會有兩個來源。
- **畫布也在 Designer 裡排（例如把 `RoiLayerItem` 的設定做成屬性）**：畫布的效能與零複製是 S02 量過才定的結構，Designer 幫不上忙；使用者也指定維持程式碼。
- **dock 的內容多包一層 `QWidget` 再放提升元件**：uic 可以直接把提升元件當 dock 的內容（實測），少一層、和原本的 widget 樹相同。
- **`ImageCanvas`／`MessageBar` 建好之後再 `setTokens`**：畫布的 `OverlayStyle` 與 Item 在建構時就參照 tokens，事後換要改很多地方；程式只有一份 tokens，用內建的就夠。
- **表單的類別名稱不帶命名空間**：會產生全域的 `Ui::MainWindow`，和精靈留下的 `mainwindow.ui` 同名（雖然那個不在建置裡）。

## 後果

- 在 Qt Creator 打開 `AiInspTool.pro`，專案樹的 `aoi_presentation` 底下有 Forms，雙擊就用 Designer 編輯。提升元件在 Designer 裡顯示成基底類別（畫布是空白的 `QGraphicsView`），執行時才是真的類別。
- 在 Designer 新增的文字要勾掉「可翻譯」（notr），並在對應類別的 `retranslate` 從 `copy.json` 設定；新增的元件要有 objectName。改了表單只要重新建置（qmake 的 Makefile 會重新跑 uic）。
- Designer 存檔會依它的格式重寫整個 `.ui`（屬性的順序、縮排）；第一次存檔的 diff 可能比實際的修改大。

## 驗證（2026-10-08，都實際執行過）

- 建置與測試：`scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）54／54 通過；Debug 與兩個選項都關的組合見 `docs/changelog-ai.md` 的這一筆。五個表單都由 uic 編譯，`/W4 /WX` 沒有警告。
- 效能（`test.ps1 -Perf -Filter 'perf_ui*'`，Release，offscreen，p95 ms）。「改之前」：第 1 輪是改任何東西之前的 `main`（`75c6b3a`），第 2–4 輪是第 2 件的 commit（`65109a2`，`-NoHalcon` 的 Release 建置；UI 的量測不用 HALCON）；「改之後」是這個 commit。前後交替量，同一輪的兩個數字可以比：

  | 輪 | U1a 前 → 後 | U1b 前 → 後 | I1 前 → 後 | U3 前 → 後 |
  |---|---|---|---|---|
  | 1 | 1.68 → 1.82 | 4.62 → 4.06 | 7.23 → 6.85 | 23.3 → 18.0 |
  | 2 | 1.88 → 1.52 | 4.14 → 4.24 | 9.07 → 6.99 | 18.0 → 18.0 |
  | 3 | 2.60 → 4.14 | 6.64 → 13.07 | 7.75 → 7.65 | 32.9 → 19.3 |
  | 4（只量 `perf_ui_baseline`） | 3.49 → 3.39 | 8.55 → 6.60 | 9.15 → 7.52 | — |

  I1 與 U3 每一輪都沒有變差。U1 在第 3 輪偏高（中位數也從 1.4 變成 3.3 ms），同一輪改之前的 U3 也異常（32.9）：那段時間機器有別的負載；第 4 輪補量，改之後比較好。U1 量的是畫布本身（測試直接建 `ImageCanvas`），這次畫布只多了一個建構子，表單碰不到量測的路徑。預算：U1a 33、U1b 50、I1 16.7 都通過；U3 照舊超過 16（S04 起的已知問題，ADR-009 與 `docs/current-status.md`）。
- Qt Creator 的 Designer：見 `docs/changelog-ai.md` 的這一筆（請使用者實際打開確認）。
