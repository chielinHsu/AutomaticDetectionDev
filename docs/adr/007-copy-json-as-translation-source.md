# ADR-007 文案：`copy.json` 直接當翻譯來源（不產生 `.ts`／`.qm`）

- 日期：2026-10-05
- 決定者：Claude Code (Claude Sonnet 5.5)（S02 實作時決定；使用者未被問到，因為這是實作細節，可逆）
- 狀態：採用

## 背景

S01 把全部 UI 文案寫在 `docs/ux/copy.json`（繁中＋英文，佔位字用 `{name}`），並在 `copy.json` 的 `note` 與 S02 計畫寫「S02 轉成 Qt 的 id-based translations，`{name}` 轉成 `%1` 或 `%n`」。
S02 要決定怎麼把它變成執行時的字串。

## 決定

1. `copy.json` 內嵌為 Qt 資源（`:/aoi/ux/copy.json`），由 `JsonTranslator : QTranslator` 在執行時讀取。程式裡用標準的 `qtTrId("ui.noMatch.title")`，語言切換用 `QCoreApplication::installTranslator`。
2. **佔位字維持具名**：`uiText("ui.preview.summary", {{"found","118"}, …})` 在翻譯**之後**把 `{found}` 換成值，不轉成 `%1`。
3. 找不到的 key，`qtTrId` 回傳 key 本身（畫面上看得到錯字）；不存在的語系，`Translations::install` 回傳 false 且不改變目前的語系。
4. 測試（`tst_theme_i18n`）逐 key 檢查：兩種語系都有字、兩種語系的佔位字集合相同。

## 證據

事實（已觀察）：

- `copy.json` 有 100 多個 key，兩種語系；`tst_theme_i18n` 在本機通過。
- `{name}` 轉 `%1` 的問題：`%1`、`%2` 是**位置**，兩種語言的語序可以不同（例如英文「Found {found} · Inferred {inferred}」與中文的順序一旦不同，位置就對錯）；具名佔位字沒有這個問題。
- Qt 6.8 的 `QCoreApplication::installTranslator` 會產生 `LanguageChange`，**排入事件佇列**（下一輪事件迴圈才送達），實測於本機（`MainWindow::changeEvent` 在 `processEvents()` 之後才被呼叫）。

假設（未驗證）：

- 沒有專業翻譯人員使用 Qt Linguist 的需求。若之後有，可以由 `copy.json` 產生 `.ts`（單向），不必改程式。

## 否決的替代方案

- **由 `copy.json` 產生 `.ts`，再用 `lrelease` 編成 `.qm`**：標準流程，但多一個產生步驟（要 Node 或另一個 CMake 步驟）、多一份「看起來可以改但會被覆蓋」的檔案，而且 `{name}` 轉 `%1` 的位置問題要另外處理。目前沒有需要這個流程的人。
- **程式碼裡寫 `tr("中文原文")`**：文案分散在 C++ 裡，`copy.json` 與原型（S01 的 `explainer.html`）就不是唯一來源。

## 後果

- 文案只有一個來源（`docs/ux/copy.json`）；改文案不必重新產生任何東西，只要重建（資源會重新內嵌）。
- 沒有複數形處理（`%n`）。目前所有文案都不需要；若英文需要「1 box／2 boxes」，要加規則，並新增一筆 ADR。
- `copy.json` 中 S02 新增的 key（`menu.*`、`ui.shell.*`、`ui.status.cursor`、`action.quit`）屬於暫代殼層，`ui.shell.*` 在 S04 的 Presenter 接上後會移除。
