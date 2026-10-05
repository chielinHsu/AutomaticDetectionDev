# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Sonnet 5.5)
- 日期：2026-10-06

## 正在做

無。**S04（Presenter、互動狀態機、復原／重做）已完成**：分支 `step/04-presenter`，以 `--no-ff` 合併到 `main`，tag `step-04-done`。全部只在本機，**沒有 push**。S05 由新的 session 做。
**有兩件事等使用者決定**：（1）[ADR-009](../docs/adr/009-ui-toolkit-review-after-s02.md)（I1 在預算邊緣，要不要重審 UI 工具集）；（2）**U3（分數滑桿到重繪）超出預算**：5 832 個元件 p95 46–61 ms，預算 16 ms。預算數字沒有改，測試用 `knownOver("U3")` 把它列為已知超標（不是放寬）；降低成本（S11）或重新定義預算，由使用者決定。

## 已完成（S04）

- application：`InteractionStateMachine`（8 狀態、19 事件）、`InspectionSession`（所有互動規則，沒有 Qt，ADR-011）、`ProposalSession`、`InspectionDocument`（5 種 Command）、`UndoStack`；domain：`box_expander.h`；infrastructure：`QtThreadPoolExecutor`。
- presentation：`AutoRoiPresenter`、`ParamsPresenter`、`SceneBuilder`、`ObjectsModel`／`ObjectsPanel`、`ParamsPanel`、`MessageTexts`、`AppPresenters`（main 與測試共用的接線）；`MainWindow` 變純 View；拖曳移動、方向鍵微調；`ShellController` 移除。
- 改了 S02 的繪製：虛線與點線也畫成填色矩形（ADR-012；點線的重繪成本差 12 倍）。
- 測試：Release 29 個 CTest 項目全過；`tst_presenter` 是接上真核心與視窗的整合測試（`tests/ui/PresenterRig.h`）；41 個變異測試最終全被抓到。
- 文件：`docs/steps/S04-presenter/`（四種輸出＋`perf.md`）、ADR-011、ADR-012、`perf/baseline/casparhsu1-presenter.json`；`docs/plan/S05`–`S07`、`S10`、`S11` 開頭有「S04 完成後的提醒」。

## 下一步

**S05：Domain 晶格估計與補漏**（`docs/plan/S05-lattice.md`，開頭有 S03、S04 的提醒）。取代 `SimpleGrid`（`fitGrid`、`gridHoles`）；Presenter 只透過 use case 用它，不必改 `presentation/`。框層級的矩陣已經在 `box_expander.h`，晶格估計只管元件層級。

## 不要動

- `ai-project-template/`：與產品無關的 AI 結構範本；除非使用者要求，不要修改。
- `docs/ux/tokens.json`、`copy.json`：畫面唯一的來源；改了要重建並跑 `check-contrast`、`tst_theme_i18n`。
- `perf/budgets.json` 的 limit 一個都沒改；不要為了讓測試過而放寬。`tests/ui/perf_ui_s04.cpp` 的 `knownOver("U3")` 要等 S11 處理 U3 後才拿掉。
- `tests/ui/PresenterRig.h` 的視窗閘門（`gateWindows`）：閘門後面故意不看取消旗標，用來模擬不可中斷的函式庫呼叫；改成看旗標，`aLateSuccessfulResultOfAReplacedSearchIsDropped` 就抓不到遲到的成功結果（M18 就是這樣存活過）。
- port 的契約與 `ImageView`、`SearchParams`（S03 定案）；規則表 `tools/check-deps.mjs` 的 `RULES` 與 `docs/architecture.md` 的依賴表要一起改。

## 已知問題與風險

- **U3 超標**（見上）；I3 餘裕只有 16–38%，而同一份程式連跑會差 30%：S10 的閘門不能拿單次 p95。只量了 offscreen，沒量真實視窗（S02 經驗：慢 1.5–2.5 倍）。
- **沒有人負責的 UI**：ux-spec 12.4 的 `LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）沒有任何 plan 檔涵蓋，需要使用者決定放進哪一步。流程 B 的結果固定是「樣本庫是空的」（S06、S07 才有庫與比對器）。
- 暫時的做法：接受提案產生的元件類型叫 `X1…`、沒有料號；沒有校正值的影像暫以 1 µm ＝ 1 像素展開框（暫定，不是事實）；文件只在記憶體裡；執行中不能切換 back end。
- 大檔案：`AutoRoiPresenter.cpp` 707 行、`InspectionDocument.cpp` 387 行（超過 300 行，spec 有說明，沒有拆）。
- **`SimpleGrid` 仍是暫代品**（4 900 點約 97–134 ms）；**ADR-010 的 OpenCV／HALCON 轉換仍未驗證**（S07、S08）。
- **`AiInspTool.pro`**：S04 新增的資料夾都在已納入的萬用字元底下，只改了註解。命令列 qmake ＋ jom（Release）建置成功，`--backend opencv` 結束碼 2、`--demo 500` 啟動 4 秒仍在執行；**沒有在 Qt Creator 裡載入過**；storage、OpenCV、HALCON 之後要再調整。未追蹤的 `AiInspTool/release/`、`.qtc_clangd/` 是 Qt Creator 的建置產物（在使用者的主工作目錄，我沒有動）。
- **變異測試腳本沒有留在 repo**（在 session 的暫存目錄）：還原用 `WriteAllBytes`（會更新修改時間，Ninja 才會重建）；用 `Copy-Item` 還原會保留舊的修改時間，Ninja 以為沒變，會留下變異過的目標檔。每次還原後要驗證雜湊。
- **建置環境（繁中 Windows）**：一定要在 `env.ps1` 之後 configure 與 build；Bash heredoc 與 `node -e` 會吃掉反斜線與引號，含中文或引號的檔案內容用 Write／Edit 工具寫；PowerShell 5.1 的 `.ps1` 不要放中文字串。Qt Test 要用 `-o 檔案`；`QVERIFY`／`QCOMPARE` 不要放在 worker 的 callback 裡。
- Codex 端的 AGENTS.md 流程沒有在本機實測過。

## 驗證（都已實際執行）

- `scripts/dev/test.ps1`：Release 29 個 CTest 項目全過；Debug 也是 29 個全過（第一次有 1 個失敗：`tst_proposal_session` 的測試把指標指進暫存的 `objects()`，Release 碰巧通過；已改成指向活著的物件。寫測試時不要保存指向暫存物件內部的指標）。
- `node tools/check-deps.mjs`：112 個檔案 0 違規（`--self-test` 13 個 fixture 全過）；`ste-lint --strict`：549 句 0 錯誤；`check-contrast`：0 失敗；`diagram.md` 9 張 Mermaid 解析全過（只驗語法）；`check-ai-structure.ps1`：OK（0 警告）。
- 效能：`perf_ui_s04` 連跑 3 次，U2、I3、I4、I6 在預算內，U3 超標（`docs/steps/S04-presenter/perf.md`）；`explainer.html` 在瀏覽器窗格實際操作過（狀態機、復原堆疊的接受／復原／重做流程、數字卡、窄寬度）；深色外觀沒有驗證（窗格對本機文件固定淺色）。
