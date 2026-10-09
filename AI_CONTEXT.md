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
| 建置 | qmake ＋ jom | `AiInspTool/AiInspTool.pro`（2026-10-07 起只有這一套；之前是 CMake＋Ninja，見 [ADR-018](docs/adr/018-qmake-only-build.md)）；Qt Creator 開它就能建置、執行、除錯程式與所有測試；Debug、Release、Profile；自家程式碼 `/W4 /WX` |
| 視覺 | OpenCV、HALCON | 只能在對應的 `infrastructure/backend_*` 使用；兩個都**預設編進去** |
| 測試 | Qt Test | [ADR-003](docs/adr/003-test-framework.md)（採用）；以 `scripts/dev/test.ps1` 或 Qt Creator 的測試面板執行（ADR-018；之前是 CTest） |
| 工具腳本 | Node.js | `tools/*.mjs` |
| 平台 | Windows 11、MSVC 2022 | 編譯器尚未驗證，見下表 |

## 環境（Windows 11，本機實際探測結果；S02 於 2026-10-05 實際建置與執行驗證；S06 於 2026-10-06 改用 Qt 6.12.0）

| 項目 | 位置與版本 | 驗證 |
|---|---|---|
| Visual Studio 2022 Community 17.14（2026-10-07 更新到 17.14.37710） | `D:\VS2022`（用 `vswhere` 找，不寫死） | `cl` 19.44.35229（toolset 14.44.35207；更新前是 19.44.35228）。同一個 VS 還有 14.16、14.29、14.32 三個舊 toolset，**沒有使用**。VS 安裝程式在更新時 `vswhere` 找不到 VS（`env.ps1` 會失敗），等更新完成再建置 |
| Qt 6.12.0 MSVC2022 64-bit（S06 起；使用者 2026-10-06 更新） | `D:\Qt\6.12.0\msvc2022_64`（`env.ps1` 的預設；Qt Creator 的 kit「Desktop Qt 6.12.0 MSVC2022 64bit」） | 有 Widgets、Test、Gui、Core、Concurrent、Svg 與 Debug 版程式庫；S06 實際 configure、build、四種組態的測試、效能量測都跑過。影像外掛和 6.8.0 一樣只有 gif、ico、jpeg、svg（**沒有 TIFF**）。6.12.0 棄用了 `QDragEnterEvent` 吃 `QPoint` 的建構子（`/WX` 會擋），用 `QPointF` 的 |
| Qt 6.8.0 MSVC2022 64-bit | `D:\Qt\6.8.0\msvc2022_64`（仍在機器上） | S02–S05 用它建置與量測（那幾步的基準線是 6.8.0 的數字）。要改回它：`AOI_QT_ROOT` 設成這個路徑（`env.ps1` 用它的 qmake），建置目錄要刪掉重建；Qt Creator 換 kit |
| qmake / jom / Qt Creator | `D:\Qt\6.12.0\msvc2022_64\bin\qmake.exe` / `D:\Qt\Tools\QtCreator\bin\jom\jom.exe`（1.1.7）/ `D:\Qt\Tools\QtCreator`（20.0.2） | 2026-10-07 起實際使用（ADR-018）。CMake（`D:\Qt\Tools\CMake_64`）與 Ninja 還在機器上，本專案不再使用 |
| OpenCV 4.9.0 (vc16) | `D:\opencv\opencv\build`（qmake 變數或環境變數 `AOI_OPENCV_DIR` 可以換；`AiInspTool/qmake/aoi_opencv.pri` 在底下找 `include/` 與 `x64/vc*/lib/opencv_world*.lib`）。只有 `opencv_world490`（Release）／`opencv_world490d`（Debug）一個 DLL；有 libtiff、libpng、libjpeg、OpenEXR（EXR 要設 `OPENCV_IO_ENABLE_OPENEXR` 才開放，沒有用） | **S06 起編進程式、連結與執行都成功**（vc16 的程式庫配 VS 2022 的 MSVC 19.44）；**預設編進去**（`AOI_WITH_OPENCV=OFF` 才關），DLL 建置時複製到 `<建置>/bin`。實測：`imdecode` 讀 12 bit 緊密打包的 TIFF 會給 16 bit、值左移 4 位。**S07 起是可用的 back end**（`--backend opencv`；`cv::setNumThreads` ＝ 核心數 − 1）。使用者的影像是 12 bit、存成 16 bit 容器（值 0..4095） |
| HALCON 24.11.2 Progress-Steady | runtime：`%HALCONROOT%` = `C:\Program Files\MVTec\HALCON-24.11-Progress-Steady`（`bin\x64-win64` 在 PATH 上）；開發檔：repo 的 `third_party/halcon/`（S08 從 CV_Algorithm 專案複製，見那裡的 `README.md`） | **S08 起編進程式、連結與執行都成功**；**預設編進去**（`AOI_WITH_HALCON=OFF` 才關；`halconcpp.dll` 延遲載入）。`%HALCONROOT%` 本身仍沒有 `include\`、`lib\`。**授權**：`license.dat` 是 runtime（RTL，CodeMeter `3-7614034`，有插）、`license--DEVELOP-.dat` 是 SDK（`3-7081033`，**沒插**）；HALCON 用 runtime 授權，程式會提醒（ADR-017）。模組含 Matching。第一次呼叫 HALCON 約 1.6 秒（初始化＋授權）。授權檔寫 `SESSIONS=1`。HALCON 曾兩次拿不到授權（錯誤 2300；Windows 仍認得 `VID_064F` 的裝置）：2026-10-07 中午重插後恢復；2026-10-07 15:06 到 2026-10-08 13:20 重插、換埠、重新啟動都沒恢復，使用者在另一個專案拔插之後恢復，**原因不明**；2026-10-09 早上第三次（之後自己恢復）；**2026-10-09 12:39 起第四次（S09 途中），到 S09 收工都沒恢復**：使用者決定 dongle 相關的測試記下來、之後再測（`docs/steps/S09-contract-ab/perf.md` §6）。**`hhostid` 在這台與另一台電腦都不列出這個 dongle，不能拿來判斷授權**。2026-10-09 起程式碰 HALCON 之前先用另一個行程檢查授權（`AiInspTool.exe --probe-halcon-license`，內部用）：HALCON 在行程裡第一次初始化失敗後會記住失敗，這樣插上 dongle 再切一次就好、不必重新啟動；`SESSIONS=1` 不會讓初始化著 HALCON 的程式擋住別的程式（三個行程同時拿到授權，ADR-021 的後續）；判斷用 `test.ps1 -Filter 'tst_halcon_backend'`（log 的 Totals 0 skipped）。運算子說明：`%HALCONROOT%\help\operators_en_US.ref`（文字檔）或執行時 `get_operator_info` |
| HALCON 22.11.3 Steady（另一套，本專案沒有用） | `C:\Users\Caspar.Hsu\AppData\Local\Programs\MVTec\HALCON-22.11-Steady` | 使用者的其他工具；授權是過期的試用版（`VALID=2026-01-01`）。`HALCONROOT`（使用者環境變數）指 24.11，所以 22.11 的 HDevelop 會讀 24.11 的授權檔、報 2041 Invalid host。不要把 22.11 的 `bin` 放到本專案的 PATH 前面（`HalconRuntime` 會因版本不合判定不可用） |

- **進入開發環境**：每個 PowerShell session 先 dot-source `. .\scripts\dev\env.ps1`（用 `vswhere` 找 VS、跑 `vcvars64.bat`、把 Qt 的 `bin`（qmake）與 jom 放進 PATH）。路徑可用環境變數 `AOI_QT_ROOT`、`AOI_QT_TOOLS` 覆寫。Claude Code 的 PowerShell 工具**每次呼叫都是新的 shell**，所以每次呼叫都要重新 dot-source。`scripts/dev/test.ps1` 自己會 dot-source。
- **命令列建置 ＝ Qt Creator 的步驟**：在建置資料夾裡 `qmake <repo>\AiInspTool\AiInspTool.pro -spec win32-msvc [CONFIG+=debug] [AOI_WITH_HALCON=OFF …]` → `jom qmake_all` → `jom`（`test.ps1` 就是這樣做）。qmake 的 Makefile 自己掃 `#include` 記錄標頭相依，CMake＋Ninja 時代「configure 與 build 的字碼頁不同，改標頭不會重新編譯」的問題不再存在（`env.ps1` 仍把字碼頁對齊 OEM 字碼頁）。
- **PowerShell 腳本要存成 UTF-8 with BOM**（`scripts/**/*.ps1`）：Windows PowerShell 5.1 讀沒有 BOM 的檔案會當成 CP950，中文字串與註解會壞掉。
- **路徑長度**：Windows 預設路徑上限 260 字元。命令列的建置放在 repo 內的 `build/qmake-*`，Qt Creator 預設放在 `AiInspTool/build/<kit>-<組態>`（都被 gitignore）；放到很深的暫存路徑（例如 Claude 的 scratchpad）時，MSVC 會因 PDB 路徑過長失敗（C1041）。repo 的路徑不能有空白（ADR-018）。
- **Qt Test 的純文字輸出在這台機器不寫到被接走的 stdout**（重新導向後是空的；`-xml`、`-lightxml` 會寫，所以 Qt Creator 的測試面板看得到）；`test.ps1` 用 `-o 檔案,txt`，log 在 `build/qmake-*/test-logs/<name>.txt`，失敗時會印出來。
- offscreen 平台不會自己找到字型：要設 `QT_QPA_FONTDIR=C:/Windows/Fonts`。每個測試執行檔編進 `tests/support/test_environment.cpp`，在 `main` 之前設好 offscreen 與字型（已經有設定時不覆寫），所以 Qt Creator 直接執行測試也一樣。
- Qt 6.8 起（6.12.0 也一樣，測試照舊通過）的 `LanguageChange` 是排入事件佇列的，語言切換後要等下一輪事件迴圈（測試用 `QCoreApplication::processEvents()`）。
- Shell 是 Windows PowerShell 5.1：沒有 `&&`；commit 訊息寫成檔案再 `git commit -F <檔案>`，不要用 `-F -`（格式見 `project-rules.md`「Commit 訊息」）。
- Node 可用（v22），工具腳本放 `tools/*.mjs`（`check-deps.mjs` 也由 `test.ps1` 呼叫，所以 `node` 要在 PATH 上；找不到時那 4 項算失敗）。`python.exe` 只是 Store 捷徑，不要依賴。
- 以上路徑若有差異，**更新此表**。

## 重要目錄

| 路徑 | 內容 |
|---|---|
| `src/` | 程式碼，依五層分（`domain`、`application`、`infrastructure`、`presentation`、`app`）。`domain`：型別多半只有標頭（座標與角度慣例在 `geometry.h`，框的展開在 `box_expander.h`，影像的所有權規則在 `image_view.h`；S06 的 `image_buffer`、`image_copy`（複製計數）、`image_depth`（12 bit）有 .cpp），S05 起 `lattice/` 有 .cpp（晶格估計 `LatticeEstimator`、預測 `LatticePredictor`、正規方程累加器、NMS；`aoi_domain` 因此是 STATIC）；`application`：`Error`／`Result`／`CancelToken`、`ports/`、`jobs/JobRunner`、`usecases/`、`backend/BackendRegistry`（2026-10-08 起加 `SearchEngine`、`BackendSwitch`：執行中切換的擁有權與規則，ADR-021），S04 起有 `document/`（`UndoStack`、`ProposalSession`、`InspectionDocument`）與 `interaction/`（`InteractionStateMachine`、`InspectionSession`：所有互動規則，沒有 Qt）；`infrastructure`：`backend_fake/`、`perf/`、`threading/`（含 S04 的 `QtThreadPoolExecutor`）；S06 加 `image_qt/`（QImage ⇄ ImageView、`QtImageSource`）、`storage/`（配方與樣本庫的 JSON、TRI 的分層）、`backend_opencv/`（影像轉換與 `OpenCvImageSource`，`AOI_WITH_OPENCV=ON` 才建置；S07 加範本建立器、搜尋器、樣本庫比對器，ADR-016）；S08 加 `backend_halcon/`（shape model 的範本建立器、搜尋器、樣本庫比對器、延遲載入與授權判斷、位姿換算，`AOI_WITH_HALCON=ON` 才建置，ADR-017）；`presentation`（`canvas/`、`tools/`、`shell/`、`theme/`、`i18n/`、`dev/`；S04 起接上 application：`presenters/`＝`AutoRoiPresenter`、`ParamsPresenter`、`BackendPresenter`、`SceneBuilder`、`AppPresenters`，`objects/`＝物件樹，`params/`＝參數面板）；`app`：`CompositionRoot`（組裝）與 `main.cpp`。檔案命名見 `docs/architecture.md` |
| `tests/ui/` | Qt Test（S02、S04）：`tst_*` 是單元測試與整合測試（`tst_presenter` 接上真的核心、假 back end 與視窗；`PresenterRig.h` 是它的測試台），`perf_ui_baseline`、`perf_paint_micro`、`perf_ui_s04` 是效能量測（`test.ps1 -Perf`）；S09 加 A/B 比較視窗的 `tst_compare` 與 `perf_compare_ui` |
| `tests/core/` | Qt Test（S03、S04）：核心的單元測試（`tst_domain`、`tst_core_types`、`tst_job_runner`、`tst_generate_objects`、`tst_fake_backend`、`tst_backend_registry`、`tst_perf_probe`；S04 新增 `tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`、`tst_qt_executor`）、`tst_cli`（從外面用真的 `AiInspTool.exe` 行程驗證 `--backend`）、`perf_core`（效能量測）、`fixtures/deps/`（依賴檢查的 15 個 fixture；`check_deps*` 由 `test.ps1` 跑）；S06 新增 `tst_image_chain`、`tst_recipe_saver`；2026-10-08 新增 `tst_backend_switch`（執行中切換 back end 的規則，ADR-021）。S03 的 `tst_grid` 在 S05 搬到 `tests/domain/tst_lattice.cpp` |
| `tests/backend/` | Qt Test（S07，`AOI_WITH_OPENCV=ON` 才有）：`tst_opencv_backend`（S09 起只留 OpenCV 才有的事：金字塔、小分數圖的 NCC、複製次數、位元數；契約與端到端搬到 `tests/contract/`）；效能量測 `perf_opencv`、`perf_opencv_real`（使用者的真實影像，**不入版控**：環境變數 `AOI_REAL_IMAGE_DIR` 指到影像資料夾才跑，否則跳過；疊圖寫到測試的工作資料夾 `build/qmake-*/test-work/real-*-overlay.png`）；兩個都是效能量測。S08（`AOI_WITH_HALCON=ON` 才有；不能用 HALCON 時跳過並說原因）：`tst_halcon_backend`（只留授權、位姿公式、中斷、nativeGray16）、`tst_halcon_library_matcher`（只留一次 vs 每類型）、`perf_halcon`（OpenCV 也編進來時同一次量 `CV-*` 並排）、`perf_halcon_real`（要 OpenCV 讀 TIFF） |
| `tests/contract/` | Qt Test（S09）：契約測試，以組裝根註冊的 back end 參數化（一個 back end 一列；沒編進來或沒有授權的 **SKIPPED 並說原因**）：`tst_contract_searcher`、`tst_contract_end_to_end`、`tst_contract_library_matcher`；容差只在 `ContractTolerances.h`（改之前看 ADR-004）；效能量測 `perf_ab`（A/B 並排，每一種情境的比較寫到 `build/qmake-*/test-work/ab-*.json`，說明頁可以載入）。每一種建置都有 |
| `third_party/halcon/` | S08：HALCON 24.11.2 的標頭與 `halconcpp.lib`（使用者指示從 CV_Algorithm 專案複製；`-text`，照原樣，不要改；換版本整份換掉並更新 `README.md`） |
| `tests/infra/` | Qt Test（S06）：`tst_image_qt`、`tst_recipe_store`、`tst_library_store`、`tst_library_layers`、`tst_synthetic_board`、`tst_image_source_composition`；`AOI_WITH_OPENCV=ON` 時加 `tst_opencv_image`、`tst_image_sources_agree`；效能量測 `perf_storage`；`fixtures/recipe-v1.json`、`library-v1.json`（v1 的範例檔：手寫、**不要改**，之後的版本也要讀得進來）、`recipe-v2.json`（S07：v1 加上 `board.significantBits`，同樣不要改） |
| `tests/support/` | 測試共用（S06）：合成板子產生器 `synthetic_board`（標準庫；同 seed 同輸出、標準答案 JSON；8 bit、彩色、12／16 bit）與 `synthetic_board_io`（存成 PNG＋JSON）；`test_environment.cpp`（ADR-018：每個測試執行檔都編進它，在 `main` 之前設 offscreen 與字型） |
| `tests/domain/` | Qt Test（S05）：晶格的測試（`tst_lattice`、`tst_lattice_scenarios`（1 029 種情境，總表印在 log）、`tst_lattice_parts`、`tst_lattice_rules`、`tst_nms`、`tst_lattice_crosscheck`（與 S00 示範頁交叉驗證，fixture 在 `fixtures/`）、`tst_lattice_explainer`（說明頁的資料是不是目前的 C++ 算的；設 `AOI_EXPORT_EXPLAINER=1` 執行會改寫頁面））、`perf_lattice`（T3，效能量測）；`test.ps1` 的 `s00_demo_fixture_is_current` 檢查 fixture 是否過期 |
| `AiInspTool/` | **唯一的建置**（qmake，ADR-018）：`AiInspTool.pro`（subdirs）、`.qmake.conf`（`AOI_ROOT`、`AOI_QMAKE`、`AOI_BUILD_ROOT`）、`qmake/*.pri`（選項、**依賴表 `aoi_deps.pri`**、OpenCV、HALCON、旗標、程式庫與測試的範本）、`src/`（13 個程式庫的 `.pro`、`app/app.pro` ＝ `AiInspTool.exe`、`presentation/aoi_ux.qrc`）、`tests/`（`tests.pro` ＝ 測試清單、每個測試一個 `.pro`）。`.pro` 對照 `src/`、`tests/` 的結構放在這裡，原始碼用 `$$AOI_ROOT/...` 引用（放在原始碼旁邊時，輸出會跑到建置資料夾外面）；**檔案明確列出**，新檔案要加進對應的 `.pro`。輸出：`<建置>/bin`（`AiInspTool.exe`、測試、OpenCV 的 DLL）、`<建置>/lib`。精靈產生的 `main.cpp`、`mainwindow.*` 沒有被使用（使用者 2026-10-07 決定保留）。Qt Designer 的表單（2026-10-08，ADR-022）在 `src/presentation/`，和類別放在一起，列在 `presentation.pro` 的 `FORMS`；表單裡的文字是不翻譯的佔位，文案由各類別從 `copy.json` 設定。**不在五層之內，`tools/check-deps.mjs` 不掃它** |
| `scripts/dev/` | `env.ps1`（進入開發環境：MSVC、qmake、jom）、`test.ps1`（qmake＋jom 建置、照清單跑測試與 4 個 node 檢查，失敗時印 log；ADR-018） |
| `perf/baseline/` | 各機器的效能基準線（入版控）；原始量測在 `perf/results/`（不入版控） |
| `docs/` | 計畫、ADR、AI 交接文件 |
| `docs/plan/` | S01–S12 每一步的計畫與 Start prompt |
| `docs/steps/` | 每一步的四種輸出；範本在 `_template/` |
| `docs/adr/` | 設計決策，一檔一決策（`NNN-slug.md`） |
| `perf/` | 效能預算；實測結果在 `perf/results/`（不入版控） |
| `docs/ux/` | UX 規格（`ux-spec.md`）、狀態語言 token（`tokens.json`）、文案（`copy.json`）；S02 的 QPalette／QSS 與 i18n 以此為來源 |
| `tools/` | Node 工具：`ste-lint.mjs`、`check-contrast.mjs`；`check-deps.mjs` 在 S03 建立；`s00-demo-fixture.mjs`（S05：執行 S00 示範頁原本的 JavaScript，產生交叉驗證的 fixture；`--check` 比對是否過期） |
| `ai-project-template/` | 與本產品無關的 AI 結構範本（Codex 與 Claude Code 共用）：`New-AiProject.ps1` 把結構套到新資料夾，設計說明見其 `README.md`。已納入版控；除非使用者要求，不要修改 |
| `.agents/`、`.claude/`、`.ai/`、`scripts/ai/` | AI agent 的規範、skill、交接單與檢查腳本，各檔職責見 `.agents/harness/engineering-workflow.md` 第 0 節。模型與推理強度路由（Claude 專用，[ADR-019](docs/adr/019-model-routing.md)、[ADR-020](docs/adr/020-adaptive-model-effort-routing.md)）：根目錄的 `MODEL_ROUTING.md`、`.claude/agents/`（`fast-worker`、`developer`、`developer-deep`、`architect`、`architect-deep`，frontmatter 有 `model` 與 `effort`）、`.claude/hooks/model_router.mjs`（`settings.json` 的 `UserPromptSubmit`） |

## 建置與測試

驗證指令是 agent 判斷「做完了沒有」的唯一依據。尚未建立的檢查，回報時標明「不適用（尚未建立）」，不要寫成通過。

| 目的 | 指令 | 備註 |
|---|---|---|
| 建置 | Qt Creator 開 `AiInspTool/AiInspTool.pro`（Debug／Release／Profile）；命令列：`scripts/dev/test.ps1`（含建置），或 `. .\scripts\dev\env.ps1` 後在建置資料夾 `qmake <repo>\AiInspTool\AiInspTool.pro -spec win32-msvc`、`jom qmake_all`、`jom` | 2026-10-07 起只用 qmake（ADR-018）。選項（qmake 的命令列變數，例如 `AOI_WITH_HALCON=OFF`；Qt Creator 填在 qmake 步驟的「其他引數」）：`AOI_WITH_OPENCV`、`AOI_WITH_HALCON`、`AOI_BUILD_TESTS`、`AOI_DEMO_SCENE`（**預設全開**）、`AOI_OPENCV_DIR`、`AOI_HALCON_SDK_DIR`；打開的選項找不到函式庫時 qmake 直接報錯 |
| 單元測試 | `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1`（含建置；預設 Release＋OpenCV＋HALCON；`-Config Debug`／`Profile`、`-NoOpenCv`、`-NoHalcon`、`-Filter`、`-NoBuild`）；Qt Creator：〔工具 › 測試 › 執行所有測試〕或測試面板 | 測試清單在 `AiInspTool/tests/tests.pro`。預設的建置 52 個測試執行檔＋4 個 node 檢查（`check_deps`、`check_deps_self_test`、`check_deps_catches_a_violation`、`s00_demo_fixture_is_current`）＝ **56 項**；`-NoHalcon` 54、`-NoOpenCv` 53、兩個都關 51（S09：加了 3 個契約測試、`tst_backend_comparison`、`tst_compare`，`tst_opencv_end_to_end`、`tst_halcon_end_to_end`、`tst_opencv_library_matcher` 併入契約測試後刪除；之前是 54／51／49／46）。offscreen 平台、每項逾時 300 秒、log 在 `build/qmake-*/test-logs/`；最後列出通過數與「有跳過項目的測試」（HALCON 讀不到授權時整組跳過，**跳過不是通過**，要看 log）。node 的 4 項只在 `test.ps1`；契約測試（`tests/contract/`）在每一種建置都有，沒編進來或沒有授權的 back end 那一列 SKIPPED |
| 效能基準線 | `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1 -Perf`（只跑 `AOI_PERF_TESTS`：`perf_*`；要 Release） | S02 的 UI 基準線（U1、U4、I1、I2、I5）、S03 的核心量測（`perf_core`：PerfProbe 開關成本、工作延遲、取消延遲、use case 自己的成本）、S05 的晶格量測（`perf_lattice`：T3 晶格擬合 1 000 點、NMS、O(1) 漸進重新擬合對每次重算）與 S04 的互動量測（`perf_ui_s04`：U2 物件表填入 5 000 列、U3 分數滑桿到重繪、I3 搜尋期間事件迴圈最長阻塞、I4 按取消到 UI 閒置、I6 復原 1 000 框的接受；`perf_paint_micro` 是繪製的微基準）、S09 的 A/B（`perf_ab`：兩個 back end 在 12 種 5 MP 合成板子上並排；`perf_compare_ui`：A/B 疊圖的 U1 類）；Release 才有意義；結果見 `docs/steps/S02-ui-shell/perf.md`、`docs/steps/S03-core/perf.md`、`docs/steps/S04-presenter/perf.md`、`docs/steps/S05-lattice/perf.md`、`docs/steps/S09-contract-ab/perf.md`，基準線在 `perf/baseline/` |
| 執行程式 | Qt Creator 的〔執行〕（執行設定選 `AiInspTool`）；命令列：`build\qmake-release\bin\AiInspTool.exe`（Qt 的 `bin` 要在 PATH 上，`env.ps1` 會加）。`--demo 5000` 開合成板子與框，`--lang zh_TW\|en`、`--theme light\|dark\|system`、`--backend fake\|opencv\|halcon`（沒給時用上次用的、沒有記錄就是 `fake`；預設的建置三個都有）；命令列指定的 back end 沒編進程式、沒有 runtime 或沒有授權時以結束碼 2 明確失敗，訊息寫到 stderr；授權的提醒也寫到 stderr。執行中可以在上方的 back end 選單切換（ADR-021）；上次用的記在 `%APPDATA%\AOI\AutoCadCreated.ini`（`backend/last`；環境變數 `AOI_SETTINGS_DIR` 可以換資料夾，`tst_cli` 用暫存資料夾），記住的那一個在視窗出現之後於背景換過去，不能用時留在 fake 並在訊息列說原因 | `AiInspTool.exe` 是 GUI 程式（ADR-018）：stderr 要經管線或重新導向才看得到（Qt Creator 的〔應用程式輸出〕、`AiInspTool.exe … 2> err.txt`） |
| 依賴方向檢查 | `node tools/check-deps.mjs` | S03 起可用；掃 `src/` 的 `#include`，違規即失敗（結束碼 1）。`--self-test` 對 15 個 fixture 驗證它真的抓得到；`--root <dir>` 掃別的樹。`test.ps1` 的 `check_deps`、`check_deps_self_test`、`check_deps_catches_a_violation`（找不到 Node 時算失敗）。連結與 Qt 標頭的那一半由依賴表 `AiInspTool/qmake/aoi_deps.pri` 擋 |
| 文件風格檢查 | `node tools/ste-lint.mjs` | 現在可用；只檢查 `<!--STE-->` 標記之間的文字 |
| 顏色對比檢查 | `node tools/check-contrast.mjs` | S01 起可用；驗算 `docs/ux/tokens.json`（文字 ≥ 4.5、圖形 ≥ 3）並檢查四種狀態線型不重複；改過顏色後加 `--write` 更新結果 |
| 效能 | S10 的 harness（新的執行檔照 ADR-018 加 `.pro` 並列進 `tests.pro`） | 預算在 `perf/budgets.json` |
| AI 結構檢查 | `powershell -ExecutionPolicy Bypass -File scripts/ai/check-ai-structure.ps1` | 改過入口檔或 skill 後執行 |
| 模型路由檢查 | `node scripts/ai/check-model-routing.mjs` | 改過 `MODEL_ROUTING.md`、`.claude/agents/`、`.claude/hooks/` 或 `.claude/settings.json` 後執行；不花額度 |
| 模型路由實測 | `node scripts/ai/bench-model-routing.mjs verify --yes`、`… bench --yes` | **會呼叫 Claude、花額度**（verify 約 US$1）；verify 從 session 紀錄驗證五個 subagent 實際的 model 與 effort（換 Claude Code 版本或帳號後跑），bench 比較同一組任務在五個 agent 上的正確率、時間、tokens；沒有 `--yes` 只印計畫，結果在 `perf/results/model-routing-*.json`（`MODEL_ROUTING.md` 第 8 節） |

`VERIFIED` 的定義（取自 Step Protocol 第 3 步）：建置通過、單元與契約測試通過（`scripts/dev/test.ps1` 的預設建置 Release＋OpenCV＋HALCON 全部通過，含 `check-deps` 的 3 項；動到 Debug 才有的行為或選項時也跑 `-Config Debug` 與關掉選項的組合）、`ste-lint` 無 error，且本步驟的效能指標已量測（`test.ps1 -Perf`）並與 `perf/budgets.json` 比對。HALCON 的測試「跳過」不算通過（看 `test.ps1` 最後列的「有跳過項目的測試」）。失敗就如實回報，不要宣稱完成。

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
| 改分層、新增模組 | `docs/architecture.md`、`docs/decisions.md`（索引，連到 `docs/adr/`）、`AiInspTool/qmake/aoi_deps.pri` | 已記錄的決定沒有新證據不推翻；依賴表、`architecture.md`、`check-deps` 的 `RULES` 三邊一起改 |
| 改建置（`.pro`、`.pri`、選項、測試清單） | [ADR-018](docs/adr/018-qmake-only-build.md)、`AiInspTool/qmake/*.pri`、`AiInspTool/tests/tests.pro` | 改完跑 `test.ps1` 的預設與 `-Config Debug`；動到選項時加 `-NoOpenCv`、`-NoHalcon` 的組合；Qt Creator 的三種組態要建得起來 |
| 寫步驟文件 | `docs/steps/_template/`、`project-rules.md` 的「每一步的四種輸出」 | 跑 `node tools/ste-lint.mjs` |
| 效能相關 | `docs/perf-test-plan.md`、`perf/budgets.json` | 沒有 before 數字不動手 |
| 改 port、use case 或新增 back end | `docs/steps/S03-core/spec.md`、port 標頭裡的契約、ADR-010、`docs/architecture.md` | 新 back end ＝ adapter ＋ 在 `CompositionRoot.cpp` 註冊；不要改 `application/`、`domain/` |
| 改 OpenCV 的搜尋 | `docs/steps/S07-opencv/spec.md`、ADR-016、`OpenCvSearchOptions.h`、`docs/steps/S07-opencv/perf.md` | 改完跑 `tst_opencv_*` 與 `perf_opencv`（T2a、T4、T6；`test.ps1 -Perf -Filter 'perf_opencv*'`）；真實影像用 `perf_opencv_real` |
| 改任何 back end 的行為、新增 back end、改 port 的契約 | [ADR-004](docs/adr/004-backend-divergence.md)、`tests/contract/ContractTolerances.h`、`docs/steps/S09-contract-ab/spec.md` 的契約表 | 改完跑 `test.ps1 -Filter 'tst_contract*'`（每個 back end 一列）；放寬容差要在 ADR-004 寫理由；新 back end 在組裝根註冊就自動多一列 |
| 改 HALCON 的搜尋或授權 | `docs/steps/S08-halcon/spec.md`、ADR-017、ADR-010 的 S08 後續、`HalconSearchOptions.h`、`docs/steps/S08-halcon/perf.md`；運算子的行為查 `%HALCONROOT%\help\operators_en_US.ref` 或 `get_operator_info`，不憑記憶 | 改完跑 `tst_halcon_*`、`tst_cli` 與 `perf_halcon`（T2b、T4、T6、AS-*）；換 HALCON 版本要重跑全部 |
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
| 晶格（lattice） | 元件陣列的規律：格子 (i, j) 的中心 ＝ origin ＋ i·u ＋ j·v；一維（一排）時沒有 v。`domain/lattice/LatticeEstimator` 從找到的元件找出它（可能多組），`LatticePredictor` 給缺格；寧可沒有晶格也不要錯的晶格（ADR-013） |
| NMS | 非極大值抑制：同一個元件被找到好幾次時只留分數最高的；`domain/lattice/NonMaxSuppression`，旋轉矩形的精確重疊 |
| 假 back end（fake） | `infrastructure/backend_fake`：以樣本為中心排可重現的合成網格，不看影像；`--backend=fake`，S07 之前讓整個流程有東西可跑 |
| 由粗到細（coarse-to-fine） | OpenCV 搜尋器（S07）：先在縮小的影像（金字塔頂層）上比所有角度，再只在候選附近往下一層一層對準；ADR-016 |
| NCC | 正規化相關係數（`TM_CCOEFF_NORMED`）：範本與影像各扣掉平均後的相關，−1..1，對亮度的倍數與偏移不敏感；OpenCV 搜尋器的分數 |
| shape model | HALCON 的比對模型（S08）：樣本邊緣上的點與梯度方向；分數是方向一致的程度（0..1），和 NCC 不是同一把尺 |
| RTL／SDK dongle | HALCON 的兩種授權（S08，使用者說明）：runtime（部署用）與 development（開發用，另有 HDevelop）。HALCON 用找得到 dongle 的那一個、不報錯；程式在「用 runtime、SDK 的 dongle 沒插」時提醒 |
| 契約測試（contract test） | S09：同一組測試對每個 back end 各跑一次（`tests/contract/`）；port 的文字與 ADR-010 違反就是 bug，準確度的上限在 `ContractTolerances.h`（ADR-004） |
| A/B 比較 | S09：同一個樣本在兩個 back end 依序搜尋，配對成「兩邊都找到／只有 A／只有 B」與原因（`application/compare/BackendComparison`）；主視窗的〔A/B 比較…〕打開比較視窗（`presentation/compare/`），兩邊的門檻各自一個 |
| 有效位元數（significantBits） | 16 bit 灰階影像實際用到的位元數（12 bit 相機 → 值到 4095）；看最大值推測，使用者可以在參數面板覆寫（S07，存在板子／配方 v2） |
