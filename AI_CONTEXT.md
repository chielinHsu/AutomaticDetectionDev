# AutoCadCreated — AI Context

> 只寫「這個專案的事實」。工作流程在 `.agents/harness/engineering-workflow.md`，規則在 `.agents/harness/project-rules.md`，不要在此複製。
> 本檔的內容原本在 `CLAUDE.md`，2026-10-05 搬進共用結構（見 `docs/adr/000-ai-agent-structure.md`）。

## 專案用途

AOI 教學工具：使用者在影像上框一個樣本 ROI → 程式找出相似元件 → 推算陣列間距並補漏 → 產生檢測框（"CAD objects"）。
**OpenCV 與 HALCON 兩個 back end 共用同一份程式碼**，執行時可切換。

2026-10-05（S01）起有第二個流程：使用者框**任意區域** → 程式與**樣本庫**所有樣本比對 → 列出對應的 CAD 框。資料模型是 Board › Component › Box，框由「元件類型」推導，見 `docs/adr/006-sample-library-and-hierarchy.md`。

- 完整計畫：`docs/00-master-plan.md`
- 目前進度：`docs/current-status.md`（開工前先讀）
- 每一步的細節與交接 prompt：`docs/plan/Sxx-*.md`
- 效能測試計畫：`docs/perf-test-plan.md`；預算：`perf/budgets.json`

## 技術棧

| 層 | 技術 | 版本／備註 |
|---|---|---|
| 語言 | C++20 | `domain` 層只用標準庫，見 `docs/architecture.md` |
| UI | Qt Widgets ＋ QGraphicsView | ADR-001（已採用）；S02 起有程式 |
| 建置 | CMake ＋ Ninja | `CMakeLists.txt`、`CMakePresets.json`（debug／release／relwithdebinfo）；自家程式碼 `/W4 /WX` |
| 視覺 | OpenCV、HALCON | 只能在對應的 `infrastructure/backend_*` 使用 |
| 測試 | Qt Test | [ADR-003](docs/adr/003-test-framework.md)（採用），以 CTest 執行 |
| 工具腳本 | Node.js | `tools/*.mjs` |
| 平台 | Windows 11、MSVC 2022 | 編譯器尚未驗證，見下表 |

## 環境（Windows 11，本機實際探測結果；S02 於 2026-10-05 實際建置與執行驗證）

| 項目 | 位置與版本 | 驗證 |
|---|---|---|
| Visual Studio 2022 Community 17.14.35 | `D:\VS2022`（用 `vswhere` 找，不寫死） | `cl` 19.44.35228（toolset 14.44.35207）。同一個 VS 還有 14.16、14.29、14.32 三個舊 toolset，**沒有使用** |
| Qt 6.8.0 MSVC2022 64-bit | `D:\Qt\6.8.0\msvc2022_64` | 有 Widgets、Test、Gui、Core、Concurrent、Svg、LinguistTools 與 Debug 版程式庫；實際 configure、build、執行過 |
| CMake / Ninja | `D:\Qt\Tools\CMake_64\bin`（3.30.5）/ `D:\Qt\Tools\Ninja`（1.12.1） | 實際使用 |
| OpenCV 4.9.0 (vc16) | `D:\opencv\opencv\build`；**`OpenCV_DIR` 要指到 `D:\opencv\opencv\build\x64\vc16\lib`**（preset 已設）。指到 `build` 根目錄會失敗：根目錄的 `OpenCVConfig.cmake`（Windows Pack）找不到相容 MSVC 19.44 的二進位檔 | S02 實際跑過 `-DAOI_WITH_OPENCV=ON` 的 configure：找到 4.9.0。**沒有編進任何程式、沒有連結過**（vc16 的程式庫配 MSVC 2022 的 vc17 toolset 能不能連結，S07 才知道） |
| HALCON 24.11 Progress-Steady | `%HALCONROOT%` = `C:\Program Files\MVTec\HALCON-24.11-Progress-Steady` | **只有 runtime**：有 `bin\x64-win64\halconcpp.dll` 與 `license\`，但**沒有 `include\` 與 `lib\`**（找不到 `HalconCpp.h` 與任何 `.lib`）。要在 S08 用 C++ 編譯，要先補裝開發元件；`-DAOI_WITH_HALCON=ON` 現在會在 configure 時明確失敗 |

- **進入開發環境**：每個 PowerShell session 先 dot-source `. .\scripts\dev\env.ps1`（用 `vswhere` 找 VS、跑 `vcvars64.bat`、把 Qt／CMake／Ninja 放進 PATH）。路徑可用環境變數 `AOI_QT_ROOT`、`AOI_QT_TOOLS` 覆寫。Claude Code 的 PowerShell 工具**每次呼叫都是新的 shell**，所以每次呼叫都要重新 dot-source。
- **建置要在 `env.ps1` 建立的環境裡做（configure 與 build 同一個）**：這台是繁中 Windows，`cl.exe` 的 `/showIncludes` 輸出是中文，而且這個 VS 只裝了 1028 繁中資源（`VSLANG=1033` 無效）。Ninja 靠這行中文前綴記錄標頭相依；前綴編碼取決於主控台字碼頁，configure 與 build 的字碼頁不同時（實測：從 Git Bash 啟動 `powershell.exe` 就會），相依記錄掉了，**只改標頭檔不會重新編譯**，輸出還會被亂碼洪水淹沒。`env.ps1` 會把字碼頁對齊系統 OEM 字碼頁。症狀出現時（亂碼洪水、改標頭沒反應）：刪掉 `build/<preset>` 重新 configure。
- **路徑長度**：Windows 預設路徑上限 260 字元。建置目錄放在 repo 內的 `build/<preset>`（被 gitignore）；放到很深的暫存路徑（例如 Claude 的 scratchpad）時，MSVC 會因 PDB 路徑過長失敗（C1041）。
- **Qt Test 在這台機器不把結果寫到 stdout**（重新導向後是空的）；要用 `-o 檔案,txt`。`CMakeLists` 已經把 log 放在 `build/<preset>/test-logs/<name>.txt`，`scripts/dev/test.ps1` 失敗時會印出來。
- offscreen 平台不會自己找到字型：要設 `QT_QPA_FONTDIR=C:/Windows/Fonts`（CTest 與 test preset 已設）。
- Qt 6.8 的 `LanguageChange` 是排入事件佇列的，語言切換後要等下一輪事件迴圈（測試用 `QCoreApplication::processEvents()`）。
- Shell 是 Windows PowerShell 5.1：沒有 `&&`；commit 訊息寫成檔案再 `git commit -F <檔案>`，不要用 `-F -`（格式見 `project-rules.md`「Commit 訊息」）。
- Node 可用（v22），工具腳本放 `tools/*.mjs`（`check-deps.mjs` 也由 CTest 呼叫，所以 `node` 要在 PATH 上）。`python.exe` 只是 Store 捷徑，不要依賴。
- 以上路徑若有差異，**更新此表**。

## 重要目錄

| 路徑 | 內容 |
|---|---|
| `src/` | 程式碼，依五層分（`domain`、`application`、`infrastructure`、`presentation`、`app`）。`domain`：只有標頭（座標與角度慣例在 `geometry.h`，框的展開在 `box_expander.h`）；`application`：`Error`／`Result`／`CancelToken`、`ports/`、`jobs/JobRunner`、`usecases/`、`backend/BackendRegistry`，S04 起有 `document/`（`UndoStack`、`ProposalSession`、`InspectionDocument`）與 `interaction/`（`InteractionStateMachine`、`InspectionSession`：所有互動規則，沒有 Qt）；`infrastructure`：`backend_fake/`、`perf/`、`threading/`（含 S04 的 `QtThreadPoolExecutor`）（`backend_opencv`、`backend_halcon`、`storage` 在 S06–S08）；`presentation`（`canvas/`、`tools/`、`shell/`、`theme/`、`i18n/`、`dev/`；S04 起接上 application：`presenters/`＝`AutoRoiPresenter`、`ParamsPresenter`、`SceneBuilder`、`AppPresenters`，`objects/`＝物件樹，`params/`＝參數面板）；`app`：`CompositionRoot`（組裝）與 `main.cpp`。檔案命名見 `docs/architecture.md` |
| `tests/ui/` | Qt Test（S02、S04）：`tst_*` 是單元測試與整合測試（`tst_presenter` 接上真的核心、假 back end 與視窗；`PresenterRig.h` 是它的測試台），`perf_ui_baseline`、`perf_paint_micro`、`perf_ui_s04` 是效能量測（label `perf`） |
| `tests/core/` | Qt Test（S03、S04）：核心的單元測試（`tst_domain`、`tst_core_types`、`tst_grid`、`tst_job_runner`、`tst_generate_objects`、`tst_fake_backend`、`tst_backend_registry`、`tst_perf_probe`；S04 新增 `tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`、`tst_qt_executor`）、`tst_cli`（從外面用真的 `aoi` 行程驗證 `--backend`）、`perf_core`（label `perf`）、`fixtures/deps/`（依賴檢查的 13 個 fixture）；`check_deps*` 的 CTest 項目也在這裡註冊 |
| `AiInspTool/` | 使用者用 Qt Creator 開啟的 **qmake 專案**（`AiInspTool.pro`＋`aoi_ux.qrc`）：直接建置 `../src` 的 domain、presentation 與 `app/main.cpp`（程式碼只有一份，用萬用字元納入）。精靈產生的 `main.cpp`、`mainwindow.*` 沒有被使用。**不在五層之內，`tools/check-deps.mjs`（S03）不要掃它**；單元測試只在 CMake。S03 起 `src/` 多出 application、infrastructure（OpenCV、HALCON）時，這個 `.pro` 要跟著調整，否則 Qt Creator 建置會壞（正式路徑是 CMake）。S03 已把 application、`infrastructure/{backend_fake,perf,threading}` 與組裝根納入（萬用字元遞迴納入，S04 新增的 `application/{document,interaction}`、`threading/QtThreadPoolExecutor`、`presentation/{presenters,objects,params}` 不必改 `.pro`）；S06 的 storage、S07／S08 的 OpenCV／HALCON 還沒有 |
| `cmake/`、`CMakeLists.txt`、`CMakePresets.json` | 建置設定；`cmake/AoiTargets.cmake` 放警告設定與 `aoi_add_qt_test` |
| `scripts/dev/` | `env.ps1`（進入開發環境）、`test.ps1`（建置＋跑測試，失敗時印 log） |
| `perf/baseline/` | 各機器的效能基準線（入版控）；原始量測在 `perf/results/`（不入版控） |
| `docs/` | 計畫、ADR、AI 交接文件 |
| `docs/plan/` | S01–S12 每一步的計畫與 Start prompt |
| `docs/steps/` | 每一步的四種輸出；範本在 `_template/` |
| `docs/adr/` | 設計決策，一檔一決策（`NNN-slug.md`） |
| `perf/` | 效能預算；實測結果在 `perf/results/`（不入版控） |
| `docs/ux/` | UX 規格（`ux-spec.md`）、狀態語言 token（`tokens.json`）、文案（`copy.json`）；S02 的 QPalette／QSS 與 i18n 以此為來源 |
| `tools/` | Node 工具：`ste-lint.mjs`、`check-contrast.mjs`；`check-deps.mjs` 在 S03 建立 |
| `ai-project-template/` | 與本產品無關的 AI 結構範本（Codex 與 Claude Code 共用）：`New-AiProject.ps1` 把結構套到新資料夾，設計說明見其 `README.md`。已納入版控；除非使用者要求，不要修改 |
| `.agents/`、`.claude/`、`.ai/`、`scripts/ai/` | AI agent 的規範、skill、交接單與檢查腳本，各檔職責見 `.agents/harness/engineering-workflow.md` 第 0 節 |

## 建置與測試

驗證指令是 agent 判斷「做完了沒有」的唯一依據。尚未建立的檢查，回報時標明「不適用（尚未建立）」，不要寫成通過。

| 目的 | 指令 | 備註 |
|---|---|---|
| 建置 | `. .\scripts\dev\env.ps1` 後 `cmake --preset release` 與 `cmake --build build/release`（`debug`、`relwithdebinfo` 同理） | S02 起可用。選項 `AOI_WITH_OPENCV`、`AOI_WITH_HALCON`（預設 OFF）、`AOI_BUILD_TESTS`、`AOI_DEMO_SCENE` |
| 單元測試 | `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1`（含建置；`-Preset debug` 可換組態） | S02 起 5 個 UI 的 Qt Test 執行檔；S03 起再加 8 個核心的測試執行檔、`tst_cli`（命令列）與 3 個 `check_deps*` 項目；S04 起再加 7 個核心（`tst_undo_stack`…`tst_qt_executor`）與 5 個 UI（`tst_objects_model`、`tst_params`、`tst_messages`、`tst_scene`、`tst_presenter`），共 29 個項目；經 CTest 在 offscreen 平台執行；契約測試 S09 起 |
| 效能基準線 | `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1 -Perf` | S02 的 UI 基準線（U1、U4、I1、I2、I5）、S03 的核心量測（`perf_core`：PerfProbe 開關成本、工作延遲、取消延遲、use case 自己的成本）與 S04 的互動量測（`perf_ui_s04`：U2 物件表填入 5 000 列、U3 分數滑桿到重繪、I3 搜尋期間事件迴圈最長阻塞、I4 按取消到 UI 閒置、I6 復原 1 000 框的接受；`perf_paint_micro` 是繪製的微基準）；Release 才有意義；結果見 `docs/steps/S02-ui-shell/perf.md`、`docs/steps/S03-core/perf.md`、`docs/steps/S04-presenter/perf.md`，基準線在 `perf/baseline/` |
| 執行程式 | 進入開發環境後 `build\release\aoi.exe`；`--demo 5000` 開合成板子與框，`--lang zh_TW\|en`、`--theme light\|dark\|system`、`--backend fake\|opencv\|halcon`（預設 `fake`；沒編進程式的 back end 以結束碼 2 明確失敗，訊息寫到 stderr） | |
| 依賴方向檢查 | `node tools/check-deps.mjs` | S03 起可用；掃 `src/` 的 `#include`，違規即失敗（結束碼 1）。`--self-test` 對 13 個 fixture 驗證它真的抓得到；`--root <dir>` 掃別的樹。CTest 的 `check_deps`、`check_deps_self_test`、`check_deps_catches_a_violation`（找不到 Node 時不註冊，configure 會警告） |
| 文件風格檢查 | `node tools/ste-lint.mjs` | 現在可用；只檢查 `<!--STE-->` 標記之間的文字 |
| 顏色對比檢查 | `node tools/check-contrast.mjs` | S01 起可用；驗算 `docs/ux/tokens.json`（文字 ≥ 4.5、圖形 ≥ 3）並檢查四種狀態線型不重複；改過顏色後加 `--write` 更新結果 |
| 效能 | S10 的 harness | 預算在 `perf/budgets.json` |
| AI 結構檢查 | `powershell -ExecutionPolicy Bypass -File scripts/ai/check-ai-structure.ps1` | 改過入口檔或 skill 後執行 |

`VERIFIED` 的定義（取自 Step Protocol 第 3 步）：建置通過、單元與契約測試通過、`check-deps` 與 `ste-lint` 皆無 error，且本步驟的效能指標已量測並與 `perf/budgets.json` 比對。失敗就如實回報，不要宣稱完成。

## 限制與不變條件

- 分層與依賴方向見 `docs/architecture.md`，違規由 `check-deps` 擋下。
- 不要在 `domain` 或 `application` include Qt、`cv::`、`HalconCpp`（`check-deps` 會擋）。
- port 必須可重入（latest-wins 時新舊工作會重疊）、不丟例外、取消是協作式的；契約寫在各 port 的標頭，S09 變成契約測試。
- 座標與角度慣例只有一種（ADR-010，`src/domain/geometry.h`）：adapter 自己轉換，不要在 domain 或 use case 裡補丁。
- 不要在 `JobRunner` 的 `work`／`done` 裡解構那個 `JobRunner`（解構會等所有工作結束，等到自己）。
- 不要讓 `cv::Mat`、`HalconCpp::HImage`、`QImage` 跨過 port（用 `ImageView`）。
- 不要沒量測就做效能優化（S11 規則：先有 before 數字）。
- 不要把效能預算數字當事實：`perf/budgets.json` 內為「提議值」，要在 S02 與 S07 用本機實測校準後才轉為正式。
- 真實產線影像（`/data/`）、`build/`、`perf/results/` 不入版控。

## Context Routing（任務 → 先讀什麼）

只載入與任務相關的文件，不要一次讀完。

| 任務 | 必讀文件／skill | 注意 |
|---|---|---|
| 開始任一步驟 | `docs/current-status.md`、`docs/plan/Sxx-*.md`、`project-rules.md` 的 Step Protocol | 一個 session 只做一步 |
| 改分層、新增模組 | `docs/architecture.md`、`docs/decisions.md`（索引，連到 `docs/adr/`） | 已記錄的決定沒有新證據不推翻 |
| 寫步驟文件 | `docs/steps/_template/`、`project-rules.md` 的「每一步的四種輸出」 | 跑 `node tools/ste-lint.mjs` |
| 效能相關 | `docs/perf-test-plan.md`、`perf/budgets.json` | 沒有 before 數字不動手 |
| 改 port、use case 或新增 back end | `docs/steps/S03-core/spec.md`、port 標頭裡的契約、ADR-010、`docs/architecture.md` | 新 back end ＝ adapter ＋ 在 `CompositionRoot.cpp` 註冊；不要改 `application/`、`domain/` |
| 接續別人的工作 | `.ai/handoff.md`、`docs/changelog-ai.md` | 先看 `git status`，別覆蓋未完成的修改 |
| 收工、換 agent | `ai-handoff` skill | |

## 詞彙表

| 詞 | 意義 |
|---|---|
| ROI | 使用者在影像上框選的區域（教樣本時是一個元件；區域掃描時可含很多元件） |
| Board | 一塊板子：影像＋所有元件 |
| Component（元件） | 板上的一個零件實例，一筆 CAD 記錄（名稱、X、Y、角度、類型）；框不屬於它，而是從它的類型推導 |
| ComponentType（元件類型） | 樣本庫的一個條目（料號、別名、框群組）；改它，同類型的所有元件一起更新 |
| BoxGroup（框群組） | Body（每類型一個）、Pin（矩陣）、Window（連結到父群組，大小用父框的比例） |
| CAD objects／Box（框） | 程式產生的檢測矩形 |
| back end | 視覺實作：OpenCV 或 HALCON，執行時可切換 |
| port | application 層定義的介面，由 infrastructure 的 adapter 實作 |
| `ImageView` | 跨過 port 傳遞影像的型別，不暴露任何函式庫的影像類別 |
| Step Protocol | 每完成一步必做的流程，見 `project-rules.md` |
| Candidate／classify | use case 一次回傳所有候選與分數（≥ 0.30）；`classify(proposal, threshold)` 依門檻分成 Sample／Found／Inferred／Rejected，拖門檻不必重新搜尋 |
| latest-wins | `JobRunner`：新工作提交時舊工作的取消旗標立刻升起，不等它停下；過期結果靠工作編號與 `JobReport::latest` 丟棄 |
| 假 back end（fake） | `infrastructure/backend_fake`：以樣本為中心排可重現的合成網格，不看影像；`--backend=fake`，S07 之前讓整個流程有東西可跑 |
