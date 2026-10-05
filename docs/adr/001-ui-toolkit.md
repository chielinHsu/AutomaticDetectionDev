# ADR-001 UI 工具集：Qt Widgets + QGraphicsView

- 日期：2026-10-05
- 決定者：使用者（在 S01 session 中選定），Claude Code (Claude Sonnet 5.5) 整理證據與撰寫
- 狀態：採用

## 背景

總體計畫的風險 #1：畫布與介面用 Qt Widgets + `QGraphicsView`，還是 QML / Qt Quick。選擇會影響 S02 的骨架、S04 的 Presenter 綁定方式、效能量測的做法，以及依賴方向檢查。

畫布要承擔：5 MP 影像、最多 5 000 個框（預算 U1b、I1）、游標中心縮放、拖曳樣本框、把手 ≥ 24 px 的點擊區。

## 決定

用 **Qt Widgets + `QGraphicsView`**。使用者在 2026-10-05 從「Widgets + QGraphicsView」「QML / Qt Quick」「先做 S02 量測再決定」三個選項中選了第一個。

## 證據

事實（已觀察）：

- 本機 Qt 6.8.0 msvc2022_64 同時有 `Qt6Widgets`、`Qt6Quick`、`Qt6QuickControls2`、`Qt6QuickTest`、`Qt6Test`（`D:\Qt\6.8.0\msvc2022_64\lib\cmake`）。**兩條路在這台機器上都可行**，可行性不是差異點。
- 整個專案的分層與依賴規則靠 `tools/check-deps.mjs` 掃 C++ 的 `#include`（`docs/architecture.md`、S03 計畫）。QML 檔案不在它的掃描範圍。
- 效能計畫（`docs/perf-test-plan.md`）用 `QT_QPA_PLATFORM=offscreen` 加 `QTest` 注入滑鼠與滾輪事件量 I1、I2、I5；表格用 `QAbstractItemModel` 量 U2。這套方法直接對應 Widgets 的 `QGraphicsView` 與 `QTableView`。
- S01 的互動原型已把畫布互動規則（游標中心縮放、把手 24 px 點擊區、小框把手外推）跑過；這些規則用 `QGraphicsItem` 的事件與座標轉換可直接表達。

假設（未驗證，S02 要用數字驗證）：

- 5 000 個框用一個自訂的批次繪製 Item（`RoiLayerItem`：只畫可見範圍、依縮放簡化）可達到 I1 p95 ≤ 16.7 ms。**這是 S02 的 U1、I1 量測要回答的問題**。
- QML 的 GPU 場景圖在大量圖形項時可能更順；沒有在本專案量過。
- `QGraphicsView` 不夠快時，可以改用 OpenGL 視埠作為備案；沒有驗證過。

## 否決的替代方案

- **QML / Qt Quick**：場景圖由 GPU 繪製、動畫與樣式彈性大。代價：多一種語言（QML / JS），依賴檢查掃不到；offscreen 的事件注入與幀時間量測要改用 Qt Quick Test 或視窗抓圖，方法與 `perf-test-plan.md` 不同；Presenter 要多一層 `QObject` 屬性綁定；要承擔 GPU 與驅動差異。本專案沒有觸控、沒有複雜動畫需求，這些好處用不到。
- **先量測再決定**：S02 同時做兩種最小畫布。多花一步，而兩條路都可行、而且 Widgets 的風險有明確的量測關卡，所以不值得。

## 後果

- S02 用 Widgets + `QGraphicsView` 起步：`ImageCanvas : QGraphicsView`、`RoiGraphicsItem`、`RoiLayerItem`。深色主題與樣式要用 `QPalette` / QSS 自己做（`docs/ux/tokens.json` 是來源）。
- **重審條件**：S02 量測若在批次繪製與縮放簡化都做完後，I1（5 000 框）p95 仍 > 16.7 ms，且 profiler 顯示瓶頸在繪製，才新增一筆 ADR 重審（並標明取代本筆）。沒有這個證據不推翻。
- Presenter 只依賴窄的 `I*View` / `I*Controller` 介面（`docs/ux/ux-spec.md` 第 11 節），不認識 `QWidget`。所以日後若要換 View 技術，不必改 Presenter。
- `presentation` 層依賴 Qt Widgets（`docs/architecture.md` 的表已是如此），`application` 與 `domain` 不變。
