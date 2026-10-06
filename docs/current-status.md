# 目前狀態（進度表）

新 session 開工前先讀這份，再讀 `docs/plan/Sxx-*.md`。步驟完成後由該步驟的 session 更新。原檔名 `STATUS.md`，2026-10-05 更名為本檔以符合共用結構（見 `docs/adr/000-ai-agent-structure.md`）。

- 更新日期：2026-10-06
- 階段：S00、S01、S02、S03、S04 完成；S05 待開始。S01–S04 都已用 `--no-ff` 合併到 `main` 並打 tag（`step-01-done`…`step-04-done`）；AI 共用結構與 commit 格式規則也已合併到 `main`（全部只在本機，**沒有 push**）。**有兩件事等使用者決定**：（1）[ADR-009](adr/009-ui-toolkit-review-after-s02.md)：I1 在預算邊緣、真實視窗超標，要不要重審 UI 工具集；（2）**U3（分數滑桿到重繪）超出預算**，要降低成本還是重新定義這個預算（見下方 S04 一節與 `docs/steps/S04-presenter/perf.md`）

| 步驟 | 層級 | 主題 | 狀態 | 分支 | tag |
|---|---|---|---|---|---|
| S00 | 基礎 | 版控 + 總體計畫 + 架構導覽 | ✅ 完成 | `step/00-master-plan` | `step-00-done` |
| S01 | 介面 | UX 規格與可點擊原型（含樣本庫與 Board › Component › Box 階層） | ✅ 完成 | `step/01-ux-spec` | `step-01-done` |
| S02 | 介面 | Qt UI 骨架與影像畫布 | ✅ 完成（⚠ I1 待使用者決定） | `step/02-ui-shell` | `step-02-done` |
| S03 | 系統 | 核心：ports、use case、DI、執行緒、PerfProbe | ✅ 完成 | `step/03-core` | `step-03-done` |
| S04 | 系統 | Presenter、互動狀態機、復原/重做 | ✅ 完成（⚠ U3 超標、待使用者決定） | `step/04-presenter` | `step-04-done` |
| S05 | 底層 | Domain：晶格估計與補漏（純 C++） | ⏳ 下一個 | `step/05-lattice` | |
| S06 | 底層 | 影像緩衝、轉接器、Recipe 儲存 | ⬜ | `step/06-image-storage` | |
| S07 | 底層 | OpenCV back end | ⬜ | `step/07-opencv` | |
| S08 | 底層 | HALCON back end | ⬜ | `step/08-halcon` | |
| S09 | 底層 | 契約測試與 A/B 比較 | ⬜ | `step/09-contract-ab` | |
| S10 | 驗證 | 效能測試 harness 與回歸閘門 | ⬜ | `step/10-perf-harness` | |
| S11 | 驗證 | 效能優化（先量再改） | ⬜ | `step/11-optimize` | |
| S12 | 發行 | 匯出、打包、最終文件 | ⬜ | `step/12-release` | |

圖例：✅ 完成　⏳ 下一個　⬜ 尚未開始　⚠️ 有未解決問題

## S01 的產出與使用者決定（2026-10-05）

- 設計：`docs/ux/ux-spec.md`、`tokens.json`、`copy.json`；ADR-001（Widgets + QGraphicsView）、ADR-002（復原模型）、ADR-006（樣本庫與階層）全部「採用」。四種輸出與 `perf.md` 在 `docs/steps/S01-ux-spec/`。
- **範圍在 S01 中擴大**：使用者看原型後提出第二個流程（框任意區域 → 與樣本庫比對 → 列出對應的 CAD 框），決定兩種流程並存，並提供三份 TRI 參考文件。模型變成 Board › Component › Box，框由「元件類型」推導，見 ADR-006。後續步驟的影響已用一行提醒寫在 `docs/plan/S02`–`S10`、`S12` 開頭。
- 使用者決定：UI 預算（U/I）照原數字＋三點定義；兩種流程並存；庫與板子是**複本**（TRI 的 Merge）；庫存 **µm**＋影像校正值；疊圖固定亮色；接受／捨棄後 `Ctrl+Z` 回 Preview；`Esc` 捨棄不確認；手動補的元件標示「手動」；訊息不蓋在影像上。
- 驗證：`ste-lint --strict` 386 句 0 錯誤；`check-contrast` 淺 18、深 18、疊圖 24 組全過；`diagram.md` 12 張 Mermaid 語法全過（只驗語法）；原型頁內腳本整套 124 項斷言全過（另有比對範圍約 20 項）。**沒有建置、沒有 Qt 程式、沒有任何效能量測**（`perf.md` 全部「未量測」）。

## S02 的產出與發現（2026-10-05）

- **做了什麼**：CMake＋presets（debug／release／relwithdebinfo，`/W4 /WX`）、`src/` 三層（domain 只有 `image_view.h`）、主視窗與右側三個 dock 空殼、影像畫布（游標中心縮放、平移、適配、零複製影像）、批次繪製層與單一物件 Item（把手 24 px）、四個工具（V／S／R／A）與註冊表、暫代 controller、訊息列（固定高度，不蓋影像）、tokens → QPalette／QSS（淺色、深色、跟隨系統；字級 100／125／150%）、文案直接讀 `copy.json`（繁中、English）、`FakeScene`、5 個 Qt Test 單元測試執行檔（Release 與 Debug 都通過）＋效能基準線。細節：`docs/steps/S02-ui-shell/`。
- **效能（這台機器；細節與限制見 `docs/steps/S02-ui-shell/perf.md`）**：U1a、U1b、U4、I2、I5 通過，餘裕大。**I1（5 000 框平移縮放）在預算邊緣**：offscreen 13 次執行 p95 在 14.1–20.7 ms（預算 16.7，7 次通過）；**真實視窗 p95 22.1 ms，超標**。預算數字沒有改。
- **最重要的三個發現**：（1）第一版用 cosmetic 描邊畫框，I1 p95 約 50 ms；瓶頸是描邊的光柵化，改成整數像素的填色細矩形後快約 4 倍。（2）「批次繪製是 5 000 框變順的主因」這個假設**只有部分成立**：Item 不多時批次對平移縮放幀幾乎沒有幫助；價值在第一幀與 Item 很多時的 p95。（3）量測會被機器狀態影響 2 倍：要釘核心、關電源節流（`MeasurementHygiene.h`），而且同一份程式在不同時間量仍會飄 ±30%。
- **測試抓到的兩個真錯誤**：Rejected 的點線原本完全畫不出來（Qt 不畫長度 0 的虛線段）；繁中 Windows 下建置環境若字碼頁不一致，Ninja 掉標頭相依，只改標頭檔不會重新編譯（`env.ps1` 已處理）。
- **新增設計決定**：ADR-007（文案直接讀 `copy.json`）、ADR-008（訊息列固定高度）、ADR-009（待決定）；ADR-001 補了 S02 實測結果。`docs/ux/tokens.json` 新增 `sizes.messageBarHeight`，`copy.json` 新增殼層用的 key（`ux-spec.md` 修改紀錄有寫）。

## S03 的產出與發現（2026-10-05）

- **做了什麼**：domain 型別與座標慣例（`geometry.h`、ADR-010）；九個 port（`ITemplateBuilder`、`IPatternSearcher`、`IImageSource`、`IRecipeStore`、`IProgressSink`、`IPerfProbe`、`IExecutor`，加 S01 的 `ILibraryStore`、`ILibraryMatcher`）；`Result<T>`／`Error`；`CancelSource`／`CancelToken`；`JobRunner`（latest-wins）；use case `GenerateObjectsFromSample`（流程 A）與 `classify`（門檻 → 狀態）；暫代的 `SimpleGrid`；`BackendRegistry` 與組裝根（`--backend=fake|opencv|halcon`）；假 back end；`PerfProbe`；`ThreadPoolExecutor`；`tools/check-deps.mjs`（掛 CTest，13 個 fixture）；ADR-003、ADR-010；`AiInspTool.pro` 同步。細節：`docs/steps/S03-core/`。
- **驗證（都已實際執行）**：Release 與 Debug 各 17 個 CTest 項目全過（5 個 UI、8 個核心＋`tst_cli`、3 個 `check_deps*`）；`ste-lint --strict` 488 句 0 錯誤；`diagram.md` 10 張 Mermaid 解析全過（只驗語法）；`check-ai-structure.ps1` OK；14 個變異測試全部被抓到（`perf.md`）；`explainer.html` 在瀏覽器窗格實際操作過（淺色、深色、窄寬度）；`AiInspTool.pro` 以 qmake ＋ jom 命令列建置成功。
- **效能（這台機器，Release；細節見 `docs/steps/S03-core/perf.md`）**：`PerfProbe` 關閉的成本量不出來（和空迴圈差在 ±1 ns 內；目標 < 1 µs）；開啟約 257–291 ns／個計時；工作提交到開始約 0.04 ms；取消旗標生效約 0.01 ms（工作一直在看旗標時）。**暫代的晶格擬合在約 4 900 點要 97–134 ms**（T3 目標：1 000 點 ≤ 2 ms，屬 S05）。
- **要留意的三件事**：（1）取消的延遲上限是「工作兩次看旗標之間的最長時間」；每 1 ms 看一次旗標的工作，量到的延遲是 1.4–8.7 ms（中位數）、p95 約 16–17 ms，由 Windows 計時器解析度（系統狀態）決定。（2）`ImageView`、`SearchParams`、port 契約都已定下，S04 之後要改它們就是改契約。（3）OpenCV／HALCON 的座標與角度轉換（ADR-010）只是文件上的假設，**沒有驗證**，S07、S08 實測。
- **新增設計決定**：ADR-003（Qt Test 統一）、ADR-010（座標與角度慣例，解決 ADR-006 未決 #5）。`docs/plan/S04`–`S10` 開頭各加了一行「S03 完成後的提醒」。

## S04 的產出與發現（2026-10-06）

- **做了什麼**：互動規則放在沒有 Qt 的 application 層（ADR-011）：`InteractionStateMachine`（8 個狀態、19 個事件、轉移表照 ux-spec 第 6 節）、`InspectionSession`（狀態機＋文件＋提案＋選取；接受、捨棄、刪除、手動新增、移動、復原、重做、門檻）、`ProposalSession`（一份提案與它的修改，memento 式的復原）、`InspectionDocument`（已接受的板子，5 種 Command）、`UndoStack`（Command 不可變、深度 200、乾淨位置追蹤）。domain 新增 `box_expander.h`（Body／Pin 矩陣／Window 比例展開成框）。infrastructure 新增 `QtThreadPoolExecutor`（`IExecutor` 的 Qt 版）。presentation：`AutoRoiPresenter`（搜尋走 `JobRunner`、世代丟棄遲到的結果、忙碌提示 200 ms／1 s、取消立即回 Idle）、`ParamsPresenter`（120 ms 防抖）、`SceneBuilder`、`ObjectsModel`／`ObjectsPanel`（物件樹，延遲載入）、`ParamsPanel`、`MessageTexts`（結果卡）、`AppPresenters`（接線，main 與測試共用）；`MainWindow` 變成純 View；拖曳移動、方向鍵微調（600 ms 內合併成一個 Command）；`ShellController` 移除。ADR-011、ADR-012。細節：`docs/steps/S04-presenter/`。
- **驗證（都已實際執行）**：Release 29 個 CTest 項目全過（核心 19、UI 10；`tst_presenter` 是接上真的核心與視窗的整合測試）；植入 41 個缺陷全被測試抓到（過程與不完美處寫在 `perf.md`：第一輪 31／37，補測試後全抓到）。
- **效能（這台機器，Release，offscreen，連跑 3 次的範圍；細節見 `docs/steps/S04-presenter/perf.md`）**：U2 p95 8.0–9.3 ms（預算 50）、I3 p95 30.9–41.9 ms（預算 50，餘裕只有 16–38%）、I4 p95 3.5–4.9 ms（預算 100）、I6 p95 13.6–15.8 ms（預算 50）都在預算內。**U3 超標：5 832 個元件 p95 46.4–60.7 ms（預算 16）**；原因是整個畫布的重繪（19–30 ms），不是重新過濾（< 1 ms）；504 個元件時 p95 14–24 ms，也常超標。**預算數字沒有改。**
- **要留意的事**：（1）U3 的處置是使用者的決定：降低成本（S11 的候選：只重畫狀態改變的框、拆開幾何與狀態、快取背景一層），或重新定義這個預算。（2）三次執行差到 30%，I3 的餘裕又小，S10 的閘門不能用單次 p95。（3）只量了 offscreen；S02 的經驗是真實視窗慢 1.5–2.5 倍，所以 I3 在真實視窗可能更接近預算。（4）改了 S02 的繪製程式（ADR-012）：虛線與點線也畫成填色矩形。
- **新增設計決定**：ADR-011（互動規則在 application 層）、ADR-012（虛線與點線畫成整數像素的填色矩形，取代 S02 的做法）。`docs/plan/S05`–`S07`、`S10`、`S11` 開頭各加了一行「S04 完成後的提醒」。
- **S04 之後（2026-10-06，使用者要求）**：新增〔適配寬度〕（`W`）：影像寬度撐滿畫布，上下超出的部分靠平移看；載入影像仍是適配視窗。分支 `feat/fit-width`，已用 `--no-ff` 合併到 `main`（沒有 tag）。細節見 `docs/changelog-ai.md`。

## 預算校準狀態

`perf/budgets.json` 頂層仍是 `status: "proposed"`（T 類搜尋預算要等 S07）。UI 類：U1a、U1b、U4、I2、I5 標 `calibrated`，I1 標 `calibrated-at-limit`，每列附實測 p95；S04 起 U2、I3、I4、I6 也標 `calibrated`（附 3 次執行的 p95 範圍，`uiCalibrationS04`），**U3 標 `measured-over-limit`**；**沒有改任何 limit**（使用者在 S01 確認過）。S03 沒有新增或改變任何預算：T 類搜尋預算要真實 back end（S05、S07、S08），假 back end 的數字不能校準它們；S03 的量測只記在 `docs/steps/S03-core/perf.md` 與 `perf/baseline/casparhsu1-core.json`。基準線：`perf/baseline/casparhsu1.json`（S02）、`casparhsu1-core.json`（S03）、`casparhsu1-presenter.json`（S04）（只在這台機器可比較）。

## 未解決事項

- ~~UI 工具集（Widgets vs QML）~~ — 已由 S01 決定：Widgets + QGraphicsView（ADR-001）。重審條件（S02 量測 I1 超標且瓶頸在繪製）在真實視窗成立，已寫成 [ADR-009](adr/009-ui-toolkit-review-after-s02.md)，待使用者決定。
- ADR-006 未決：實例覆寫（只改這一個元件）、Panel 多連板、角度正負號、窗的演算法與參數、自由公式、庫的 System／Project 層。新預算 T7（區域掃描隨樣本庫大小 N）等 S07 實測後再提。
- 原型的比對是假的；真實的 `ILibraryMatcher` 在 S07／S08。HALCON「每個條目一個 shape model」是預期做法，未驗證。
- ~~MSVC 2022 工具鏈是否可用~~ — S02 已驗證：VS 2022 17.14、`cl` 19.44、Qt 6.8.0、CMake 3.30.5、Ninja 1.12.1 全部可用（細節在 `AI_CONTEXT.md` 環境表）。**HALCON 只有 runtime，沒有 `include`／`lib`（找不到 `HalconCpp.h`）：S08 前要補裝開發元件**；OpenCV 4.9.0 的 `find_package` 找得到（`OpenCV_DIR` 要指到 `build\x64\vc16\lib`），但沒有連結過
- **等使用者決定（ADR-009）**：I1 在預算邊緣、真實視窗超標——維持 Widgets 並在 S11 試 OpenGL 視埠？I1 的預算要不要註明視埠大小與平台？使用者平常的螢幕解析度與縮放習慣是什麼？
- **等使用者決定（U3）**：分數滑桿到重繪超出預算（5 832 個元件 p95 46–61 ms，預算 16 ms），降低成本（S11）還是重新定義這個預算？預算數字沒有改。
- 設定記憶（主題、字級、語言）、減少動態、高 DPI 螢幕上的實際外觀：**沒有做**；高 DPI 只驗證了粗細的換算。拖曳移動元件與方向鍵微調已在 S04 完成。
- **S04 沒有涵蓋的 UI**：ux-spec 12.4 列的 `LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）**沒有任何一步的 plan 檔涵蓋**，需要使用者決定放進哪一步。流程 B（區域掃描）的 Presenter 路徑已接好，但結果固定是「樣本庫是空的」（S06、S07 才有庫與比對器）。接受提案時產生的元件類型是暫時的，名稱 `X1…`、沒有料號；沒有校正值的影像暫以 1 µm ＝ 1 像素展開框（暫定，不是事實）；文件只在記憶體裡，沒有存檔；執行中不能切換 back end（下拉選單停用）。
- S02 的暫代品：`ShellController` 與 `copy.json` 的 `ui.shell.*` key 已在 S04 移除；`ImageLoader` 與 `ImageViewAdapter`（Qt 影像 → `ImageView`）仍是暫代，S06 的 `IImageSource` 取代；`ImageView` 的所有權規則 S06 定案
- S03 的暫代品：`SimpleGrid`（S05 取代；在約 4 900 點要 97–134 ms）、假 back end 不看影像（S07 才有真的搜尋）。流程 B（區域掃描）只有 port（`ILibraryMatcher`、`ILibraryStore`）與型別，沒有 use case、沒有假實作。
- `AiInspTool.pro` 已納入 application、`infrastructure/{backend_fake,perf,threading}` 與組裝根（S04 新增的資料夾在萬用字元底下，不必改）。S04 重新驗證了命令列 qmake ＋ jom（Release）建置，執行檔可以啟動；**沒有在 Qt Creator 裡實際載入過**；storage、OpenCV、HALCON 還沒有（S06–S08）。
- ADR-010 的 OpenCV／HALCON 轉換未驗證（見上）；`QBENCHMARK` 沒有用到也沒有驗證（ADR-003）。
- 真實板子影像（去識別化 1–2 張）— S07 前請使用者提供
- AI 共用結構（Codex 與 Claude Code）已於 2026-10-05 套用，commit `fd4fd2a` 並以 `--no-ff` 合併到 `main`，**沒有 push**。S01 的 Start prompt 已改成引用 `AGENTS.md` 與 `docs/current-status.md`，這些檔案都在 `main` 上。變更含：新增 `AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/`、`scripts/ai/`、`docs/adr/000-*`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`；改寫 `CLAUDE.md`；`docs/STATUS.md` 更名為本檔；14 個檔案的引用更新。stage 時一律明確列出路徑，不要 `git add -A`。
- `ai-project-template/`：AI 結構範本，與產品程式碼無關。2026-10-05 由 Claude Code 在這個 session，依使用者「設計共用結構」的要求產生，並依使用者決定納入版控、放在這個 repo。若日後要搬離，同步更新 `AI_CONTEXT.md` 等引用它的文件。
- Codex 端的 AGENTS.md 流程沒有在本機端到端測過（該機器的 Codex CLI 無法執行 `codex exec`）。
