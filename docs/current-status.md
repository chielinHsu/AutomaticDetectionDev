# 目前狀態（進度表）

新 session 開工前先讀這份，再讀 `docs/plan/Sxx-*.md`。步驟完成後由該步驟的 session 更新。原檔名 `STATUS.md`，2026-10-05 更名為本檔以符合共用結構（見 `docs/adr/000-ai-agent-structure.md`）。

- 更新日期：2026-10-06
- 階段：S00–S05 完成；S06 待開始。S01–S05 都已用 `--no-ff` 合併到 `main` 並打 tag（`step-01-done`…`step-05-done`）；AI 共用結構、commit 格式規則與 S04 之後的〔適配寬度〕也已合併到 `main`（全部只在本機，**沒有 push**）。**有兩件事等使用者決定**：（1）[ADR-009](adr/009-ui-toolkit-review-after-s02.md)：I1 在預算邊緣、真實視窗超標，要不要重審 UI 工具集；（2）**U3（分數滑桿到重繪）超出預算**，要降低成本還是重新定義這個預算（見下方 S04 一節與 `docs/steps/S04-presenter/perf.md`）

| 步驟 | 層級 | 主題 | 狀態 | 分支 | tag |
|---|---|---|---|---|---|
| S00 | 基礎 | 版控 + 總體計畫 + 架構導覽 | ✅ 完成 | `step/00-master-plan` | `step-00-done` |
| S01 | 介面 | UX 規格與可點擊原型（含樣本庫與 Board › Component › Box 階層） | ✅ 完成 | `step/01-ux-spec` | `step-01-done` |
| S02 | 介面 | Qt UI 骨架與影像畫布 | ✅ 完成（⚠ I1 待使用者決定） | `step/02-ui-shell` | `step-02-done` |
| S03 | 系統 | 核心：ports、use case、DI、執行緒、PerfProbe | ✅ 完成 | `step/03-core` | `step-03-done` |
| S04 | 系統 | Presenter、互動狀態機、復原/重做 | ✅ 完成（⚠ U3 超標、待使用者決定） | `step/04-presenter` | `step-04-done` |
| S05 | 底層 | Domain：晶格估計與補漏（純 C++） | ✅ 完成 | `step/05-lattice` | `step-05-done` |
| S06 | 底層 | 影像緩衝、轉接器、Recipe 儲存 | ⏳ 下一個 | `step/06-image-storage` | |
| S07 | 底層 | OpenCV back end | ⬜ | `step/07-opencv` | |
| S08 | 底層 | HALCON back end | ⬜ | `step/08-halcon` | |
| S09 | 底層 | 契約測試與 A/B 比較 | ⬜ | `step/09-contract-ab` | |
| S10 | 驗證 | 效能測試 harness 與回歸閘門 | ⬜ | `step/10-perf-harness` | |
| S11 | 驗證 | 效能優化（先量再改） | ⬜ | `step/11-optimize` | |
| S12 | 發行 | 匯出、打包、最終文件 | ⬜ | `step/12-release` | |

圖例：✅ 完成　⏳ 下一個　⬜ 尚未開始　⚠️ 有未解決問題

## S01 的產出與使用者決定（2026-10-05）

- 設計：`docs/ux/ux-spec.md`、`tokens.json`、`copy.json`；ADR-001（Widgets + QGraphicsView）、ADR-002（復原模型）、ADR-006（樣本庫與階層）全部「採用」。四種輸出與 `perf.md` 在 `docs/steps/S01-ux-spec/`。驗證只有文件與原型（`ste-lint`、`check-contrast`、Mermaid、原型頁內 124 項斷言）；**沒有建置、沒有效能量測**。
- **範圍在 S01 中擴大**：使用者看原型後提出第二個流程（框任意區域 → 與樣本庫比對 → 列出對應的 CAD 框），決定兩種流程並存，並提供三份 TRI 參考文件。模型變成 Board › Component › Box，框由「元件類型」推導，見 ADR-006。後續步驟的影響已用一行提醒寫在 `docs/plan/S02`–`S10`、`S12` 開頭。
- 使用者決定：UI 預算（U/I）照原數字＋三點定義；兩種流程並存；庫與板子是**複本**（TRI 的 Merge）；庫存 **µm**＋影像校正值；疊圖固定亮色；接受／捨棄後 `Ctrl+Z` 回 Preview；`Esc` 捨棄不確認；手動補的元件標示「手動」；訊息不蓋在影像上。

## S02、S03 的重點（2026-10-05；完整內容在 `docs/steps/S02-ui-shell/`、`S03-core/` 與 `docs/changelog-ai.md`）

- **S02**：CMake＋presets（`/W4 /WX`）、影像畫布（游標中心縮放、平移、適配、零複製）、批次繪製層、四個工具、訊息列（固定高度）、主題與文案（直接讀 `copy.json`）、`FakeScene`；ADR-007、008、009。效能：U1a、U1b、U4、I2、I5 通過；**I1（5 000 框平移縮放）在預算邊緣**：offscreen 13 次 p95 14.1–20.7 ms（預算 16.7）、真實視窗 22.1 ms 超標（ADR-009 待決定）。
- **S02 的發現**：整數像素的填色細矩形比 cosmetic 描邊快約 4 倍；批次繪製的價值在第一幀與 Item 很多時；量測要釘核心、關電源節流（`MeasurementHygiene.h`），同一份程式不同時間仍會飄 ±30%；繁中 Windows 的字碼頁不一致會讓 Ninja 掉標頭相依（`env.ps1` 已處理）。
- **S03**：domain 型別與座標慣例（ADR-010）、九個 port、`Result`／`CancelToken`、`JobRunner`（latest-wins）、use case 與 `classify`、`BackendRegistry` 與 `--backend`、假 back end、`PerfProbe`、`ThreadPoolExecutor`、`tools/check-deps.mjs`；ADR-003、ADR-010。效能：`PerfProbe` 關閉的成本量不出來、開啟約 257–291 ns；提交到開始約 0.04 ms。
- **S03 要留意**：取消的延遲上限是工作兩次看旗標之間的最長時間（每 1 ms 看一次時 p95 約 16–17 ms，由 Windows 計時器解析度決定）；`ImageView`、`SearchParams`、port 契約已定下，改它們就是改契約；OpenCV／HALCON 的座標轉換（ADR-010）**沒有驗證**（S07、S08）。

## S04 的產出與發現（2026-10-06）

- **做了什麼**：互動規則放在沒有 Qt 的 application 層（ADR-011）：`InteractionStateMachine`（8 個狀態、19 個事件、轉移表照 ux-spec 第 6 節）、`InspectionSession`（狀態機＋文件＋提案＋選取；接受、捨棄、刪除、手動新增、移動、復原、重做、門檻）、`ProposalSession`（一份提案與它的修改，memento 式的復原）、`InspectionDocument`（已接受的板子，5 種 Command）、`UndoStack`（Command 不可變、深度 200、乾淨位置追蹤）。domain 新增 `box_expander.h`（Body／Pin 矩陣／Window 比例展開成框）。infrastructure 新增 `QtThreadPoolExecutor`（`IExecutor` 的 Qt 版）。presentation：`AutoRoiPresenter`（搜尋走 `JobRunner`、世代丟棄遲到的結果、忙碌提示 200 ms／1 s、取消立即回 Idle）、`ParamsPresenter`（120 ms 防抖）、`SceneBuilder`、`ObjectsModel`／`ObjectsPanel`（物件樹，延遲載入）、`ParamsPanel`、`MessageTexts`（結果卡）、`AppPresenters`（接線，main 與測試共用）；`MainWindow` 變成純 View；拖曳移動、方向鍵微調（600 ms 內合併成一個 Command）；`ShellController` 移除。ADR-011、ADR-012。細節：`docs/steps/S04-presenter/`。
- **驗證（都已實際執行）**：Release 29 個 CTest 項目全過（核心 19、UI 10；`tst_presenter` 是接上真的核心與視窗的整合測試）；植入 41 個缺陷全被測試抓到（過程與不完美處寫在 `perf.md`：第一輪 31／37，補測試後全抓到）。
- **效能（這台機器，Release，offscreen，連跑 3 次的範圍；細節見 `docs/steps/S04-presenter/perf.md`）**：U2 p95 8.0–9.3 ms（預算 50）、I3 p95 30.9–41.9 ms（預算 50，餘裕只有 16–38%）、I4 p95 3.5–4.9 ms（預算 100）、I6 p95 13.6–15.8 ms（預算 50）都在預算內。**U3 超標：5 832 個元件 p95 46.4–60.7 ms（預算 16）**；原因是整個畫布的重繪（19–30 ms），不是重新過濾（< 1 ms）；504 個元件時 p95 14–24 ms，也常超標。**預算數字沒有改。**
- **要留意的事**：（1）U3 的處置是使用者的決定：降低成本（S11 的候選：只重畫狀態改變的框、拆開幾何與狀態、快取背景一層），或重新定義這個預算。（2）三次執行差到 30%，I3 的餘裕又小，S10 的閘門不能用單次 p95。（3）只量了 offscreen；S02 的經驗是真實視窗慢 1.5–2.5 倍，所以 I3 在真實視窗可能更接近預算。（4）改了 S02 的繪製程式（ADR-012）：虛線與點線也畫成填色矩形。
- **新增設計決定**：ADR-011（互動規則在 application 層）、ADR-012（虛線與點線畫成整數像素的填色矩形，取代 S02 的做法）。`docs/plan/S05`–`S07`、`S10`、`S11` 開頭各加了一行「S04 完成後的提醒」。
- **S04 之後（2026-10-06，使用者要求）**：新增〔適配寬度〕（`W`）：影像寬度撐滿畫布，上下超出的部分靠平移看；載入影像仍是適配視窗。分支 `feat/fit-width`，已用 `--no-ff` 合併到 `main`（沒有 tag）。細節見 `docs/changelog-ai.md`。

## S05 的產出與發現（2026-10-06）

- **做了什麼**：domain 第一次有 .cpp（`src/domain/lattice/`，只用標準庫；`aoi_domain` 由 INTERFACE 改 STATIC）：`LatticeEstimator`（`fit`、`fitAll`：kNN 位移向量分群找 u、v，RANSAC 挑原點，範圍逐次加倍的逐步精修，連通，Lagrange 約化）、`LatticePredictor`（索引範圍內的缺格，由近到遠）、`LatticeAccumulator`（正規方程，加點／拿掉點／求解都是 O(1)）、`NonMaxSuppression`（旋轉矩形的精確重疊＋空間雜湊）、`SpatialGrid`、`IVectorClustering`（策略）。接受規則寫成 ADR-013：寧可沒有晶格，也不要錯的晶格（至少 3 個內點、補缺格要有證據、二維成立逐列判斷、連通、信心＝證據 × 精度 × 涵蓋率 × 一致性；不往索引範圍外推）。use case：`SimpleGrid` 刪除；種子先做 NMS；缺格與已有候選重疊就不補；缺格由近到遠、上限內留近的；驗證找到的元件用 O(1) 重新擬合，推算的位置用修正後的。`presentation/` 沒有改。細節：`docs/steps/S05-lattice/`。
- **驗證（都已實際執行）**：Release 與 Debug 各 36 個 CTest 項目全過（新增 `tests/domain/` 的 7 個執行檔與 `s00_demo_fixture_is_current`；`tst_grid` 搬進 `tst_lattice`）。情境掃描 1 029 組，主張的範圍內（每軸均勻雜訊 ≤ 0.1 格距、缺件 ≤ 40%、離群點 ≤ 20 個、7 個角度）420 組全部完全正確；範圍外 609 組的失敗都是安全的。與 S00 示範頁的 JavaScript 交叉驗證 36 組全部一致。植入 32 個缺陷，最終全被抓到（第一輪 6 個存活：規則彼此備援，補了 `tst_lattice_rules`）。
- **效能（這台機器，Release，連跑 3 次的範圍；細節見 `docs/steps/S05-lattice/perf.md`）**：**T3（1 000 點）p95 0.65–1.19 ms，預算 2 ms，通過**。第一版 p95 3.3–3.6 ms 沒有達標，量出各段的時間後改寫熱迴圈才達標。約 4 900 個元件時，use case 的擬合階段由 S03 的 97–134 ms 降到 3.3–7.0 ms（中位數）。**預算數字沒有改。**
- **要留意的事**：（1）T3 的預算沒有說是哪一種 1 000 點：1 000 個散佈的點（沒有晶格）p95 3.1–4.2 ms、缺件加離群點 p95 1.6–2.1 ms，不在預算裡，要不要算進 T3 是使用者對預算的決定（S10 定義 harness 時）。（2）門檻（容差 0.25 格距、信心 0.15／0.6、證據、列一致性 0.6）是合成資料定的起點，S07 用真實影像校準。（3）**沒有測**：非矩形的陣列（L 形、環形）。（4）信心與「建議檢查」在 use case 的結果（`GridSummary`）裡，畫面還沒顯示。（5）缺件 ≥ 50% 時整組二維晶格不成立，常退回其中幾排的一維晶格：這是刻意的取捨（ADR-013）。（6）視窗驗證仍是一次批次；多回合的漸進驗證等 S07 量到真實 back end 每次呼叫的成本再決定。
- **新增設計決定**：ADR-013（晶格的接受規則）。`docs/plan/S06`、`S07`、`S10`、`S11` 開頭各加了一段「S05 完成後的提醒」。

## 預算校準狀態

`perf/budgets.json` 頂層仍是 `status: "proposed"`（T 類搜尋預算要等 S07）。**S05 起 T3 標 `calibrated`**（p95 0.65–1.19 ms，附 3 次的範圍與輸入，`searchCalibrationS05`；limit 沒改）。UI 類：U1a、U1b、U4、I2、I5 標 `calibrated`，I1 標 `calibrated-at-limit`，每列附實測 p95；S04 起 U2、I3、I4、I6 也標 `calibrated`（附 3 次執行的 p95 範圍，`uiCalibrationS04`），**U3 標 `measured-over-limit`**；**沒有改任何 limit**（使用者在 S01 確認過）。S03 沒有新增或改變任何預算：T 類搜尋預算要真實 back end（S05、S07、S08），假 back end 的數字不能校準它們；S03 的量測只記在 `docs/steps/S03-core/perf.md` 與 `perf/baseline/casparhsu1-core.json`。基準線：`perf/baseline/casparhsu1.json`（S02）、`casparhsu1-core.json`（S03）、`casparhsu1-presenter.json`（S04）、`casparhsu1-lattice.json`（S05；含 S05 重量的 `perf_core` P4，`casparhsu1-core.json` 的 P4 是 S03 暫代版的數字）（只在這台機器可比較）。

## 未解決事項

- ADR-006 未決：實例覆寫（只改這一個元件）、Panel 多連板、角度正負號、窗的演算法與參數、自由公式、庫的 System／Project 層。新預算 T7（區域掃描隨樣本庫大小 N）等 S07 實測後再提。
- 原型的比對是假的；真實的 `ILibraryMatcher` 在 S07／S08。HALCON「每個條目一個 shape model」是預期做法，未驗證。
- ~~MSVC 2022 工具鏈是否可用~~ — S02 已驗證：VS 2022 17.14、`cl` 19.44、Qt 6.8.0、CMake 3.30.5、Ninja 1.12.1 全部可用（細節在 `AI_CONTEXT.md` 環境表）。**HALCON 只有 runtime，沒有 `include`／`lib`（找不到 `HalconCpp.h`）：S08 前要補裝開發元件**；OpenCV 4.9.0 的 `find_package` 找得到（`OpenCV_DIR` 要指到 `build\x64\vc16\lib`），但沒有連結過
- **等使用者決定（ADR-009）**：I1 在預算邊緣、真實視窗超標——維持 Widgets 並在 S11 試 OpenGL 視埠？I1 的預算要不要註明視埠大小與平台？使用者平常的螢幕解析度與縮放習慣是什麼？
- **等使用者決定（U3）**：分數滑桿到重繪超出預算（5 832 個元件 p95 46–61 ms，預算 16 ms），降低成本（S11）還是重新定義這個預算？預算數字沒有改。
- 設定記憶（主題、字級、語言）、減少動態、高 DPI 螢幕上的實際外觀：**沒有做**；高 DPI 只驗證了粗細的換算。拖曳移動元件與方向鍵微調已在 S04 完成。
- **S04 沒有涵蓋的 UI**：ux-spec 12.4 列的 `LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）**沒有任何一步的 plan 檔涵蓋**，需要使用者決定放進哪一步。流程 B（區域掃描）的 Presenter 路徑已接好，但結果固定是「樣本庫是空的」（S06、S07 才有庫與比對器）。接受提案時產生的元件類型是暫時的，名稱 `X1…`、沒有料號；沒有校正值的影像暫以 1 µm ＝ 1 像素展開框（暫定，不是事實）；文件只在記憶體裡，沒有存檔；執行中不能切換 back end（下拉選單停用）。
- S02 的暫代品：`ShellController` 與 `copy.json` 的 `ui.shell.*` key 已在 S04 移除；`ImageLoader` 與 `ImageViewAdapter`（Qt 影像 → `ImageView`）仍是暫代，S06 的 `IImageSource` 取代；`ImageView` 的所有權規則 S06 定案
- S03 的暫代品：~~`SimpleGrid`~~（S05 已取代並刪除）、假 back end 不看影像（S07 才有真的搜尋）。
- **S05 留下的問題**：T3 要不要涵蓋「沒有晶格的散佈點」（p95 3.1–4.2 ms）是對預算的決定（見上方 S05 一節）；晶格的門檻要用真實影像校準（S07）；非矩形陣列沒有測；信心與「建議檢查」還沒有顯示在畫面上（沒有任何一步的 plan 檔寫到，需要決定放進哪一步）；多回合的漸進驗證等 S07 的數字。流程 B（區域掃描）只有 port（`ILibraryMatcher`、`ILibraryStore`）與型別，沒有 use case、沒有假實作。
- `AiInspTool.pro` 已納入 application、`infrastructure/{backend_fake,perf,threading}` 與組裝根（S04 新增的資料夾在萬用字元底下，不必改）。S04 重新驗證了命令列 qmake ＋ jom（Release）建置，執行檔可以啟動；**沒有在 Qt Creator 裡實際載入過**；storage、OpenCV、HALCON 還沒有（S06–S08）。
- ADR-010 的 OpenCV／HALCON 轉換未驗證（見上）；`QBENCHMARK` 沒有用到也沒有驗證（ADR-003）。
- 真實板子影像（去識別化 1–2 張）— S07 前請使用者提供
- AI 共用結構（Codex 與 Claude Code）已於 2026-10-05 套用，commit `fd4fd2a` 並以 `--no-ff` 合併到 `main`，**沒有 push**。S01 的 Start prompt 已改成引用 `AGENTS.md` 與 `docs/current-status.md`，這些檔案都在 `main` 上。變更含：新增 `AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/`、`scripts/ai/`、`docs/adr/000-*`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`；改寫 `CLAUDE.md`；`docs/STATUS.md` 更名為本檔；14 個檔案的引用更新。stage 時一律明確列出路徑，不要 `git add -A`。
- `ai-project-template/`：AI 結構範本，與產品程式碼無關。2026-10-05 由 Claude Code 在這個 session，依使用者「設計共用結構」的要求產生，並依使用者決定納入版控、放在這個 repo。若日後要搬離，同步更新 `AI_CONTEXT.md` 等引用它的文件。
- Codex 端的 AGENTS.md 流程沒有在本機端到端測過（該機器的 Codex CLI 無法執行 `codex exec`）。
