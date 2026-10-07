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
| 建置 | CMake ＋ Ninja | `CMakeLists.txt`、`CMakePresets.json`（debug／release／relwithdebinfo；S06 加 debug-opencv／release-opencv）；自家程式碼 `/W4 /WX` |
| 視覺 | OpenCV、HALCON | 只能在對應的 `infrastructure/backend_*` 使用 |
| 測試 | Qt Test | [ADR-003](docs/adr/003-test-framework.md)（採用），以 CTest 執行 |
| 工具腳本 | Node.js | `tools/*.mjs` |
| 平台 | Windows 11、MSVC 2022 | 編譯器尚未驗證，見下表 |

## 環境（Windows 11，本機實際探測結果；S02 於 2026-10-05 實際建置與執行驗證；S06 於 2026-10-06 改用 Qt 6.12.0）

| 項目 | 位置與版本 | 驗證 |
|---|---|---|
| Visual Studio 2022 Community 17.14（2026-10-07 更新到 17.14.37710） | `D:\VS2022`（用 `vswhere` 找，不寫死） | `cl` 19.44.35229（toolset 14.44.35207；更新前是 19.44.35228）。同一個 VS 還有 14.16、14.29、14.32 三個舊 toolset，**沒有使用**。VS 安裝程式在更新時 `vswhere` 找不到 VS（`env.ps1` 會失敗），等更新完成再建置 |
| Qt 6.12.0 MSVC2022 64-bit（S06 起；使用者 2026-10-06 更新） | `D:\Qt\6.12.0\msvc2022_64`（preset 與 `env.ps1` 的預設） | 有 Widgets、Test、Gui、Core、Concurrent、Svg 與 Debug 版程式庫；S06 實際 configure、build、四種組態的測試、效能量測都跑過。影像外掛和 6.8.0 一樣只有 gif、ico、jpeg、svg（**沒有 TIFF**）。6.12.0 棄用了 `QDragEnterEvent` 吃 `QPoint` 的建構子（`/WX` 會擋），用 `QPointF` 的 |
| Qt 6.8.0 MSVC2022 64-bit | `D:\Qt\6.8.0\msvc2022_64`（仍在機器上） | S02–S05 用它建置與量測（那幾步的基準線是 6.8.0 的數字）。要改回它：`AOI_QT_ROOT` 設成這個路徑、configure 時 `-DCMAKE_PREFIX_PATH=D:/Qt/6.8.0/msvc2022_64`，建置目錄要刪掉重建 |
| CMake / Ninja | `D:\Qt\Tools\CMake_64\bin`（3.30.5）/ `D:\Qt\Tools\Ninja`（1.12.1） | 實際使用 |
| OpenCV 4.9.0 (vc16) | `D:\opencv\opencv\build`；**`OpenCV_DIR` 要指到 `D:\opencv\opencv\build\x64\vc16\lib`**（preset 已設）。指到 `build` 根目錄會失敗：根目錄的 `OpenCVConfig.cmake`（Windows Pack）找不到相容 MSVC 19.44 的二進位檔。只有 `opencv_world490`（Release）／`opencv_world490d`（Debug）一個 DLL；有 libtiff、libpng、libjpeg、OpenEXR（EXR 要設 `OPENCV_IO_ENABLE_OPENEXR` 才開放，沒有用） | **S06 起編進程式、連結與執行都成功**（vc16 的程式庫配 VS 2022 的 MSVC 19.44；preset `release-opencv`／`debug-opencv`，`aoi.exe` 建置後會複製 DLL，測試的 PATH 由 `cmake/AoiTargets.cmake` 加上）。實測：`imdecode` 讀 12 bit 緊密打包的 TIFF 會給 16 bit、值左移 4 位。**S07 起是可用的 back end**（`--backend opencv`；`cv::setNumThreads` ＝ 核心數 − 1）。使用者的影像是 12 bit、存成 16 bit 容器（值 0..4095） |
| HALCON 24.11.2 Progress-Steady | runtime：`%HALCONROOT%` = `C:\Program Files\MVTec\HALCON-24.11-Progress-Steady`（`bin\x64-win64` 在 PATH 上）；開發檔：repo 的 `third_party/halcon/`（S08 從 CV_Algorithm 專案複製，見那裡的 `README.md`） | **S08 起編進程式、連結與執行都成功**（preset `release-halcon`／`debug-halcon`＝OpenCV＋HALCON；`halconcpp.dll` 延遲載入）。`%HALCONROOT%` 本身仍沒有 `include\`、`lib\`。**授權**：`license.dat` 是 runtime（RTL，CodeMeter `3-7614034`，有插）、`license--DEVELOP-.dat` 是 SDK（`3-7081033`，**沒插**）；HALCON 用 runtime 授權，程式會提醒（ADR-017）。模組含 Matching。第一次呼叫 HALCON 約 1.6 秒（初始化＋授權）。授權檔寫 `SESSIONS=1`。2026-10-07 有一次 HALCON 突然讀不到插著的 RTL dongle（錯誤 2300、`hhostid` 看不到、Windows 仍認得 `VID_064F` 的裝置），**重插 dongle** 後恢復。運算子說明：`%HALCONROOT%\help\operators_en_US.ref`（文字檔）或執行時 `get_operator_info` |
| HALCON 22.11.3 Steady（另一套，本專案沒有用） | `C:\Users\Caspar.Hsu\AppData\Local\Programs\MVTec\HALCON-22.11-Steady` | 使用者的其他工具；授權是過期的試用版（`VALID=2026-01-01`）。`HALCONROOT`（使用者環境變數）指 24.11，所以 22.11 的 HDevelop 會讀 24.11 的授權檔、報 2041 Invalid host。不要把 22.11 的 `bin` 放到本專案的 PATH 前面（`HalconRuntime` 會因版本不合判定不可用） |

- **進入開發環境**：每個 PowerShell session 先 dot-source `. .\scripts\dev\env.ps1`（用 `vswhere` 找 VS、跑 `vcvars64.bat`、把 Qt／CMake／Ninja 放進 PATH）。路徑可用環境變數 `AOI_QT_ROOT`、`AOI_QT_TOOLS` 覆寫。Claude Code 的 PowerShell 工具**每次呼叫都是新的 shell**，所以每次呼叫都要重新 dot-source。
- **建置要在 `env.ps1` 建立的環境裡做（configure 與 build 同一個）**：這台是繁中 Windows，`cl.exe` 的 `/showIncludes` 輸出是中文，而且這個 VS 只裝了 1028 繁中資源（`VSLANG=1033` 無效）。Ninja 靠這行中文前綴記錄標頭相依；前綴編碼取決於主控台字碼頁，configure 與 build 的字碼頁不同時（實測：從 Git Bash 啟動 `powershell.exe` 就會），相依記錄掉了，**只改標頭檔不會重新編譯**，輸出還會被亂碼洪水淹沒。`env.ps1` 會把字碼頁對齊系統 OEM 字碼頁。症狀出現時（亂碼洪水、改標頭沒反應）：刪掉 `build/<preset>` 重新 configure。
- **路徑長度**：Windows 預設路徑上限 260 字元。建置目錄放在 repo 內的 `build/<preset>`（被 gitignore）；放到很深的暫存路徑（例如 Claude 的 scratchpad）時，MSVC 會因 PDB 路徑過長失敗（C1041）。
- **Qt Test 在這台機器不把結果寫到 stdout**（重新導向後是空的）；要用 `-o 檔案,txt`。`CMakeLists` 已經把 log 放在 `build/<preset>/test-logs/<name>.txt`，`scripts/dev/test.ps1` 失敗時會印出來。
- offscreen 平台不會自己找到字型：要設 `QT_QPA_FONTDIR=C:/Windows/Fonts`（CTest 與 test preset 已設）。
- Qt 6.8 起（6.12.0 也一樣，測試照舊通過）的 `LanguageChange` 是排入事件佇列的，語言切換後要等下一輪事件迴圈（測試用 `QCoreApplication::processEvents()`）。
- Shell 是 Windows PowerShell 5.1：沒有 `&&`；commit 訊息寫成檔案再 `git commit -F <檔案>`，不要用 `-F -`（格式見 `project-rules.md`「Commit 訊息」）。
- Node 可用（v22），工具腳本放 `tools/*.mjs`（`check-deps.mjs` 也由 CTest 呼叫，所以 `node` 要在 PATH 上）。`python.exe` 只是 Store 捷徑，不要依賴。
- 以上路徑若有差異，**更新此表**。

## 重要目錄

| 路徑 | 內容 |
|---|---|
| `src/` | 程式碼，依五層分（`domain`、`application`、`infrastructure`、`presentation`、`app`）。`domain`：型別多半只有標頭（座標與角度慣例在 `geometry.h`，框的展開在 `box_expander.h`，影像的所有權規則在 `image_view.h`；S06 的 `image_buffer`、`image_copy`（複製計數）、`image_depth`（12 bit）有 .cpp），S05 起 `lattice/` 有 .cpp（晶格估計 `LatticeEstimator`、預測 `LatticePredictor`、正規方程累加器、NMS；`aoi_domain` 因此是 STATIC）；`application`：`Error`／`Result`／`CancelToken`、`ports/`、`jobs/JobRunner`、`usecases/`、`backend/BackendRegistry`，S04 起有 `document/`（`UndoStack`、`ProposalSession`、`InspectionDocument`）與 `interaction/`（`InteractionStateMachine`、`InspectionSession`：所有互動規則，沒有 Qt）；`infrastructure`：`backend_fake/`、`perf/`、`threading/`（含 S04 的 `QtThreadPoolExecutor`）；S06 加 `image_qt/`（QImage ⇄ ImageView、`QtImageSource`）、`storage/`（配方與樣本庫的 JSON、TRI 的分層）、`backend_opencv/`（影像轉換與 `OpenCvImageSource`，`AOI_WITH_OPENCV=ON` 才建置；S07 加範本建立器、搜尋器、樣本庫比對器，ADR-016）；S08 加 `backend_halcon/`（shape model 的範本建立器、搜尋器、樣本庫比對器、延遲載入與授權判斷、位姿換算，`AOI_WITH_HALCON=ON` 才建置，ADR-017）；`presentation`（`canvas/`、`tools/`、`shell/`、`theme/`、`i18n/`、`dev/`；S04 起接上 application：`presenters/`＝`AutoRoiPresenter`、`ParamsPresenter`、`SceneBuilder`、`AppPresenters`，`objects/`＝物件樹，`params/`＝參數面板）；`app`：`CompositionRoot`（組裝）與 `main.cpp`。檔案命名見 `docs/architecture.md` |
| `tests/ui/` | Qt Test（S02、S04）：`tst_*` 是單元測試與整合測試（`tst_presenter` 接上真的核心、假 back end 與視窗；`PresenterRig.h` 是它的測試台），`perf_ui_baseline`、`perf_paint_micro`、`perf_ui_s04` 是效能量測（label `perf`） |
| `tests/core/` | Qt Test（S03、S04）：核心的單元測試（`tst_domain`、`tst_core_types`、`tst_job_runner`、`tst_generate_objects`、`tst_fake_backend`、`tst_backend_registry`、`tst_perf_probe`；S04 新增 `tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`、`tst_qt_executor`）、`tst_cli`（從外面用真的 `aoi` 行程驗證 `--backend`）、`perf_core`（label `perf`）、`fixtures/deps/`（依賴檢查的 15 個 fixture）；S06 新增 `tst_image_chain`、`tst_recipe_saver`；`check_deps*` 的 CTest 項目也在這裡註冊。S03 的 `tst_grid` 在 S05 搬到 `tests/domain/tst_lattice.cpp` |
| `tests/backend/` | Qt Test（S07，`AOI_WITH_OPENCV=ON` 才有）：`tst_opencv_backend`（契約草稿、座標與角度、格式、取消、複製次數）、`tst_opencv_end_to_end`（組裝根的 OpenCV ＋ use case，合成板子與標準答案；有效位元數的覆寫）、`tst_opencv_library_matcher`（流程 B）；效能量測 `perf_opencv`、`perf_opencv_real`（使用者的真實影像，**不入版控**：環境變數 `AOI_REAL_IMAGE_DIR` 指到影像資料夾才跑，否則跳過；疊圖寫到建置目錄的 `real-*-overlay.png`）；label `perf`。S08（`AOI_WITH_HALCON=ON` 才有；不能用 HALCON 時跳過並說原因）：`tst_halcon_backend`、`tst_halcon_end_to_end`、`tst_halcon_library_matcher`、`perf_halcon`（OpenCV 也編進來時同一次量 `CV-*` 並排）、`perf_halcon_real`（要 OpenCV 讀 TIFF） |
| `third_party/halcon/` | S08：HALCON 24.11.2 的標頭與 `halconcpp.lib`（使用者指示從 CV_Algorithm 專案複製；`-text`，照原樣，不要改；換版本整份換掉並更新 `README.md`） |
| `tests/infra/` | Qt Test（S06）：`tst_image_qt`、`tst_recipe_store`、`tst_library_store`、`tst_library_layers`、`tst_synthetic_board`、`tst_image_source_composition`；`AOI_WITH_OPENCV=ON` 時加 `tst_opencv_image`、`tst_image_sources_agree`；效能量測 `perf_storage`（label `perf`）；`fixtures/recipe-v1.json`、`library-v1.json`（v1 的範例檔：手寫、**不要改**，之後的版本也要讀得進來）、`recipe-v2.json`（S07：v1 加上 `board.significantBits`，同樣不要改） |
| `tests/support/` | 測試共用（S06）：合成板子產生器 `synthetic_board`（標準庫；同 seed 同輸出、標準答案 JSON；8 bit、彩色、12／16 bit）與 `synthetic_board_io`（存成 PNG＋JSON） |
| `tests/domain/` | Qt Test（S05）：晶格的測試（`tst_lattice`、`tst_lattice_scenarios`（1 029 種情境，總表印在 log）、`tst_lattice_parts`、`tst_lattice_rules`、`tst_nms`、`tst_lattice_crosscheck`（與 S00 示範頁交叉驗證，fixture 在 `fixtures/`）、`tst_lattice_explainer`（說明頁的資料是不是目前的 C++ 算的；設 `AOI_EXPORT_EXPLAINER=1` 執行會改寫頁面））、`perf_lattice`（T3，label `perf`）；CTest 的 `s00_demo_fixture_is_current` |
| `AiInspTool/` | 使用者用 Qt Creator 開啟的 **qmake 專案**（`AiInspTool.pro`＋`aoi_ux.qrc`）：直接建置 `../src` 的 domain、presentation 與 `app/main.cpp`（程式碼只有一份，用萬用字元納入）。精靈產生的 `main.cpp`、`mainwindow.*` 沒有被使用。**不在五層之內，`tools/check-deps.mjs`（S03）不要掃它**；單元測試只在 CMake。S03 起 `src/` 多出 application、infrastructure（OpenCV、HALCON）時，這個 `.pro` 要跟著調整，否則 Qt Creator 建置會壞（正式路徑是 CMake）。S03 已把 application、`infrastructure/{backend_fake,perf,threading}` 與組裝根納入（萬用字元遞迴納入，S04 新增的 `application/{document,interaction}`、`threading/QtThreadPoolExecutor`、`presentation/{presenters,objects,params}` 不必改 `.pro`）；S06 加了 `infrastructure/image_qt`、`storage`（萬用字元；Qt 6.12.0 的 qmake＋jom 建置過）；`backend_opencv` 不在 qmake 專案裡（要 OpenCV），所以 Qt Creator 版讀不了 TIFF、也沒有 OpenCV back end（S07 的搜尋器只在 CMake 的 `*-opencv` preset）；`backend_halcon` 也不在（S08；只在 CMake 的 `*-halcon` preset），qmake 版的 halcon 是「沒有編進程式」 |
| `cmake/`、`CMakeLists.txt`、`CMakePresets.json` | 建置設定；`cmake/AoiTargets.cmake` 放警告設定與 `aoi_add_qt_test` |
| `scripts/dev/` | `env.ps1`（進入開發環境）、`test.ps1`（建置＋跑測試，失敗時印 log） |
| `perf/baseline/` | 各機器的效能基準線（入版控）；原始量測在 `perf/results/`（不入版控） |
| `docs/` | 計畫、ADR、AI 交接文件 |
| `docs/plan/` | S01–S12 每一步的計畫與 Start prompt |
| `docs/steps/` | 每一步的四種輸出；範本在 `_template/` |
| `docs/adr/` | 設計決策，一檔一決策（`NNN-slug.md`） |
| `perf/` | 效能預算；實測結果在 `perf/results/`（不入版控） |
| `docs/ux/` | UX 規格（`ux-spec.md`）、狀態語言 token（`tokens.json`）、文案（`copy.json`）；S02 的 QPalette／QSS 與 i18n 以此為來源 |
| `tools/` | Node 工具：`ste-lint.mjs`、`check-contrast.mjs`；`check-deps.mjs` 在 S03 建立；`s00-demo-fixture.mjs`（S05：執行 S00 示範頁原本的 JavaScript，產生交叉驗證的 fixture；`--check` 比對是否過期） |
| `ai-project-template/` | 與本產品無關的 AI 結構範本（Codex 與 Claude Code 共用）：`New-AiProject.ps1` 把結構套到新資料夾，設計說明見其 `README.md`。已納入版控；除非使用者要求，不要修改 |
| `.agents/`、`.claude/`、`.ai/`、`scripts/ai/` | AI agent 的規範、skill、交接單與檢查腳本，各檔職責見 `.agents/harness/engineering-workflow.md` 第 0 節 |

## 建置與測試

驗證指令是 agent 判斷「做完了沒有」的唯一依據。尚未建立的檢查，回報時標明「不適用（尚未建立）」，不要寫成通過。

| 目的 | 指令 | 備註 |
|---|---|---|
| 建置 | `. .\scripts\dev\env.ps1` 後 `cmake --preset release` 與 `cmake --build build/release`（`debug`、`relwithdebinfo` 同理） | S02 起可用。選項 `AOI_WITH_OPENCV`、`AOI_WITH_HALCON`（預設 OFF）、`AOI_BUILD_TESTS`、`AOI_DEMO_SCENE` |
| 單元測試 | `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1`（含建置；`-Preset debug` 可換組態） | S02 起 5 個 UI 的 Qt Test 執行檔；S03 起再加 8 個核心的測試執行檔、`tst_cli`（命令列）與 3 個 `check_deps*` 項目；S04 起再加 7 個核心（`tst_undo_stack`…`tst_qt_executor`）與 5 個 UI（`tst_objects_model`、`tst_params`、`tst_messages`、`tst_scene`、`tst_presenter`）；S05 起 `tst_grid` 併入 `tests/domain/`，再加 7 個 domain 的測試執行檔與 `s00_demo_fixture_is_current`，共 36 個項目；S06 再加 `tst_image_view`、`tst_image_chain`、`tst_recipe_saver` 與 `tests/infra/` 的 6 個，共 45 個（`-Preset release-opencv` 再加 2 個，共 47 個）；S07 的 `tests/backend/` 只在 OpenCV 的 preset：`release` 仍是 45 個，`release-opencv` 50 個；S08 的 `-Preset release-halcon`（OpenCV＋HALCON）53 個；經 CTest 在 offscreen 平台執行；契約測試 S09 起 |
| 效能基準線 | `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1 -Perf` | S02 的 UI 基準線（U1、U4、I1、I2、I5）、S03 的核心量測（`perf_core`：PerfProbe 開關成本、工作延遲、取消延遲、use case 自己的成本）、S05 的晶格量測（`perf_lattice`：T3 晶格擬合 1 000 點、NMS、O(1) 漸進重新擬合對每次重算）與 S04 的互動量測（`perf_ui_s04`：U2 物件表填入 5 000 列、U3 分數滑桿到重繪、I3 搜尋期間事件迴圈最長阻塞、I4 按取消到 UI 閒置、I6 復原 1 000 框的接受；`perf_paint_micro` 是繪製的微基準）；Release 才有意義；結果見 `docs/steps/S02-ui-shell/perf.md`、`docs/steps/S03-core/perf.md`、`docs/steps/S04-presenter/perf.md`、`docs/steps/S05-lattice/perf.md`，基準線在 `perf/baseline/` |
| 執行程式 | 進入開發環境後 `build\release\aoi.exe`；`--demo 5000` 開合成板子與框，`--lang zh_TW\|en`、`--theme light\|dark\|system`、`--backend fake\|opencv\|halcon`（預設 `fake`；`opencv` 要用 `build\release-opencv\aoi.exe`〔S07〕，`halcon` 要用 `build\release-halcon\aoi.exe`〔S08，兩個都有〕；沒編進程式、沒有 runtime 或沒有授權的 back end 以結束碼 2 明確失敗，訊息寫到 stderr；授權的提醒也寫到 stderr） | |
| 依賴方向檢查 | `node tools/check-deps.mjs` | S03 起可用；掃 `src/` 的 `#include`，違規即失敗（結束碼 1）。`--self-test` 對 15 個 fixture 驗證它真的抓得到；`--root <dir>` 掃別的樹。CTest 的 `check_deps`、`check_deps_self_test`、`check_deps_catches_a_violation`（找不到 Node 時不註冊，configure 會警告） |
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
| 改 OpenCV 的搜尋 | `docs/steps/S07-opencv/spec.md`、ADR-016、`OpenCvSearchOptions.h`、`docs/steps/S07-opencv/perf.md` | 改完跑 `tst_opencv_*` 與 `perf_opencv`（T2a、T4、T6）；真實影像用 `perf_opencv_real` |
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
| 有效位元數（significantBits） | 16 bit 灰階影像實際用到的位元數（12 bit 相機 → 值到 4095）；看最大值推測，使用者可以在參數面板覆寫（S07，存在板子／配方 v2） |
