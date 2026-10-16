# ADR-027 操作步驟的提示：訊息列的提示補強、左側的「操作步驟」dock（目前這一步標亮、每一步有逐格的示範畫面）、說明選單（F1）

- 日期：2026-10-15
- 決定者：使用者提出需求（「在操作介面上請提供步驟提示訊息，讓使用者知道要怎麼操作，最好還有影片操作提示的功能」）並在三個方案中選了 (a)＋(b)＋程式產生的 PNG 逐格動畫、說明選單 F1 打開面板；Claude Code (Claude Fable 5.1) 整理現況、提出方案與實作
- 狀態：採用

## 背景

使用者實際用程式建 CAD 框之後回報：介面要有步驟提示讓使用者知道怎麼操作，最好有影片提示。從程式碼確認的現況（S04–ADR-025）：

- 提示只有三種狀態有（`AutoRoiPresenter::hintText`）：`Idle`（沒樣本一句、有樣本一句）、`DrawingRegion`、`Searching`（**顯示的是 Idle 的那一句，過時**）；`Preview`、`NoMatch`、`Error` 的操作說明都在訊息列的卡片裡；`Empty` 是畫布中央的空白卡片（說明沒提 F5 與 Enter）。吐司（4 秒）會蓋過提示。
- 「現在該顯示哪一句」在 Presenter 裡用狀態判斷；沒有「總共幾步、做到哪一步」的概念。
- 沒有「說明」選單。ux-spec §5 的規則：除了空白卡片，什麼都不能蓋在影像上（檢測員要看著料件）。
- 這台機器的 Qt 6.12.0 **沒有 Qt Multimedia**（`lib` 沒有 `Qt6Multimedia*`、`plugins` 沒有 `multimedia/`）；imageformats 只有 gif、ico、jpeg、svg，而且 Qt 寫不出 GIF。S01 的分鏡（`docs/steps/S01-ux-spec/storyboard.md`）是原型的流程（存入樣本庫、類型編輯器），和 ADR-025 之後的「S → F5 → Enter」不同。

比較過的方案（完整的比較在這個 session 的筆記，摘要）：

| 方案 | 內容 | 評估 |
|---|---|---|
| (a) 訊息列一行提示的補強 | 每個狀態都有「下一步」 | 最便宜；但一行 96 px、吐司會蓋掉、看不到全貌 |
| (b) 可收合的「操作步驟」面板 | 五步的清單、目前這一步標亮、每一步的示範 | 看得到全貌與進度、不和訊息列搶位置、不蓋影像；程式量較多 |
| (c) 第一次開啟時的導覽對話框 | 三頁、〔下次不再顯示〕 | 看一次就關、不知道做到哪；單獨做幫助有限 |
| 影片：Qt Multimedia 播 mp4 | 要安裝模組、部署 ffmpeg 後端 | 不建議：只為幾段示範多一個部署的模組 |
| 影片：GIF（`QMovie`） | 手動錄、256 色 | 可行，但 UI 改了就要重錄 |
| **影片：逐格 PNG 的關鍵畫面** | 程式從合成板子走一遍流程抓畫面 | 全彩、可重現、UI 改了重跑一次就更新 |
| 影片：外部檔案用系統播放器開 | 零依賴 | 之後使用者自己錄的長影片可以用 |

## 決定

1. **訊息列的提示補強（a）**：`Searching` 改成自己的提示（`ui.search.hint`：「搜尋中… · Esc 取消 · 改角度範圍會重新搜尋」）；空白卡片多一行三步的摘要（`ui.empty.steps`：「S 框一個樣本元件 → F5 搜尋 → Enter 接受。左側的「操作步驟」（F1）有示範。」）。預覽的卡片**沒有**加「下一步」：訊息列固定 96 px（ADR-008），再多一行會把〔接受〕〔捨棄〕推到要捲動才看得到；接受之後的微調在 Idle 的提示（`ui.sample.hintIdle`）與面板的第五步。
2. **「操作步驟」dock（b）**，左側（`Qt::LeftDockWidgetArea`，工具列旁邊），預設打開，檢視 › 面板 可以關、關掉記在 `QSettings`（`guidance/visible`，只在使用者切換時寫，視窗關閉時 dock 跟著藏起來不算）。五步：① 開啟影像（Ctrl+O）② 框一個樣本（S）③ 搜尋（F5）④ 接受（Enter）⑤ 微調（B、拖把手、選取資訊）。目前這一步用主題的 Highlight 色標亮（palette，跟著主題走）、做完的打勾並變淡、進行中（拖曳中、搜尋中）與要重做（沒有結果、搜尋失敗、影像打不開）在那一步下面多一行。
3. **規則在 application**（ADR-011）：`application/interaction/StepGuide`（`GuideStep`、`GuideStatus`、`guideStatus(const InspectionSession&)`，純函式、沒有 Qt，`tst_step_guide` 16 項逐條驗證）。做完的判斷看資料（有影像；有樣本或有自動產生的元件；在預覽中或有自動產生的元件；有自動產生的元件）；目前這一步看狀態；`Idle` 時「有樣本的類型在板上還沒有自動產生的元件」就是〔搜尋〕（新樣本、捨棄或取消之後），否則〔微調〕。**不能**用 `lastSearch()`：回到 `Idle` 時它一律被清掉。已知的限制：一個樣本搜尋後一個都沒找到，接受之後面板仍指著〔搜尋〕。
4. **Presenter 只在狀態變了的時候才交給 View**（`AutoRoiPresenter::publishGuidance`，`GuideStatus` 有 `operator==`）：分數門檻拖動留在 `Preview`，面板一次都不更新（U3 不受影響；`tst_presenter` 數 `updateCount`）。面板自己也只重畫有變的列（Debug 實測面板一次更新約 45 ms 版面＋60 ms 重畫）。`IGuidanceView` 只有一個方法（`setGuideStatus`）；面板自己從 `copy.json` 取文字（步驟是固定的，不是資料）。
5. **示範＝逐格的關鍵畫面**（`presentation/guidance/GuideDemo.h`：每一步 2–3 張，共 13 張；張數、檔名 `docs/ux/guidance/<步驟>-<n>.png`、字幕 `ui.guide.demo.<步驟>.<n>` 三邊一致，`tst_messages` 檢查每一張都在資源裡、每一句字幕都有）。面板用 `QTimer` 每 1.5 秒換一張、循環；跟著目前的步驟走，點另一步看那一步的；面板看不到時計時器停下。**UI 執行緒不等任何解碼或平滑縮放**：面板看得到、第一次看那一步時才在背景解碼（`QtConcurrent::run` ＋ 掛在面板上的 `QFutureWatcher`，`QPixmap` 回到 UI 執行緒才建；presentation 因此多用 Qt Concurrent，`aoi_deps.pri`），解碼結果整個行程共用一份；縮到面板寬度的平滑縮放也在背景做，做好之前先用快速縮放頂著，每個寬度只縮一次；示範圖的寬度不參與版面（`QSizePolicy::Ignored`）、面板的 `sizeHint` 固定成 dock 的最小寬度（內容變了不推寬 dock、不讓畫布重排）；畫面存成 400 px 寬。第一版在 UI 執行緒同步解碼、縮放 720 px 的 PNG：Release 的 I6 從 13.8 變 20.9 ms（每次換步驟都重新縮放），Debug 的 `tst_presenter`（每個測試台都開一個面板）超過 300 秒逾時；改成上面的做法之後 I6 回到 14.9 ms。畫面由 `tests/ui/record_guidance`（不是測試，`tests.pro` 的 `AOI_TOOLS`，和 `tst_presenter` 同一個測試台：產品的接線、合成板子、假 back end）走一遍 S → F5 → Enter → B 產生，400 px 寬；UI 改了版面或文案之後重跑一次再重新建置（`aoi_ux.qrc` 內嵌）。
6. **說明選單**（`menu.help`）的〔操作步驟〕（`action.guide`，`F1` ＝ `QKeySequence::HelpContents`）把 dock 打開並帶到前面。不做 (c) 的對話框：面板預設開著，第一次使用自然看到。

## 證據

- 現況（Observed Fact）：`AutoRoiPresenter.cpp` 的 `hintText()`（`Searching` 回 `ui.idle.hint`）、`MessageTexts.cpp` 的三種卡片、`MessageBar.cpp` 的優先順序、`EmptyCard.cpp` 的四個 key、`MainWindow.ui` 的選單（沒有說明）。
- Qt Multimedia：`D:\Qt\6.12.0\msvc2022_64\lib` 沒有 `Qt6Multimedia*`，`plugins/` 沒有 `multimedia/`；`plugins/imageformats` 只有 qgif、qico、qjpeg、qsvg。
- 效能（`test.ps1 -Perf -Filter perf_ui_s04`，Release，p95 ms）：2026-10-16 前後交替各兩次，U3 前 29.4、13.9／後 10.1、15.2，I6 前 3.6、4.9／後 2.8、3.1，全部在預算內、差異在雜訊內（這時測試台已經關著 dock）。前一晚機器慢、dock 開著時：I6 前 13.8 → 第一版 20.9（每次換步驟在 UI 執行緒重新縮放 PNG）→ 背景縮放後 14.9。門檻拖動不呼叫 View 由 `tst_presenter` 的 `updateCount` 鎖住。數字的細節在 `docs/changelog-ai.md`。
- Debug 的對照：2026-10-15 晚上 `tst_presenter` 的 `aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce`（等 2 秒內看到「忙碌」）在本分支一直失敗，**改之前的 `main`（eb95b79，主 checkout 的 Debug 建置）也失敗**，整組 231 秒；隔天機器正常時本分支的 Debug 58／58 通過、`tst_presenter` 整組 85 秒。它是跟機器速度有關的不穩定測試，不是這次的修改造成的。

## 否決的替代方案

- **浮在畫布上的導覽氣泡**：違反 ux-spec §5（不蓋影像）。
- **規則寫在 Presenter 或面板裡**：每條規則都要開視窗才測得到（ADR-011 的理由）。
- **Presenter 每次 `publishAll` 都更新面板**：門檻拖動每個畫格都會碰到 dock，進 U3。
- **Qt Multimedia 播影片**：這台機器沒有、要多部署一個模組（含 ffmpeg 後端）。
- **GIF**：Qt 寫不出 GIF，素材要手動錄；UI 改了就過期。
- **第一次開啟的對話框**：看一次就關，之後找不到；用「預設開著的面板＋F1」取代。
- **面板的更新延後 80 ms 合併送出**（試過）：想讓面板的更新不和搜尋開始的那一批同步工作擠在一起，但 Debug 的慢搜尋測試沒有因此變好（後來證實那個測試在改之前的 `main` 也失敗，見「證據」），只多了一個計時器、測試還要等；撤掉，維持同步送出。
- **把面板放右側**：右側已有三個 dock，會互相擠；左側只有工具列。

## 後果

- 新的 dock 讓畫布的預設寬度少約一個 dock 的寬度（`tokens.sizes.dockMinWidth`）；使用者可以關掉（記住）。
- UI 改了版面或文案要重跑 `record_guidance` 並重新建置，否則示範畫面是舊的（沒有測試抓得到「畫面過期」，只抓得到「少了一張」）。
- 示範畫面是合成板子（`FakeScene`）與假 back end 的畫面，不是真實的板子；要真實板子的影片，之後由使用者錄、用外部播放器開（本 ADR 沒做）。
- **測試台（`PresenterRig`）預設把操作步驟 dock 關著**（和 `record_guidance` 一樣）：`tst_presenter` 85 個測試台每個都開一個面板，面板的版面與重畫在 Debug 每次狀態變化約 100 ms，整組從 `main` 的 231 秒變成 297 秒，貼著 300 秒的逾時。要測面板的測試自己 `showGuidance()`；`tst_shell` 的面板測試不經測試台。`perf_ui_s04` 用同一個測試台，因此不含這個 dock（門檻拖動本來就不碰它）；`perf_e2e` 用自己的視窗，含 dock。
- 文案多 36 個 key（`ui.guide.*`、`menu.help`、`action.guide`、`ui.search.hint`、`ui.empty.steps`）。

## 驗證（2026-10-15）

見 `docs/changelog-ai.md` 的這一筆（`test.ps1` 預設與 `-Config Debug`、`ste-lint`、`check-deps`、`perf_ui_s04` 前後）。
