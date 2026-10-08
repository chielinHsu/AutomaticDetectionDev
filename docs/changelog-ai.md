# AI 工作紀錄

> 只新增，新紀錄放最上方。每個 agent session 一筆，讓下一位 agent 知道上一位做過什麼、為什麼。
> agent 欄位寫實際的工具與模型；不要從舊紀錄抄。

## 格式

```markdown
## YYYY-MM-DD — Claude Code (模型) ／ Codex (模型)

### 完成
- 做了什麼（一行一件）

### 修改的檔案
- path/to/file

### 驗證
- 實際執行的指令與結果；沒執行就寫「未驗證」與原因

### 未完成
- 還沒做完的事

### 注意事項
- 下一位 agent 需要知道的限制、否決的做法、不要還原的修改
```

## 2026-10-08 — Claude Code (Claude Sonnet 5.5，中途換成 Claude Opus 5.5)（模型路由）

### 完成
- 依使用者提供的 ChatGPT 討論與其中的設定 prompt，建立模型路由（[ADR-019](adr/019-model-routing.md)）：根目錄的 `MODEL_ROUTING.md`（`CLAUDE.md` 匯入）；`.claude/agents/` 的 `fast-worker`（haiku，唯讀）、`developer`（sonnet，不能再委派）、`architect`（opus，唯讀，輸出修改計畫）；`UserPromptSubmit` hook `.claude/hooks/model_router.mjs`（Node，關鍵字分類，只注入提示）；驗證腳本 `scripts/ai/check-model-routing.mjs`。
- 第一版只寫規則（`.claude/model-routing.md`，沒有 hook 與 subagent）；使用者接著貼上完整的設定 prompt，規則搬到根目錄並加上 subagent 與 hook。session 中途主模型由 Sonnet 5.5 換成 Opus 5.5。

### 修改的檔案
- 新增：`MODEL_ROUTING.md`、`.claude/agents/fast-worker.md`、`.claude/agents/developer.md`、`.claude/agents/architect.md`、`.claude/hooks/model_router.mjs`、`scripts/ai/check-model-routing.mjs`、`docs/adr/019-model-routing.md`
- 修改：`.claude/settings.json`（只加 `hooks`，`permissions` 沒動）、`CLAUDE.md`、`AI_CONTEXT.md`、`docs/decisions.md`、`docs/adr/000-ai-agent-structure.md`、`docs/changelog-ai.md`、`docs/current-status.md`、`.ai/handoff.md`

### 驗證
- `node scripts/ai/check-model-routing.mjs`：44 項通過（含用 Git Bash 跑 `settings.json` 的實際指令字串）。
- 檢查腳本本身：在 scratchpad 的複本故意改壞 9 處（model 寫版本號、唯讀的加 Edit、拿掉 `disallowedTools`、刪一條權限、`bypassPermissions`、`PowerShell(*)`、hook 的 Opus 分支失效、`CLAUDE.md` 不匯入），每一處都報 FAIL。
- headless 實測 `claude -p … --model haiku --output-format stream-json --verbose`（Claude Code 2.1.292）：hook 有執行（context 的提示與 hook 的輸出相同）；`system/init` 列出三個 subagent；三個同時委派、回報回到主 session；實際模型 `fast-worker` → `claude-haiku-4-5-20251001`、`developer` → `claude-sonnet-5-5`、`architect` → `claude-opus-5-5`。這次測試 US$0.59（API 計價）。
- 互動式 session（桌面 app 的 Code 分頁）：改完 `settings.json` 之後，這個 session 的下一則使用者訊息的 context 就有「[模型路由提示]」，不必重新開 session。
- `check-ai-structure.ps1`、`node tools/ste-lint.mjs`（0 errors）、`git diff --check`：通過。
- 沒有建置、沒有跑 `scripts/dev/test.ps1`（沒有改產品程式碼）。

### 未完成
- 沒有 Git Bash 的電腦（Claude Code 改用 PowerShell 執行 hook）沒有驗證。
- commit 在分支 `chore/model-routing`，沒有合併到 `main`、沒有 push（合併等使用者要求）。

### 注意事項
- **`haiku` 別名在 Claude Code 2.1.292 對應 Haiku 4.5，不是 5.5**；照使用者的要求仍用別名。
- 每個 subagent 啟動時載入 `CLAUDE.md` 與匯入的文件（48K–68K tokens）：委派有固定成本（Sonnet 約 US$0.17、Opus 約 US$0.24 起跳）。
- hook 只注入提示，不切換主 session 的模型；`AOI_MODEL_ROUTER=off` 可以關掉。
- ChatGPT 分享頁的程式碼區塊抓不到；hook 與 subagent 依使用者貼上的 prompt 與官方文件（經 `claude-code-guide` 查證）寫，不是照抄來源的範例。來源範例的 Python 在這台機器不能用（`python.exe` 是 Store 捷徑），所以改 Node。

## 2026-10-08 — Claude Code (Claude Opus 5.5)（session A 的後續：HALCON 授權恢復後補驗證）

### 完成
- 2026-10-07 15:06 起 HALCON 拿不到授權（錯誤 2300）。2026-10-08 依序試過：重插、換 USB 埠（原本經過 Genesys Logic 的 Hub）、插電腦本體、真正重新啟動（開機時間 12:47:40），都沒恢復；授權檔、runtime、環境變數、系統更新都沒有變化。使用者在另一台電腦跑 `hhostid` 也看不到這個 dongle，而且使用者自己的專案（`D:\AOI_Project\SType\AlgoParamFineTune\BnSDKLibsController\CV_Algorithm\Algo\Algo_SkewInspect`，呼叫 HALCON 的 API）插著 dongle 時能檢測、拔掉時報 2041 Invalid host。13:20 起我們的程式又拿得到授權，原因不明。
- **更正**：之前說「`hhostid` 看不到 dongle 就代表 HALCON 讀不到」是錯的——`hhostid`（24.11 與 22.11）在這台與另一台都不列出這個 dongle。之前說「沒有別的行程佔著 dongle」也不完整：只用名稱找了本專案的程式與 HDevelop（13:20 時改用「載入 HALCON DLL 的行程」查，沒有）。
- 補跑 HALCON 的驗證並更新 ADR-018 的「驗證」、`current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md` 的 HALCON 那一列。

### 修改的檔案
- `docs/adr/018-qmake-only-build.md`、`docs/current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md`、`docs/changelog-ai.md`

### 驗證
- `test.ps1 -NoBuild -Config Debug`：53／53，`tst_halcon_backend` 29、`tst_halcon_end_to_end` 12、`tst_halcon_library_matcher` 9 項通過、0 跳過；`tst_cli` 通過。
- `test.ps1 -NoBuild -Config Profile -BuildDir AiInspTool\build\Desktop_Qt_6_12_0_MSVC2022_64bit_Profile`：53／53，HALCON 同上。
- `test.ps1 -NoBuild -Perf -Filter 'perf_halcon'`（Release）：通過；T1 p95 5.3 ms、T2b 22.5、T4 26.5、T6a 100%、T6b 0.485 px、授權第一次檢查 1.45 s、取消延遲 p95 19.2 ms（S08：7.0–22.0、31.5–82.2、28.6–46.6、100%、0.48、1.55–1.87、19–30）。預算沒有改。
- Qt Creator 模擬的 Release：`AiInspTool.exe --backend halcon <0_0_B.tif>` 開圖成功，訊息列與 stderr 都有授權提醒（截圖確認）。

### 未完成
- 錯誤 2300 的原因不明。授權 `SESSIONS=1`：別的 HALCON 程式同時在跑會不會讓我們拿不到授權，沒有驗證。

### 注意事項
- 判斷 HALCON 能不能用：`test.ps1 -Filter 'tst_halcon_backend'`，看 log 的 Totals 是不是 0 skipped；不要用 `hhostid`。

## 2026-10-08 — Claude Code (Claude Opus 5.5)（S09 之前的 session A：建置改成只用 qmake）

### 完成
- 使用者 2026-10-07 的決定：建置只用 qmake（`AiInspTool/AiInspTool.pro` 位置不變，包含所有程式庫、程式與所有 Qt Test）、移除 CMake、執行檔改叫 `AiInspTool.exe`、OpenCV 與 HALCON 預設編進去、Qt Creator 的 Debug／Release／Profile 都要能建置。session 中使用者決定：精靈產生的 `AiInspTool/main.cpp`、`mainwindow.*` **保留**；主目錄 `AiInspTool/` 的舊原地建置輸出與 `.qtcreator/AiInspTool.pro.user` 由 agent 在合併前刪除。分支 `build/qmake-only`（從 `main` 的 `bd4e285`），在 worktree 做。
- 先用小範例驗證推論：subdirs 的子專案寫成 `../lib/lib.pro` 時，輸出跑到 `<建置>/../lib`（建置資料夾外面，三種組態共用）→ 所有 `.pro`／`.pri` 放在 `AiInspTool/` 底下對照 `src/`、`tests/`，原始碼用 `$$AOI_ROOT` 引用（ADR-018）。
- qmake 專案：`AiInspTool.pro`（subdirs；qmake 時印出這次建置有哪些功能）、`.qmake.conf`、`qmake/`（`aoi_options`、`aoi_deps`＝依賴表、`aoi_opencv`、`aoi_halcon`、`aoi_target`、`aoi_lib`、`aoi_test`）、`src/src.pro`＋13 個程式庫＋`app/app.pro`（`AiInspTool.exe`，GUI 子系統）、`tests/tests.pro`（測試清單，qmake 時寫 `aoi-tests.txt`、`aoi-perf-tests.txt`）＋`test_support.pro`＋59 個測試的 `.pro`＋5 個資料夾的 `*_tests.pri`。檔案明確列出。`aoi_ux.qrc` 搬到 `AiInspTool/src/presentation/`，由用到 presentation 的程式內嵌；OpenCV 的 DLL 由 `backend_opencv.pro` 的 `COPIES` 複製到 `<建置>/bin`；`/IGNORE:4199`（LNK4199，連了組裝根但沒用到 HALCON 的測試，29 個）。
- `tests/support/test_environment.cpp`：每個測試在 `main` 之前設 offscreen 與字型（不覆寫已有的設定），Qt Creator 直接執行測試也一樣。
- `scripts/dev/test.ps1` 改寫：qmake → `jom qmake_all` → jom（和 Qt Creator 相同）；`-Config Release|Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`、`-BuildDir`、`-TimeoutSec`、`-Jobs`；照清單跑、每項逾時、失敗印 log、列出通過數與「有跳過項目的測試」、4 個 node 檢查（`check_deps_catches_a_violation` 要失敗才算通過）。`env.ps1` 改成 MSVC＋qmake＋jom。兩個腳本都是 UTF-8 with BOM（PowerShell 5.1）。
- `CompositionRoot.cpp` 的「沒有編進程式」訊息改成 qmake 的說法（保留 `AOI_WITH_OPENCV=ON`／`AOI_WITH_HALCON=ON` 字樣）；註解裡的 CMake／CTest／`aoi.exe`（`HalconRuntime.h`、`perf_halcon*.cpp`、`perf_opencv_real.cpp`、`tst_cli.cpp`、`tst_lattice_crosscheck.cpp`、`tst_lattice_explainer.cpp`、`tools/check-deps.mjs`）。
- 證明等價之後移除 CMake：21 個 `CMakeLists.txt`、`cmake/`（2 個）、`CMakePresets.json`。`.gitignore`（根目錄與 `AiInspTool/`）改成 qmake 的輸出與 Qt Creator 的快取。
- 文件：ADR-018（新）、ADR-003 與 ADR-017 的「後續」、`decisions.md`、`AI_CONTEXT.md`（技術棧、環境、目錄、建置與測試、VERIFIED、Context Routing）、`docs/architecture.md`（依賴改由 `aoi_deps.pri` 表達、模組表）、`README.md`、`docs/plan/S09`–`S12`（改掉 preset／label／CTest 的說法，加「建置改 qmake 之後的提醒」）、`docs/perf-test-plan.md`、`docs/00-master-plan.md`（一格）、`docs/current-status.md`。`docs/steps/S02`–`S08` 是歷史紀錄，不改（對照表在 ADR-018）。

### 修改的檔案
- 新增：`AiInspTool/.qmake.conf`、`AiInspTool/qmake/*.pri`（7 個）、`AiInspTool/src/**.pro`（15 個）、`AiInspTool/tests/**`（`tests.pro`、`support/test_support.pro`、59 個測試 `.pro`、5 個 `*_tests.pri`）、`tests/support/test_environment.cpp`、`docs/adr/018-qmake-only-build.md`
- 搬移：`AiInspTool/aoi_ux.qrc` → `AiInspTool/src/presentation/aoi_ux.qrc`
- 修改：`AiInspTool/AiInspTool.pro`、`AiInspTool/.gitignore`、`.gitignore`、`scripts/dev/env.ps1`、`scripts/dev/test.ps1`、`src/app/CompositionRoot.cpp`、上面列的註解、文件
- 刪除：`CMakeLists.txt`、`CMakePresets.json`、`cmake/AoiTargets.cmake`、`cmake/FindHalconLocal.cmake`、`src/**/CMakeLists.txt`（13 個）、`tests/**/CMakeLists.txt`（7 個）

### 驗證
- 8 種組合（`test.ps1` 的 Release／Debug × 兩個都關／`-NoHalcon`／`-NoOpenCv`／預設）移除 CMake 前後各一輪，全部通過：45／50／48／53（Release 與 Debug 相同，和 CTest 時代的數量一樣）。第一輪 HALCON 的測試在 Release、Debug 都有跑（29／12／9 項，0 跳過）；第二輪 Release 也有跑，**第二輪 Debug 的 HALCON 測試因為讀不到 dongle（錯誤 2300）整組跳過**。
- Qt Creator 的三種組態（Qt Creator 20 的 qmake 參數、建置資料夾 `AiInspTool/build/Desktop_Qt_6_12_0_MSVC2022_64bit_{Debug,Release,Profile}`）：建置成功、0 個警告、各 60 個執行檔與 OpenCV 的 DLL；旗標看過（Release／Profile 有 `-O2 -DNDEBUG`，Debug `-MDd` 沒有 `NDEBUG`）。Profile 跑全部測試：53／53（HALCON 的 3 個跳過，dongle）。
- 實際啟動：Release 開使用者的 `0_0_B.tif`（16 bit LZW）成功（截圖確認影像、偵測 12 bit），`--backend halcon` 時 stderr 有授權提醒（2026-10-07，dongle 還讀得到時）；Qt Creator 模擬的 Release：`--backend opencv` 開同一張 TIFF 成功，`--backend halcon` 在讀不到 dongle 時結束碼 2、原因寫在 stderr。`--help`、參數錯誤經管線執行時照常寫到 stdout／stderr。
- Qt Test 的純文字輸出不寫到被接走的 stdout，`-xml`、`-lightxml` 會寫（實測）。
- qmake 的錯誤：`AOI_OPENCV_DIR`、`AOI_HALCON_SDK_DIR` 指錯、`AOI_WITH_HALCON=no` 都以結束碼 3 停下並說明。
- `test.ps1 -Perf`（Release，單次）：10 項通過；T1 0.31、T2a 56.8、T4 29.0 ms、T6b 0.106 px、T3 0.48、U1b 3.9、U4-PNG 100、I1 6.9、U3 16.5（已知超標）、I3 25.9 ms，和 S05–S07 的文件同一個量級。`perf_halcon` 因為 dongle 跳過。預算沒有改。
- `node tools/check-deps.mjs`（0 違規）、`node tools/ste-lint.mjs`（0 error）、`node tools/check-contrast.mjs`（0 失敗）、`scripts/ai/check-ai-structure.ps1`（OK）。

### 未完成
- **HALCON 讀不到 dongle（錯誤 2300）時的驗證**：重插 dongle 後補跑 `test.ps1 -Config Debug -Filter 'tst_halcon*'`、`test.ps1 -Perf -Filter 'perf_halcon'`。
- 新的專案還沒有在 Qt Creator 的畫面裡打開過（只用命令列模擬同樣的步驟）。

### 注意事項
- 建置進行中不要改 `.pro`／`.pri`：Makefile 會觸發 qmake 重跑。
- `scripts/**/*.ps1` 要存成 UTF-8 with BOM；bash 的 heredoc 會吃掉反斜線，改 `.pri`、JSON 用檔案寫入。
- qmake 把 `#` 當成註解的開頭（引號裡也是）；`$$cat()` 的結果要先放進變數再 `for`。
- 不要把 `/IGNORE:4199` 拿掉（理由在 `aoi_target.pri`）；不要把測試清單改成看 `bin/` 的檔案。

## 2026-10-07 — Claude Code (Claude Opus 5.5)（S08：HALCON back end）

### 完成
- 依 Step Protocol 做 S08，在 worktree 的分支 `step/08-halcon`（從 `main` 的 `000aa36` 開出）。
- 開工時實測授權並向使用者說明：runtime（RTL，`3-7614034`）可用、SDK（`3-7081033`）的 dongle 沒插、`%HALCONROOT%` 沒有開發檔。使用者指示從 CV_Algorithm 專案複製開發檔，並要求分辨兩種 dongle、授權錯了要提醒。
- `third_party/halcon/`（24.11.2 的標頭＋`halconcpp.lib`，`-text`、`.gitignore` 例外、`README.md`）；`cmake/FindHalconLocal.cmake`（`Halcon::HalconCpp`、`/DELAYLOAD:halconcpp.dll`、`NOMINMAX`）；preset `release-halcon`／`debug-halcon`（OpenCV＋HALCON）；`test.ps1 -Preset release-halcon`。
- HALCON back end（`src/infrastructure/backend_halcon/`，ADR-017）：範本建立器（`create_shape_model`）、搜尋器（`find_shape_model`，視窗清單 ＝ 一個聯集的區域）、樣本庫比對器（一次 `find_shape_models`）、`HalconImages`（緊密排列零複製）、`HalconPose`（位姿換算）、`HalconCancelWatch`（`interrupt_operator`）、`HalconRuntime`（延遲載入的保護、版本）、`HalconLicense`（探測、判斷、提醒）、`HalconErrors`、`HalconSearchSteps`、`HalconBackend`（工廠、`setHalconThreads`）。
- 運算子的行為查 `%HALCONROOT%\help\operators_en_US.ref` 與執行時的 `get_operator_info`（`find_shape_model(s)` 可中斷、'no_restriction'；`create_shape_model` 不可中斷）。
- 實測後改的：'auto' 的角度步進（48×28 給 5.2°，±5° 只剩三個角度）→ 角落每步 0.5 px＋模型範圍兩端多 2 步；位姿要用 `affine_trans_pixel` 的修正（測試證明）；`Gray16` 換 8 bit 由逐像素（213 ms）改查表（8–11 ms）；次像素預設 `least_squares`（試過 `_high`）。
- 組裝根：halcon 的登記（PATH 上有 DLL 才可用、授權在工廠裡）、`backendNotices`；main：提醒寫 stderr（fflush）、吐司、選單提示；`MainWindow::setBackends` 改成 `BackendChoice`（不可用的停用、加 ✕、提示原因）；`copy.json` 新增 2 個 key。
- 測試：`tst_halcon_backend`（29 項）、`tst_halcon_end_to_end`、`tst_halcon_library_matcher`、`perf_halcon`、`perf_halcon_real`；`tst_cli`（有授權啟動並提醒、沒有授權結束碼 2、沒有 DLL 時 fake 照常）、`tst_backend_registry`、`tst_shell`（選單）。
- 文件：`docs/steps/S08-halcon/`（四種輸出＋`perf.md`）、ADR-017、ADR-010／014 的「後續（S08）」、`decisions.md`、`architecture.md`、`AI_CONTEXT.md`、`README.md`、`docs/plan/S09`–`S12` 的「S08 完成後的提醒」；`perf/budgets.json`（T2b calibrated、`searchCalibrationS08`，limit 沒改）、`perf/baseline/casparhsu1-halcon.json`。

### 修改的檔案
- 新增：`src/infrastructure/backend_halcon/`（24 個檔案）、`cmake/FindHalconLocal.cmake`、`third_party/halcon/`、`tests/backend/{tst_halcon_backend,tst_halcon_end_to_end,tst_halcon_library_matcher,perf_halcon,perf_halcon_real}.cpp`、`docs/steps/S08-halcon/`、`docs/adr/017-halcon-shape-model-backend.md`、`perf/baseline/casparhsu1-halcon.json`
- 修改：`CMakeLists.txt`、`CMakePresets.json`、`cmake/AoiTargets.cmake`、`.gitattributes`、`.gitignore`、`scripts/dev/test.ps1`、`src/infrastructure/CMakeLists.txt`、`src/app/{CMakeLists.txt,CompositionRoot.h,CompositionRoot.cpp,main.cpp}`、`src/presentation/shell/MainWindow.{h,cpp}`、`tests/backend/{CMakeLists.txt,BackendTestSupport.h}`、`tests/core/{CMakeLists.txt,tst_backend_registry.cpp,tst_cli.cpp}`、`tests/ui/tst_shell.cpp`、`docs/ux/copy.json`（只新增 key）、`perf/budgets.json`、`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/adr/010-*`、`014-*`、`docs/plan/S09`–`S12`、`.ai/handoff.md`

### 驗證
- CTest：Release 45、Release＋OpenCV 50、**Release＋OpenCV＋HALCON 53**、**Debug＋OpenCV＋HALCON 53**、**只有 HALCON（`-DAOI_WITH_HALCON=ON -DAOI_WITH_OPENCV=OFF`）48** 個項目全過。三個 HALCON 組態是 dongle 恢復後、在最後的程式碼上跑的（HALCON 的測試 0 跳過：29／12／9 項），之後只改了兩處註解，重建並重跑 HALCON、`tst_cli`、`tst_backend_registry`、`tst_shell` 共 6 個項目。Release 與 Release＋OpenCV 是在授權探測與一個 HALCON 測試修改之前跑的：那兩個改動只在 HALCON 的組態編譯。
- 降級（手動與 `tst_cli`）：`HALCONROOT` 指到空資料夾 → `--backend=halcon` 結束碼 2、錯誤 2036；PATH 上沒有 HALCON → halcon 登記成不可用、`--backend=fake` 照常啟動；有授權時 `--backend=halcon` 啟動，stderr 有「SDK 的 dongle 沒插」的提醒。
- `node tools/check-deps.mjs`：0 違規；`--self-test` 15 個 fixture 全過。`node tools/ste-lint.mjs --strict`：0 錯誤、0 警告。`diagram.md` 8 張 Mermaid 用 mermaid 11（jsDelivr，瀏覽器窗格）解析全過，故意寫壞的一張被抓到。`explainer.html`：滑桿、勾選框、主題都操作過，0 個 console 訊息，375 px 寬沒有水平捲動。
- 效能：`-Preset release-halcon -Perf` 連跑 3 次（`AOI_REAL_IMAGE_DIR` 有設）＋`perf_halcon` 補跑 1 次。T1、T4、T6a、T6b 3 次都在預算內；**T2b 3 次中 1 次超過**（p95 82.2 ms，那一次整台機器變慢：S02 的 U4-PNG 也超過、`perf_ui_baseline` 300 秒逾時；第 2、3、4 次 29–35 ms）。U3 仍超標（S04 起）。
- 變異：12 個（角度正負號、沒有旋轉修正、沒有角度餘量、'auto' 步進、不用視窗過濾、拿掉 NMS、提醒規則反過來、Gray16 不計數、沒有取消監看、拿掉 Matching 檢查、視窗不往外擴、角度不夾回範圍），抓到 11 個。第一輪有 3 個因為 `/WX`（變數沒用到）編譯失敗，改成保留變數的寫法重跑；「不用視窗過濾」第一次存活，補了「元件中心在視窗外 1.5 像素」的測試後抓到；「拿掉 domain 的 NMS」存活：同一次 `find_shape_model` 裡 HALCON 自己的 MaxOverlap 已經去掉重複，測過的情境等價，NMS 留著當契約的防禦。
- `AiInspTool.pro`：Qt 6.12.0 的 qmake ＋ jom（Release）建置成功；offscreen 下 `--backend=halcon` 結束碼 2、訊息說要用 `AOI_WITH_HALCON=ON`，`--demo 500` 啟動 4 秒仍在執行。**沒有在 Qt Creator 裡載入過。**
- **真實的「讀不到 dongle」**：驗證途中（12:04）HALCON 突然讀不到**一直插著**的 RTL dongle（錯誤 2300；Windows 仍認得 `USB\VID_064F&PID_2AF9\000007614034`、狀態 OK；`hhostid` 看不到它；沒有行程載入 HALCON）。使用者重插後約 12:25 恢復，**原因不明**。那段期間 `--backend=halcon` 結束碼 2、原因寫清楚，HALCON 的 `tst_*` 跳過並說原因（不是失敗）。因此加了「沒有授權時自己讀 `%HALCONROOT%\license\*.dat`」，訊息寫出每個授權檔需要哪個 dongle。這台電腦另外裝了 HALCON 22.11（使用者的 AppData，授權是過期的試用版）；`HALCONROOT` 指 24.11，所以 22.11 的 HDevelop 讀 24.11 的授權檔，報 2041 Invalid host。

### 未完成
- HALCON 的影像來源（`read_image`）：沒有做，等使用者決定。`create_ncc_model` 的對照：沒有做（plan 寫「可選」）。
- 流程 B 的 UI（使用者決定之後再接）；兩個 back end 的分數門檻要不要分開（S09 一起看）。
- 契約測試的泛化（S09）。

### 注意事項
- **HALCON 的 'auto' 角度步進不能用在小樣本**；`HalconSearchOptions` 的 `cornerShiftPx`、`angleMarginSteps` 是實測定的（`perf.md` 的 AS-*）。
- **位姿換算不是單純 ±0.5**（`HalconPose.h`、ADR-010 的 S08 後續）。
- `halconcpp.dll` 延遲載入：任何 HALCON 呼叫之前要先過 `halconRuntimeProblem()`；組裝根只 include 不帶 HALCON 標頭的三個檔案。
- **PowerShell 的 `Get-Content -Raw`／`Set-Content` 把 UTF-8 檔當 CP950 讀壞過一次**（`HalconSearchOptions.h`，立刻用編輯工具整份重寫）；改檔一律用編輯工具。
- `QVERIFY2` 的訊息在通過時也會求值：`tst_cli` 一開始在訊息裡讀了 stderr，後面就讀不到了。
- 量測前 Visual Studio 自己更新到 17.14.37710（`cl` 19.44.35229）；更新期間 `vswhere` 找不到 VS、`env.ps1` 會失敗。
- 真實影像與疊圖不入版控；`.claude/launch.json`（瀏覽器窗格的暫時伺服器）已刪除。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S07：OpenCV back end）

### 完成
- 依 Step Protocol 做 S07，在 worktree 的分支 `step/07-opencv`（從 `main` 的 `5cf7748` 開出）。
- 開工時向使用者要真實影像，並說明「有效位元數覆寫」的設計、問了三個做法與流程 B 的範圍：使用者提供兩張去識別化影像（12 bit、24.4 MP，不入版控）；選了「存進板子、換影像沿用」「參數面板的下拉選單」「流程 B 只做 adapter＋測試＋T7，UI 先不接」。12 bit 的存法（16 bit 容器、值 0..4095）使用者在 S06 之後已回答。
- OpenCV back end（`src/infrastructure/backend_opencv/`，ADR-016）：`OpenCvTemplateBuilder`／`OpenCvTemplate`（金字塔 × 旋轉範本，含周圍影像轉）、`OpenCvPatternSearcher`（由粗到細的 NCC、視窗清單、爬坡、拋物線次像素與角度、domain 的 NMS）、`OpenCvCorrelation`（小的分數圖自己算）、`OpenCvPyramid`、`OpenCvSearchOptions`、`OpenCvLibraryMatcher`（`ILibraryMatcher`）、`OpenCvBackend`（工廠、`setOpenCvThreads`）；`OpenCvImages::toGray8`。組裝根在 `AOI_WITH_OPENCV=ON` 時註冊 opencv（`--backend opencv`），`app::workerThreads()` 給 JobRunner 與 OpenCV 共用。
- 第一版 T2a、T4 超過預算（193／87 ms 中位數）：分段計時發現精修的 5×5 分數圖 `matchTemplate` 仍走 DFT；改成整數點積＋積分影像後 64／30 ms，找到的元件完全相同。
- 有效位元數的覆寫：domain `Board::significantBits`、`decideSignificantBits`／`withSignificantBits`；application `InspectionDocument::setSignificantBits`（reset 保留）；storage 配方 schema v2（v1 → v2 遷移補 0；新增範例 `recipe-v2.json`）；presentation 參數面板「影像位元數」、`IParamsController`／`IParamsSink` 各加一個方法、Presenter 開圖與改設定時套用、比推測小時吐司；`copy.json` 新增 5 個 key。
- 測試：`tests/backend/`（`tst_opencv_backend`、`tst_opencv_end_to_end`、`tst_opencv_library_matcher`、`perf_opencv`、`perf_opencv_real`）；`tst_image_view`、`tst_document`、`tst_params`、`tst_presenter`、`tst_recipe_store` 加了位元數與 v2；`tst_backend_registry`、`tst_cli` 依建置選項檢查 opencv。
- 真實影像：HA 的種子分數分兩群（焊墊邊緣 0.70–0.85）、LA 的晶格在空槽補假缺格（S05 已知失敗 #11）——都在 use case／晶格，沒有改，列為使用者決定。
- 文件：`docs/steps/S07-opencv/`（四種輸出＋`perf.md`）、ADR-016、ADR-014／015 的「後續（S07）」、`decisions.md`、`architecture.md`、`AI_CONTEXT.md`、`README.md`、`docs/plan/S08`–`S12` 的「S07 完成後的提醒」；`perf/budgets.json`（T1、T2a、T4、T6a、T6b 標 calibrated，limit 沒改）、`perf/baseline/casparhsu1-opencv.json`。

### 修改的檔案
- 新增：`src/infrastructure/backend_opencv/OpenCv{Backend,Correlation,LibraryMatcher,PatternSearcher,Pyramid,Template,TemplateBuilder}.{h,cpp}`、`OpenCvSearchOptions.h`；`tests/backend/`（6 個檔案）；`tests/infra/fixtures/recipe-v2.json`；`docs/steps/S07-opencv/`；`docs/adr/016-opencv-coarse-to-fine-search.md`；`perf/baseline/casparhsu1-opencv.json`
- 修改：`src/domain/component.h`、`image_depth.{h,cpp}`、`image_view.h`（註解）；`src/application/document/InspectionDocument.{h,cpp}`；`src/infrastructure/backend_opencv/OpenCvImages.{h,cpp}`、`CMakeLists.txt`；`src/infrastructure/storage/JsonCodec.cpp`、`JsonRecipeStore.{h,cpp}`；`src/presentation/params/ParamsPanel.{h,cpp}`、`presenters/{AutoRoiPresenter,ParamsPresenter}.{h,cpp}`、`ControllerInterfaces.h`、`ViewInterfaces.h`、`shell/MainWindow.cpp`；`src/app/CompositionRoot.{h,cpp}`；`tests/CMakeLists.txt`、`tests/core/CMakeLists.txt`、`tst_backend_registry.cpp`、`tst_cli.cpp`、`tst_document.cpp`、`tests/domain/tst_image_view.cpp`、`tests/infra/RecipeTestSupport.h`、`tst_recipe_store.cpp`、`tests/ui/tst_params.cpp`、`tst_presenter.cpp`；`docs/ux/copy.json`（只新增 key）；`perf/budgets.json`；`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/adr/014-*`、`015-*`、`docs/plan/S08`–`S12`、`.ai/handoff.md`

### 驗證
- `scripts/dev/test.ps1`：Release 45、Release＋OpenCV 50、Debug 45、Debug＋OpenCV 50 個 CTest 項目全過（Release＋OpenCV 與 Debug＋OpenCV 在最終程式上重跑過；Release、Debug 在加角度掃描測試之前跑過，那兩個組態不建置 `tests/backend/`，之後改的只有 OpenCV 的搜尋器與它的測試）。
- `node tools/check-deps.mjs`：173 個檔案 0 違規；`--self-test` 15 個 fixture 全過。`node tools/ste-lint.mjs --strict`：0 錯誤。`diagram.md` 7 張 Mermaid 用 mermaid 11（jsDelivr，瀏覽器窗格）解析全過，故意寫壞的一張被抓到。`explainer.html` 在瀏覽器窗格：每個控制項操作過、0 個 JS 錯誤、375 px 寬沒有水平捲動。
- 效能：`-Preset release-opencv -Perf` 連跑 3 次全過（8 個 perf 執行檔；`AOI_REAL_IMAGE_DIR` 有設）。T1、T2a、T4、T6a、T6b 3 次都在預算內；U3 仍超標（S04 起）。
- 變異：14 個，抓到 13 個；M11（拿掉角度的爬坡）存活，加了 ±5°、±30° 的角度掃描仍存活（結果完全相同）。
- `AiInspTool.pro`：Qt 6.12.0 的 qmake ＋ jom（Release）建置成功；offscreen 下 `--backend opencv` 結束碼 2、訊息說要用 `AOI_WITH_OPENCV=ON`。**沒有在 Qt Creator 裡載入過。**

### 未完成
- 流程 B 的 UI（使用者決定之後再接）；`Backend` 沒有 `libraryMatcher` 欄位。
- 真實影像上的門檻與晶格問題（使用者決定）；T7 的預算（提案）。
- HALCON（S08）；契約測試的泛化（S09）。

### 注意事項
- **T6b 要扣掉樣本框中心與元件中心的固定偏移**（匹配的參考點是使用者框的中心）；不扣的話傾斜與抖動的板子多 0.3–0.6 px。
- 角度誤差約是第 0 層步進的 0.2 倍（NCC 對角度的曲線在真值兩側不對稱）；試過「每個角度用自己的次像素峰值」只改善 0.013°，沒採用。
- 真實影像的路徑在 `.ai/handoff.md`；**不要把影像或疊圖加進版控**。
- 寫程式時又踩到：bash 的 `node -e` 與 heredoc 會吃掉反斜線（`\n` 變成真的換行、`build\release` 的 `\r`）；含反斜線的編輯改用 Write 寫腳本檔再執行。
- `.claude/launch.json`（為了在瀏覽器窗格跑 Mermaid 檢查暫時建的）已刪除。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S06：影像緩衝、轉接器、配方與樣本庫儲存）

### 完成
- 依 Step Protocol 做 S06，在 worktree 的分支 `step/06-image-storage`（從 `main` 的 `79aa74b` 開出）。
- 影像的所有權規則定案（ADR-014）：像素交出去就不改、owner 讓它活著、子影像共用 owner、做不到零複製就計數；`Bgr24`、`Gray16`、`subView`；`ImageBuffer`（緊密排列、對齊 64 byte）；`countImageCopy`（全程式共用的 atomic，測試取差）。
- 新 adapter `infrastructure/image_qt`（Qt Core＋Gui）：`fromQImage`／`wrapAsQImage`／`toCanonicalQImage`、`QtImageSource`。`check-deps` 新增 `qt-gui` 類別（白名單）與這一層的規則，2 個新 fixture（共 15 個）。開工時向使用者說明了為什麼不用精靈產生的 `mainwindow.*`、為什麼要新的一層。
- `backend_opencv`：`OpenCvImages`（`fromMat`、`borrowAsMat`；拒絕包著外部記憶體的 Mat）、`OpenCvImageSource`（自己讀 UTF-8 路徑再 `imdecode`）。**OpenCV 4.9.0（vc16）第一次連結成功**；preset `release-opencv`／`debug-opencv`，`aoi.exe` 建置後複製 DLL，測試的 PATH 加上 OpenCV。
- 影像來源只交出三種正規格式（`Gray8`、`Gray16`、`Bgrx32`）；組裝順序 **Qt 在前、OpenCV 在後**（計畫寫 Qt 當後備；改的理由是複製次數，ADR-014）。開圖改走 `IImageSource`，訊號帶 `ImageView`；`ImageLoader`、`ImageViewAdapter` 刪除。
- **12 bit**（使用者中途指出影像也有 12 bit）：`ImageView::significantBits`、`domain/image_depth`（看最大值推測）、畫布轉 8 bit 顯示（計數）、use case 的對比檢查依位元數換算、參考影像存位元數、合成板子產生 12 bit。實測 OpenCV 讀緊密打包的 12 bit TIFF 會左移 4 位。
- storage（Qt Core）：`JsonCodec`（每個欄位都寫、讀得嚴格、錯誤帶 JSON 路徑）、`JsonFile`（格式標記、`schemaVersion`、逐版遷移、`QSaveFile` 原子寫入、略過 BOM）、`JsonRecipeStore`、`JsonLibraryStore`（一個檔案一層、id 永不重複使用、壞檔不覆蓋）、`LayeredLibraryStore`（**使用者決定照 TRI 分層**：專案 → 使用者 → 唯讀的系統；先另開 session 圖解我的建議與 TRI 的差異）。
- application：`ImageSourceChain`、`RecipeSaver`（「非同步寫檔」：同一個檔案依序寫、還沒開始的舊存檔被取代）。
- `tests/support/synthetic_board`（同 seed 同輸出、標準答案 JSON、PNG＋JSON 存檔；不傾斜時鎖住像素雜湊）。
- **Qt 6.12.0**：使用者中途更新 Qt，選擇 S06 就切換。preset 與 `env.ps1` 的預設改成 6.12.0；切換前在 6.8.0 跑了一次全部效能測試當對照，切換後同樣跑一次（`perf.md`）。6.12.0 棄用了 `QDragEnterEvent(QPoint…)`，`tst_shell` 改用 `QPointF`。
- 文件：`docs/steps/S06-image-storage/`（四種輸出＋`perf.md`）、ADR-014、ADR-015、`perf/baseline/casparhsu1-storage.json`、`docs/architecture.md`、`AI_CONTEXT.md`、`README.md`、`docs/plan/S07`–`S12` 的「S06 完成後的提醒」。

### 修改的檔案
- 新增：`src/domain/image_buffer.*`、`image_copy.*`、`image_depth.*`；`src/application/image/ImageSourceChain.*`、`document/RecipeSaver.*`；`src/infrastructure/image_qt/`、`storage/`、`backend_opencv/`；`src/presentation/canvas/ImageViewMetaType.h`；`tests/support/`；`tests/infra/`（含 `fixtures/recipe-v1.json`、`library-v1.json`）；`tests/domain/tst_image_view.cpp`；`tests/core/tst_image_chain.cpp`、`tst_recipe_saver.cpp`；`tests/core/fixtures/deps/image-qt-widgets/`、`storage-gui/`、`clean/src/infrastructure/image_qt/`；`docs/steps/S06-image-storage/`；`docs/adr/014-*`、`015-*`；`perf/baseline/casparhsu1-storage.json`
- 修改：`src/domain/image_view.h`、`CMakeLists.txt`；`src/application/ports/IImageSource.h`、`IRecipeStore.h`、`ILibraryStore.h`、`usecases/GenerateObjectsFromSample.cpp`、`CMakeLists.txt`；`src/infrastructure/CMakeLists.txt`；`src/presentation/shell/MainWindow.*`、`presenters/AutoRoiPresenter.*`、`canvas/ImageItem.*`、`canvas/ImageCanvas.h`、`CMakeLists.txt`；`src/app/CompositionRoot.*`、`main.cpp`、`CMakeLists.txt`；`CMakeLists.txt`、`CMakePresets.json`、`cmake/AoiTargets.cmake`、`scripts/dev/env.ps1`、`scripts/dev/test.ps1`；`tools/check-deps.mjs`；`tests/CMakeLists.txt`、`tests/core/CMakeLists.txt`、`QtTestPrinters.h`、`tst_generate_objects.cpp`；`tests/domain/CMakeLists.txt`；`tests/ui/PresenterRig.h`、`perf_ui_baseline.cpp`、`tst_presenter.cpp`、`tst_scene.cpp`、`tst_shell.cpp`；`AiInspTool/AiInspTool.pro`；`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/plan/S07`–`S12`、`.ai/handoff.md`
- 刪除：`src/presentation/shell/ImageLoader.*`、`ImageViewAdapter.*`（測試搬到 `tests/infra/tst_image_qt`）

### 驗證
- **Qt 6.12.0（最終版）**：`scripts/dev/test.ps1` 四種組態全過：Release 45、Release＋OpenCV（`-Preset release-opencv`）47、Debug 45、Debug＋OpenCV 47 個 CTest 項目。切換前在 Qt 6.8.0（12 bit 加入前）四種組態也全過（44、46、44、46 個項目）；Debug 兩種組態在 6.8.0 第一次有失敗，原因見注意事項。
- `node tools/check-deps.mjs`：158 個檔案 0 違規；`--self-test` 15 個 fixture 全過。`ste-lint --strict`：698 句 0 錯誤。`check-contrast`：0 失敗。`s00-demo-fixture.mjs --check`：一致。`diagram.md` 8 張 Mermaid 用 mermaid 11.17.2（jsdom）解析全過，只驗語法；先確認一張故意寫壞的圖會失敗。`check-ai-structure.ps1`：OK（0 警告）。
- 效能（`perf.md`）：`perf-opencv`（6 個 perf 執行檔）在 Qt 6.12.0 連跑 3 次全過，切換前在 6.8.0 跑 1 次當對照；配方的量測另外單獨跑 3 次（查一個 483 ms 的異常值）。有預算的指標除了 U3 都在預算內；`perf/budgets.json` 沒有改。
- 變異：31 個，抓到 30 個（M05 第一輪存活，補了 EXIF 方向的測試後抓到；M24 是等價變異）；在 Qt 6.8.0 的 `release-opencv` 建置上做，還原後用 SHA-256 確認。
- `AiInspTool.pro`：Qt 6.12.0 的 qmake ＋ jom（Release）建置成功；offscreen 下 `--backend opencv` 結束碼 2、`--demo 500` 啟動後仍在執行。**沒有在 Qt Creator 裡載入過。**
- `explainer.html`：S06 中在瀏覽器窗格操作過各個控制項；最後一次改資料（6.12.0 的數字）之後，只確認了載入、表格內容、沒有主控台錯誤、375 px 寬沒有水平捲動。

### 未完成
- HALCON 的 `HImage` ⇄ `ImageView` 與 `HalconImageSource`：這台機器沒有 HALCON 的 C++ 開發檔，沒有做（S08）。
- 存檔與開啟配方的 UI（使用者決定這一步不接；必要功能，記在待辦）；開啟配方還需要 v2 schema（影像路徑）。
- 分層樣本庫的組裝（各層的檔案放哪裡）；有效位元數的覆寫（配方或相機設定）。
- 灰階＋alpha 的 PNG 兩個來源是否一致沒有驗證；`Gray16` 的畫布繪製成本沒有量；開圖仍在 UI 執行緒。

### 注意事項
- **Qt 6.8.0 的 `qUncompress` 對被截斷的資料會撞上 `Q_UNREACHABLE`**（Debug 中止；Release 是未定義行為，只是碰巧沒事）。Debug 的 `tst_recipe_store` 抓到；參考影像因此改成不壓縮的 base64。Release 碰巧通過，再次證明 Debug 要跑。
- Debug＋OpenCV 第一次跑時有兩個「失敗」是時序造成的：建置開始時二進位檔還是舊的編碼器，範例檔在測試前已改成新格式。四種組態全部重建後全過。
- `tst_image_sources_agree` 的「16 階灰色表的 PNG」原本預期兩邊不一致（`QEXPECT_FAIL`），實測一致（XPASS）：Qt 的 `isGrayscale()` 對索引色的定義是「索引 ＝ 灰階值」，不是「每個顏色都是灰」。真正不一致的是 16 階灰色表的 **BMP**（OpenCV 的 BMP 解碼器看「每個顏色都是灰」），改成記住它。
- `-0.0` 存成 JSON 讀回是 `+0.0`；Qt 的大小寫折疊是簡單折疊（`ß` ≠ `ss`）；Windows 上同時存同一個檔案，有的換名會失敗（30 次約 7 次）：都寫進契約或註解。
- 寫文件與程式時踩到的坑：bash heredoc 會吃掉反斜線（`/\s+/` 變 `/s+/`，`check-deps` 的自我測試抓到）；JS 的 `String.replace` 會把替換字串裡的 `$$` 變成 `$`（`.pro` 的 `$$files` 被改壞，看 diff 發現）；`for (x : f().value())` 在 C++20 是懸空參照（測試讀到空的）。
- 變異測試腳本與 Mermaid 檢查腳本在 session 的暫存目錄，不在 repo；變異的還原用原始位元組並驗證 SHA-256。
- `perf_storage` 把測試影像（約 250 MB）寫在**目前目錄**的 `perf-images-s06/`：CTest 在建置目錄執行所以沒事，手動執行時要先切到建置目錄（這個 session 在 worktree 根目錄跑過一次，已刪掉）。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S05：晶格估計與補漏）

### 完成
- 依 Step Protocol 做 S05，在 worktree 的分支 `step/05-lattice`（從 `main` 的 `4097b02` 開出；使用者的提示寫 `5c8923f`，但 `main` 之後已多了〔適配寬度〕的合併）。
- `src/domain/lattice/`（domain 第一次有 .cpp，只用標準庫；`aoi_domain` 由 INTERFACE 改 STATIC）：`LatticeEstimator`（kNN 位移分群找 u、v，RANSAC 挑原點，範圍逐次加倍的逐步精修，連通，Lagrange 約化；`fitAll` 依序找多組）、`LatticePredictor`、`LatticeAccumulator`（O(1) 正規方程）、`NonMaxSuppression`（旋轉矩形的精確重疊＋空間雜湊）、`SpatialGrid`、`IVectorClustering`。
- 接受規則（ADR-013，寧可沒有晶格也不要錯的晶格）每一條都對應一個測試重現過的失敗：散佈的點湊成晶格、三排間距不同湊成假的二維、4 px 雜訊下「每隔一格」的粗晶格沒被標出、斜著穿過第二列的一排（說明頁的情境抓到）、一排加一個離群點、兩個同間距的陣列被接成一個。
- use case：`SimpleGrid` 刪除；種子先 NMS；缺格與已有候選重疊不補；由近到遠、上限內留近的；驗證找到的元件用 O(1) 重新擬合，推算的位置用修正後的。`presentation/` 沒改。
- T3 第一版 p95 3.3–3.6 ms 超標；分段計時後兩輪改寫熱迴圈（平方距離、先算好反矩陣的指派器、`fastRound`、密集的格子表、逐列掃描的連通、v 先從鄰居方向取）降到 0.48 ms；最終 3 次 p95 0.65–1.19 ms。
- 正確性：情境掃描 1 029 組；與 S00 示範頁的 JavaScript 交叉驗證 36 組（`tools/s00-demo-fixture.mjs` 產生 fixture，CTest 檢查是否過期）；說明頁的晶格資料由 C++ 匯出，CTest 檢查是否過期。
- 變異測試 32 個，最終全被抓到：第一輪 17 個抓到、6 個存活（規則彼此備援）、9 個無法編譯；補了 `tst_lattice_rules`、`tst_lattice` 的斷言、use case 的兩處測試，第二輪 15 個全抓到。
- 修了 S04 的 `tests/ui/tst_presenter.cpp` 兩個時間相依的斷言（見注意事項）。
- 文件：`docs/steps/S05-lattice/`（四種輸出＋`perf.md`）、ADR-013、`perf/baseline/casparhsu1-lattice.json`、`perf/budgets.json`（T3 `calibrated`、`searchCalibrationS05`，limit 沒改）、`docs/plan/S06`、`S07`、`S10`、`S11` 開頭的「S05 完成後的提醒」。

### 修改的檔案
- 新增：`src/domain/lattice/`（23 個檔案）、`tests/domain/`（`CMakeLists.txt`、`LatticeTestSupport.h`、`tst_lattice_scenarios`、`tst_lattice_parts`、`tst_lattice_rules`、`tst_nms`、`tst_lattice_crosscheck`、`tst_lattice_explainer`、`perf_lattice`、`fixtures/s00-demo-lattice.json`）、`tools/s00-demo-fixture.mjs`、`docs/steps/S05-lattice/`、`docs/adr/013-lattice-acceptance-rules.md`、`perf/baseline/casparhsu1-lattice.json`
- 修改：`src/application/usecases/GenerateObjectsFromSample.{h,cpp}`、`src/application/CMakeLists.txt`、`src/domain/CMakeLists.txt`、`tests/CMakeLists.txt`、`tests/core/CMakeLists.txt`、`tests/core/tst_generate_objects.cpp`、`tests/core/perf_core.cpp`（標籤）、`tests/ui/tst_presenter.cpp`、`AiInspTool/AiInspTool.pro`、`perf/budgets.json`、`AI_CONTEXT.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`（S02、S03 段落濃縮，行數回到上限內）、`docs/plan/S06`、`S07`、`S10`、`S11`、`.ai/handoff.md`
- 搬移：`tests/core/tst_grid.cpp` → `tests/domain/tst_lattice.cpp`（S03 的規格保留並擴充）
- 刪除：`src/application/usecases/SimpleGrid.{h,cpp}`

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 36 個 CTest 項目全過。Debug 第一次全套有 1 個失敗（`tst_presenter`，見注意事項），修正後重跑全過。
- 情境掃描：主張的範圍內 420 組全部完全正確，範圍外 609 組的失敗都安全；交叉驗證 36 組一致；變異 32 個全抓到（兩輪後用 SHA-256 確認 24 個原始檔與變異前相同）。
- `node tools/check-deps.mjs`：133 個檔案 0 違規；`--self-test` 13 個 fixture 全過。`ste-lint --strict`：615 句 0 錯誤。`check-contrast`：0 失敗。`s00-demo-fixture.mjs --check`：一致。`diagram.md` 6 張 Mermaid 用 mermaid 11.17.2（jsdom）解析全過，只驗語法；先確認一張故意寫壞的圖會失敗。`check-ai-structure.ps1`：OK（0 警告）。
- 效能：`perf_lattice` 與 `perf_core` 各連跑 3 次（Release，`perf.md`）：T3 p95 0.65–1.19 ms（預算 2 ms）。
- `AiInspTool.pro`：qmake ＋ jom（Release）建置成功；offscreen 下 `--backend opencv` 結束碼 2、`--demo 500` 啟動 4 秒仍在執行。**沒有在 Qt Creator 裡載入過。**
- `explainer.html` 在瀏覽器窗格（本機 HTTP）操作過：各個特殊情況、速度表；375 px 寬沒有水平捲動（窄寬度的表格欄寬有調整）。窗格這次是深色外觀；淺色這次沒有再看。

### 未完成
- T3 的預算沒有說是哪一種 1 000 點：散佈的點 p95 3.1–4.2 ms、缺件加離群點 p95 1.6–2.1 ms 不在預算裡；要不要算進 T3，由使用者決定。
- 晶格的門檻要用真實影像校準（S07）；非矩形陣列（L 形、環形）沒有測；信心與「建議檢查」沒有顯示在畫面上（沒有任何 plan 檔寫到）；多回合的漸進驗證等 S07 的數字。

### 注意事項
- 規則彼此備援：只看估計器的結果，拿掉一條規則常看不出差別（變異測試第一輪 6 個存活就是這樣）。改規則時跑 `tst_lattice_rules`，它直接呼叫內部的規則函式。
- `tests/domain/fixtures/s00-demo-lattice.json` 與 `explainer.html` 的 `lattice-data` 是產生出來的，不要手改（各自的 CTest 會說怎麼重新產生）。
- `tst_presenter` 的時間假設：測試台的忙碌門檻 40 ms、最短顯示 120 ms；搜尋超過 40 ms 時，「進入預覽的瞬間」狀態列還是「搜尋中…」（依設計）。S04 的兩個斷言在那一瞬間讀狀態列；S05 讓 Debug 從放開滑鼠到預覽變成 49–72 ms（含輪詢誤差；Release 26–44 ms），全套跑出一次失敗。60 ms 的固定延遲可以穩定重現（3／3 失敗），改成 `QTRY_VERIFY2` 後 Release、Debug 各 3／3 通過。沒有量 S04 版程式在 Debug 的時間。
- 量測完之後拿掉了 `perf_lattice.cpp` 裡一個無效的檢查（拿兩個用同樣步驟建的累加器互比）；被量的程式沒變，事後跑一次 T3 p95 0.73 ms。
- 這台機器同一份程式會飄 1.4–2 倍（這次 `perf_core` 第 3 次比第 1 次慢約 2 倍）。
- Mermaid 的檢查腳本與變異測試腳本都在 session 的暫存目錄，不在 repo。

## 2026-10-06 — Claude Code (Claude Opus 5.5)（S04 之後：〔適配寬度〕）

### 完成
- 使用者看到正方形影像載入後左右有大片空白，要求加「適配寬度」。新增 `ImageCanvas::fitWidth()` 與 `FitMode`（`None`／`Window`／`Width`）：寬度撐滿畫布（左右留 `fitMarginPx`），保留畫布中央那一列但上緣、下緣不離開邊界，影像比畫布扁時垂直置中；視窗大小改變時沿用同一列；縮放或平移後不再跟著。`isFitted()` 的意義不變（只代表適配視窗）。
- `MainWindow`：〔↔ 適配寬度〕放在「檢視」選單與上方工具列，快捷鍵 `W`，沒有影像時停用。`copy.json` 新增 `action.fitWidth`；`ux-spec.md` 第 8 節加一列。載入影像的預設仍是適配視窗（規格沒改）。
- 分支 `feat/fit-width`（從 `main` 開出）；使用者看過結果後同意 commit，並以 `--no-ff` 合併到 `main`（只在本機，沒有 push，沒有 tag）。

### 修改的檔案
- `src/presentation/canvas/ImageCanvas.{h,cpp}`、`src/presentation/shell/MainWindow.{h,cpp}`、`docs/ux/copy.json`、`docs/ux/ux-spec.md`、`tests/ui/tst_canvas.cpp`、`tests/ui/tst_shell.cpp`、`.ai/handoff.md`

### 驗證
- Release 與 Debug 各 29 個 CTest 項目全過；`tst_canvas` 新增 7 個測試、`tst_shell` 新增 1 個並擴充 3 個。
- 變異測試 8 個全被抓到（resize 重新讀中央列會偏移、沒有夾住上下緣、扁影像沒有置中、平移後還在適配、resize 不跟著適配、少了 `W`、沒有影像也能按、按鈕接到適配視窗）；還原後雜湊相同。
- `check-deps` 0 違規、`ste-lint --strict` 0 錯誤、`check-contrast` 0 失敗。
- 沒有跑 qmake ＋ jom（沒有新增檔案，`.pro` 用萬用字元）；沒有效能量測（只改適配時的轉換，繪製路徑沒動）；沒有在真實視窗裡手動操作過。

### 未完成
- 無。載入影像要不要記住上次的適配方式，沒有做（規格是載入時適配視窗）。

### 注意事項
- 適配寬度在 resize 時**不能**重新讀視窗中央的影像列：畫布的 resize 錨點是 `NoAnchor`，捲動值不變而視窗變大變小，重新讀的話每次 resize 都會偏一點（變異 F01 證明測試抓得到）。

## 2026-10-06 — Claude Code (Claude Sonnet 5.5)（S04：Presenter、互動狀態機、復原／重做）

### 完成
- 依 Step Protocol 做 S04，在隔離的 worktree 裡建立 `step/04-presenter`。基準：S03 的程式在這個 worktree 重新建置、全部測試通過。
- 互動規則放在沒有 Qt 的 application 層（ADR-011）：`InteractionStateMachine`（8 狀態、19 事件）、`InspectionSession`、`ProposalSession`（memento 式復原）、`InspectionDocument`（5 種 Command）、`UndoStack`（Command 不可變、深度 200、乾淨位置）。domain 新增 `box_expander.h`；infrastructure 新增 `QtThreadPoolExecutor`。
- presentation：`AutoRoiPresenter`（世代丟棄遲到的結果、忙碌提示、取消立即回 Idle）、`ParamsPresenter`（防抖）、`SceneBuilder`、`ObjectsModel`／`ObjectsPanel`、`ParamsPanel`、`MessageTexts`、`AppPresenters`；`MainWindow` 變純 View；`SelectTool` 拖曳移動與方向鍵微調；`ShellController` 移除。
- 量到 S02 的繪製假設不成立（「虛線與點線的框很少」）：預覽時拖分數門檻，上千個框一次變成點線，一個畫格要 24–289 ms。改成填色矩形（ADR-012），點線畫格由 289 ms 降到約 31 ms。
- 量到 U2 的 98 ms 瓶頸是物件表欄位的 `ResizeToContents`；改成固定寬度後約 6 ms。
- 變異測試：植入 41 個缺陷，最終全被抓到（過程在 `perf.md`）。第一輪 37 個中 31 個第一次抓到；補測試與換寫法後其餘抓到，其中 M18（遲到的成功結果）第一次補的測試仍抓不到，原因是測試台的假 back end 一看到取消旗標就回 `Cancelled`。
- `docs/steps/S04-presenter/`（四種輸出＋`perf.md`）、ADR-011、ADR-012、`perf/baseline/casparhsu1-presenter.json`；`perf/budgets.json` 標註 U2、I3、I4、I6 `calibrated`、U3 `measured-over-limit`（limit 沒改）；`docs/plan/S05`–`S07`、`S10`、`S11` 開頭各加一行「S04 完成後的提醒」。

### 修改的檔案
- 新增：`src/domain/box_expander.h`、`src/application/{document,interaction}/`、`src/infrastructure/threading/QtThreadPoolExecutor.*`、`src/presentation/{presenters,objects,params}/`、`src/presentation/shell/ImageViewAdapter.*`、`tests/core/`（`tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`、`tst_qt_executor`、`ProposalTestSupport.h`、`QtTestPrinters.h`）、`tests/ui/`（`tst_presenter`、`tst_objects_model`、`tst_params`、`tst_messages`、`tst_scene`、`PresenterRig.h`、`PerfSupport.h`、`perf_ui_s04`）、`docs/steps/S04-presenter/`、`docs/adr/011`、`012`、`perf/baseline/casparhsu1-presenter.json`
- 修改：`src/presentation/canvas/`（`BoxFrames`、`OverlayStyle`、`RoiLayerItem`、`ImageCanvas`、`ICanvasController`）、`src/presentation/shell/`（`MainWindow`、`MessageBar`）、`src/presentation/tools/`、`src/domain/geometry.h`（Windows 的 `min`／`max` 巨集）、`src/app/`（`CompositionRoot`、`main.cpp`）、各層的 `CMakeLists.txt`、`tests/ui/`（`tst_tools`、`tst_items`、`tst_shell`、`TestSupport.h`、`perf_paint_micro`）、`perf/budgets.json`、`docs/ux/ux-spec.md`、`docs/ux/copy.json`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/plan/S05`–`S07`、`S10`、`S11`、`AI_CONTEXT.md`、`README.md`、`AiInspTool/AiInspTool.pro`（只改註解）、`.ai/handoff.md`
- 刪除：`src/presentation/shell/ShellController.*`

### 驗證
- Release：`scripts/dev/test.ps1` 29 個 CTest 項目全過。Debug 也是 29 個全過（第一次有 1 個失敗：`tst_proposal_session` 的測試把指標指進暫存的 `objects()`，Release 碰巧通過，是測試的錯；已改成指向活著的物件後重跑）。
- `node tools/check-deps.mjs`：112 個檔案 0 違規；`--self-test` 13 個 fixture 全過。`ste-lint --strict`：549 句 0 錯誤。`check-contrast`：0 失敗。`diagram.md` 9 張 Mermaid 解析全過（只驗語法）。`check-ai-structure.ps1`：OK（0 警告）。
- 效能：`perf_ui_s04` 連跑 3 次（`perf.md`）：U2、I3、I4、I6 在預算內，**U3 超標**（5 832 個元件 p95 46–61 ms，預算 16 ms）。
- `AiInspTool.pro`：命令列 qmake ＋ jom（Release）建置成功；`AiInspTool.exe --backend opencv` 結束碼 2，`--demo 500` 啟動 4 秒仍在執行；沒有在 Qt Creator 裡載入過。
- `explainer.html` 在瀏覽器窗格實際操作過（狀態機、復原堆疊的接受／復原／重做流程、數字卡、窄寬度）；深色外觀沒有驗證（窗格對本機文件固定淺色）。

### 未完成
- U3 超標，處置由使用者決定（降低成本或重新定義預算）。預算數字沒有改。
- `LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）沒有任何 plan 檔涵蓋，需要使用者決定放進哪一步。
- 沒有量真實視窗；沒有量 Debug；文件沒有存檔（S06）；流程 B 的比對（S07）。

### 注意事項
- 變異測試腳本沒有留在 repo。還原要用 `WriteAllBytes` 並驗證雜湊：`Copy-Item` 會保留舊的修改時間，Ninja 不會重建，留下變異過的目標檔；一次還原失敗讓接著跑的兩個變異結果不可信，是靠雜湊檢查發現的。
- `tests/ui/PresenterRig.h` 的視窗閘門故意不看取消旗標（模擬不可中斷的函式庫呼叫），不要「修正」它。
- `perf_ui_s04` 的 `knownOver("U3")` 是已知超標的記錄，不是放寬預算；S11 處理 U3 後要拿掉。
- 這個 session 的 bash 工具有時無法解析 heredoc，`node -e` 會吃掉引號；產生腳本一律用 Write 工具寫檔再執行。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S03：系統核心）

### 完成
- 依 Step Protocol 做 S03，在隔離的 worktree 裡建立 `step/03-core`。基準：S02 的程式在這個 worktree 重新建置、5 個測試全過。
- domain 型別與座標慣例（`geometry.h`）、九個 port（含 S01 加的 `ILibraryStore`、`ILibraryMatcher`，契約寫在標頭）、`Result`／`Error`、`CancelSource`／`CancelToken`、`JobRunner`（latest-wins）、use case `GenerateObjectsFromSample` 與 `classify`、暫代的 `SimpleGrid`、`BackendRegistry`、組裝根與 `--backend`、假 back end、`PerfProbe`、`ThreadPoolExecutor`。
- `tools/check-deps.mjs`（掛 CTest）加 13 個 fixture 與 `--self-test`；`tst_cli` 從外面用真的行程驗證 `--backend`。
- ADR-003（Qt Test 統一）、ADR-010（座標與角度慣例，解決 ADR-006 未決 #5）；`docs/steps/S03-core/` 的四種輸出與 `perf.md`；`perf/baseline/casparhsu1-core.json`；`docs/plan/S04`–`S10` 開頭各加一行「S03 完成後的提醒」；`AiInspTool.pro` 同步納入新的層。
- 變異測試：植入 14 個缺陷，每一個都被對應的測試抓到（結果在 `perf.md`）。其中一個（`rotate()` 正負號反過來）只有 `tst_domain` 抓得到，因為其他測試的標準答案用同一個 `rotate()`。
- 量測發現：`PerfProbe` 關閉的成本量不出來；暫代的晶格擬合在約 4 900 點要 97–134 ms；取消延遲在 1 ms 輪詢的工作上是 Windows 計時器解析度的函數。
- 測試抓到／過程中踩到的錯：`explainer.html` 第一版把含單引號的錯誤訊息拼進 JavaScript 字串，在瀏覽器窗格才發現語法錯誤（改成走 JSON ＋ `esc()`）；`windows.h` 的 `min`／`max`／`near` 巨集弄壞 `perf_core`（改 `NOMINMAX`、改名）；Qt 在 Windows 上只有 stderr 是主控台時才寫 stderr，啟動錯誤原本用 `qCritical` 被管線接走就看不到（改寫到 stderr）。

### 修改的檔案
- 新增：`src/domain/`（`geometry.h`、`match.h`、`object_state.h`、`search_params.h`、`component.h`、`recipe.h`）、`src/application/`、`src/infrastructure/`（`backend_fake`、`perf`、`threading`）、`src/app/CompositionRoot.*`、`tools/check-deps.mjs`、`tests/core/`（含 `fixtures/deps/`）、`docs/steps/S03-core/`、`docs/adr/003`、`010`、`perf/baseline/casparhsu1-core.json`
- 修改：`src/domain/image_view.h`（新增 `umPerPixel`，預設 0）、`src/app/main.cpp`（`--backend`、啟動錯誤寫 stderr）、各層的 `CMakeLists.txt`、`tests/CMakeLists.txt`、`AiInspTool/AiInspTool.pro`、`AI_CONTEXT.md`、`README.md`、`docs/architecture.md`、`docs/decisions.md`、`docs/current-status.md`、`docs/plan/S04`–`S10`、`.ai/handoff.md`

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 17 個 CTest 項目全過（5 個 UI、8 個核心測試執行檔加 `tst_cli`、3 個 `check_deps*`）；`/W4 /WX` 無警告。
- `node tools/check-deps.mjs`：80 個檔案 0 違規；`--self-test`：13 個 fixture 全過；`node tools/ste-lint.mjs --strict`：488 句 0 錯誤；`check-ai-structure.ps1`：OK。
- `diagram.md` 10 張 Mermaid 用 mermaid 11 的解析器（jsdom）解析全過，只驗語法；先用一張故意寫壞的圖確認解析器會失敗。
- `explainer.html` 在瀏覽器窗格（本機 HTTP 伺服器）實際操作：四張卡片的每個按鈕與滑桿、淺色與深色、窄寬度；靜態快照模式不執行 script，所以要用 HTTP。
- `perf_core` 連跑 3 次（Release，量測衛生同 S02）。`aoi.exe --backend=opencv|halcon|turbo` 結束碼 2、訊息正確；預設與 `--backend=fake` 能啟動並持續執行。
- `AiInspTool.pro`：獨立目錄 qmake ＋ jom（Release）建置成功。**沒有在 Qt Creator 裡載入過。**

### 未完成
- ADR-009（UI 工具集重審）仍等使用者決定，與 S03 無關。
- 流程 B 只有 port 與型別；`SimpleGrid` 等 S05 取代；假 back end 不看影像；OpenCV／HALCON 的座標與角度轉換（ADR-010）未驗證。

### 注意事項
- 這個 session 在隔離的 worktree（分支 `claude/trusting-colden-06cdda`），`main` 在使用者的主工作目錄被檢出，所以「合併到 main」要在主工作目錄執行 `git merge`；細節見本步驟的收尾訊息。
- 用 `node -e '…'` 或 heredoc 寫含反斜線、中文或單引號的字串會被 shell 弄壞（這次踩到兩次：反斜線被吃、換行被塞進字串）；一律用 Write／Edit 工具寫檔。PowerShell 5.1 讀 `.ps1` 不認無 BOM 的 UTF-8，腳本裡不要放中文字串。
- `String.prototype.replace` 的替換字串裡 `$$` 會變成單一個 `$`（`AiInspTool.pro` 的 `$$PWD` 因此被弄壞過一次，已修）；程式化修改檔案時用函式型的替換。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（AiInspTool：qmake 專案，用 Qt Creator 開啟 S02 程式）

### 完成
- 使用者在 `AiInspTool/` 用 Qt Creator 建了 qmake 的 Qt Widgets 專案（第一次建的是 Qt Quick，建錯後砍掉重建）。把 `AiInspTool.pro` 改成直接建置 `../src` 的 S02 程式（domain、presentation、`app/main.cpp`，萬用字元納入，程式碼只有一份），新增 `aoi_ux.qrc` 內嵌 `tokens.json`、`copy.json`。精靈產生的 `main.cpp`、`mainwindow.*` 保留但沒有被使用。
- commit `fd899ce`（直接提交在 `main`，因為主工作目錄正開在 Qt Creator，切換分支會讓追蹤的檔案暫時消失）。清掉了使用者砍掉的舊檔留下的 intent-to-add 索引殘留（`CMakeLists.txt`、`Main.qml`）。
- 解釋了使用者的疑問：S 拖框只會顯示框與訊息列，**S02 還沒有建樣本與搜尋**（S04 起有假資料的完整流程，S07 才有真的搜尋）。

### 修改的檔案
- 新增：`AiInspTool/`（`AiInspTool.pro`、`aoi_ux.qrc`，以及使用者的精靈檔案 `.gitignore`、`main.cpp`、`mainwindow.cpp`、`mainwindow.h`、`mainwindow.ui`）
- 修改：`AI_CONTEXT.md`（目錄表）、`.ai/handoff.md`、`docs/changelog-ai.md`

### 驗證
- 在獨立的建置目錄用 qmake 加 jom 建出 `AiInspTool.exe`，沒有錯誤（55 秒）；離屏執行 `--demo 5000` 五秒沒有當機。
- **沒有在 Qt Creator 裡實際載入專案並執行**；Qt Creator 要手動「執行 qmake」才會讀到新的 `.pro`。

### 未完成
- 使用者的 `AiInspTool/.gitignore` 沒有涵蓋 `release/`、`debug/`、`.qtc_clangd/`，目前是未追蹤的建置產物（沒有納入 commit，也沒有改使用者的檔案）。
- S03 起 `src/` 新增 application、infrastructure 時，`AiInspTool.pro` 要跟著調整。

### 注意事項
- 這個 session 在隔離的 worktree，編輯工具被護欄擋在主工作目錄之外；放進 `AiInspTool/` 的兩個檔案是經使用者同意後用 `Copy-Item` 複製的（複製前確認使用者的 `.pro` 與備份相同）。
- `AiInspTool/` 不在五層之內，`tools/check-deps.mjs`（S03）不要掃它。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S02：Qt UI 骨架與影像畫布）

### 完成
- 依 Step Protocol 做 S02：CMake＋presets、`src/` 三層（domain 只有 `ImageView`）、主視窗與 dock 空殼、影像畫布（游標中心縮放、平移、適配、零複製）、批次繪製層與單一物件 Item、四個工具與註冊表、暫代 controller、固定高度的訊息列、tokens → QPalette／QSS、文案直接讀 `copy.json`、`FakeScene`、5 個 Qt Test 單元測試執行檔、效能基準線，以及四種輸出與 `perf.md`。
- 第一件事（工具鏈驗證）：VS 2022 17.14／`cl` 19.44、Qt 6.8.0、CMake 3.30.5、Ninja 1.12.1 實際 configure、build、執行；寫回 `AI_CONTEXT.md` 環境表。發現 HALCON 只有 runtime（沒有標頭與程式庫）、OpenCV 的 `OpenCV_DIR` 要指到 `build\x64\vc16\lib`。
- 效能：第一版（描邊）I1 p95 約 50 ms，量測找到瓶頸是描邊的光柵化，改成整數像素的填色細矩形（純 QPainter 微基準約快 4 倍）。U1a、U1b、U4、I2、I5 通過；I1 在預算邊緣（offscreen 13 次執行 p95 14.1–20.7 ms）、真實視窗 22.1 ms 超標。預算數字沒改。
- 更正原本的假設：批次繪製**不是** 5 000 框變順的主因（Item 不多時對平移縮放幀沒有幫助），價值在第一幀與 Item 很多時的 p95。
- 測試抓到：Rejected 點線原本完全畫不出來（Qt 不畫長度 0 的虛線段）；變異測試確認三個關鍵行為的測試有效。
- 發現並處理兩個環境問題：效能量測會被混合架構 CPU 的核心與電源狀態影響 2 倍（`MeasurementHygiene.h`）；繁中 Windows 下字碼頁不一致會讓 Ninja 掉標頭相依（`env.ps1` 對齊字碼頁）。
- 新增 ADR-007（文案來源）、ADR-008（訊息列固定高度）、ADR-009（待使用者決定：S02 實測後重審 UI 工具集）；ADR-001 補了實測結果。

### 修改的檔案
- 新增：`CMakeLists.txt`、`CMakePresets.json`、`cmake/`、`src/`、`tests/ui/`、`scripts/dev/`（`env.ps1`、`test.ps1`）、`perf/baseline/casparhsu1.json`、`docs/steps/S02-ui-shell/`、`docs/adr/007`、`008`、`009`
- 修改：`AI_CONTEXT.md`（環境表、目錄、建置與測試指令）、`README.md`、`docs/architecture.md`（模組表）、`docs/decisions.md`、`docs/adr/001-ui-toolkit.md`（補充）、`docs/current-status.md`、`.ai/handoff.md`、`docs/ux/tokens.json`（`sizes.messageBarHeight`）、`docs/ux/copy.json`（新增殼層 key）、`docs/ux/ux-spec.md`（修改紀錄）、`perf/budgets.json`（校準狀態與實測值，limit 沒改）

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 5 個測試執行檔全過；變異測試（縮放錨點、LOD 門檻、框夾限）都會被抓到。
- `node tools/ste-lint.mjs --strict`：434 句 0 錯誤；`node tools/check-contrast.mjs`：全過；`diagram.md` 4 張 Mermaid 用 mermaid 11 解析器全過（只驗語法）；`scripts/ai/check-ai-structure.ps1`：OK。
- 效能基準線：offscreen 全套（量測衛生開啟，多次執行）、真實視窗子集、純 QPainter 微基準，結果在 `docs/steps/S02-ui-shell/perf.md` 與 `perf/baseline/casparhsu1.json`。
- `-DAOI_WITH_HALCON=ON`：缺標頭時 configure 明確失敗；`-DAOI_WITH_OPENCV=ON`：找到 4.9.0（只 configure，沒有連結）。

### 未完成
- 等使用者決定 ADR-009（I1 與 UI 工具集）。
- 拖曳移動元件、方向鍵微調、設定記憶、減少動態、高 DPI 螢幕實測：沒有做。U2、U3、I3、I4、I6：S04。
- 暫代品：`ShellController`（S04）、`ImageLoader`（S06）、`ui.shell.*` 文案（S04）。

### 注意事項
- 一定要在 `scripts/dev/env.ps1` 之後 configure 與 build；Bash heredoc 會壓縮連續反斜線（寫含反斜線的內容用 Write／Edit）。
- 效能數字只在同一台機器、同樣的量測衛生下可比較；同一份程式不同時間會飄 ±30%，回歸閘門（S10）要考慮。
- `docs/ux/tokens.json`、`copy.json` 改了要重建（內嵌進執行檔）。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S01：UX 規格與可點擊原型）

### 完成
- 依 Step Protocol 做 S01（設計步驟，沒有 Qt 程式）：UX 規格、狀態語言 token（含顏色對比驗算工具）、文案（繁中＋英文）、ADR-001／002／006、可點擊原型，以及四種輸出與 perf.md。
- 使用者看原型後把範圍擴大：加入樣本庫與區域掃描（流程 B）、Board › Component › Box 階層、元件類型編輯器（矩陣、相依窗、套用範圍）、CAD 清單。對照了使用者提供的三份 TRI 文件。
- 使用者回饋「訊息蓋住料件」：所有要閱讀的訊息改放畫布下方的訊息列。
- 使用者決定庫與板子是複本關係、庫存 µm；原型隨之改成複本語意（〔從樣本庫更新〕、〔存入樣本庫〕）。
- 使用者找不到「存入樣本庫」的按鈕（原本只有 Enter 鍵、按鈕藏在 dock 下方）：預覽時在訊息列加上〔接受〕〔接受並存入樣本庫…〕〔捨棄〕，並修正說明文字的順序。
- 使用者問「樣本庫很多不是要比很久」：新增「比對範圍」（先比板子已有的類型，找不到再擴大）與狀態列「比對 N 個樣本」；預篩選等寫入 ADR-006 §7，未量測。

### 修改的檔案
- 新增：`docs/ux/`（ux-spec.md、tokens.json、copy.json）、`docs/adr/001-ui-toolkit.md`、`002-undo-model.md`、`006-sample-library-and-hierarchy.md`、`docs/steps/S01-ux-spec/`（spec.md、diagram.md、explainer.html、storyboard.md、perf.md）、`tools/check-contrast.mjs`
- 修改：`docs/decisions.md`、`docs/current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md`、`docs/00-master-plan.md`、`perf/budgets.json`（記錄 UI 預算確認與三點定義）、`docs/plan/S02`–`S10`、`S12`（開頭各加一行「S01 修訂」）

### 驗證
- `node tools/ste-lint.mjs --strict`：386 句，0 錯誤，0 警告；負向測試（長的被動句）會失敗。
- `node tools/check-contrast.mjs`：淺色 18、深色 18、疊圖 24 組全過；負向測試（壞的 token）會失敗。
- `diagram.md` 的 12 張 Mermaid 圖用 mermaid 11 的解析器（jsdom）解析全過，只驗語法，沒有渲染；解析器對壞語法會失敗。
- 原型頁內腳本，對最終版：整套 124 項斷言全過，另有比對範圍約 20 項，主控台無錯誤。涵蓋流程 A／B／C、三個範圍的復原、複本語意、11 種原因中的 9 種無匹配、三種錯誤、忙碌 200 ms／進度 1 s 的時機、把手 24 px 點擊區正反面、CAD 清單、訊息不蓋在影像上、淺深色、繁中／英文、無非法轉移。
- 建置、單元測試、`check-deps`：不適用（尚未建立）。效能：全部未量測。

### 未完成
- ADR-006 未決項目（見 `docs/current-status.md`）。
- S02 由新的 session 做（Start prompt 在 `docs/plan/S02-ui-shell.md` 底部）。

### 注意事項
- 過程中抓到並修正的真實缺陷：接受後復原再重做會先重做殘留動作；畫面快取鍵漏看群組內容，編輯樣本庫後「與樣本庫不同」的提示不會出現；框到多個元件時的提示是錯的（舊提示說「沒框到元件」）。
- 測試失敗多半是測試腳本自己的錯（期望值、順序、沒先選取群組），已改正重測；報告裡的數字是改正後對最終版的結果。
- 真實滑鼠／鍵盤／滾輪的抽查只在第一版原型做過；新版是合成事件加截圖。
- `main` 已在另一個工作樹 checkout，合併要在那邊做。
- 使用者的 S01 提示寫「commit 範本在 plan 檔、用 `-m`」，與 `project-rules.md` 的格式規則衝突；問過一次，使用者沒回答格式，只說 commit。我選了 `project-rules.md` 的格式（`[Caspar]docs:`、`git commit -F` 讀檔），因為那是使用者稍早確認的；不合要求可以 amend（沒有 push）。
- 合併是在 `main` 的工作樹（`D:/AOI_Project/Qt/AutoCadCreated`）用 `git -C` 做的，因為 `main` 在那邊已 checkout；做之前確認它是乾淨的。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 依使用者決定，把 `ai-project-template/`（共用結構範本、套用腳本、設計說明，共 18 個檔案）納入版控，放在這個 repo 內。這個範本是同一個 session 依使用者「設計共用結構」的要求產生的，並非產品計畫的一部分。
- 更新文件中「未追蹤、不要 stage」的說法：`AI_CONTEXT.md`、`project-rules.md`、`current-status.md`、`handoff.md`。`current-status.md` 原本寫「非本計畫產生」，造成誰做的、為什麼做的看不出來，改成寫明來源。

### 修改的檔案
- 新增：`ai-project-template/`（18 個檔案）
- 修改：`AI_CONTEXT.md`、`.agents/harness/project-rules.md`、`docs/current-status.md`、`.ai/handoff.md`、`docs/changelog-ai.md`

### 驗證
- `git add -n` 預覽：18 個檔案，沒有被 ignore 的內容；掃描沒有密鑰或機器路徑（只有規則裡拿 `HQ\Caspar.Hsu` 當作者名字的範例）。
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤（它只掃 `docs/`，不含範本）。
- 用 repo 內這份範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過。

### 未完成
- 已合併的分支（`chore/ai-agent-structure`、`chore/commit-format`）與備份分支 `backup/before-commit-reformat` 還沒清。

### 注意事項
- 範本放在產品 repo 內但與產品無關。它底下的 `template/CLAUDE.md`、`template/AGENTS.md`、`template/.claude/settings.json` 是給「新專案」用的檔案，不是這個 repo 根目錄的設定；agent 在這個 repo 工作時，以根目錄的 `AGENTS.md`、`CLAUDE.md` 為準。
- 若日後要把範本搬離這個 repo，同步更新 `AI_CONTEXT.md` 等引用它的文件。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把前三個 commit 的訊息改寫成使用者的 `[作者]屬性:` 格式（檔案內容、作者、時間不變；新編號 `fd4fd2a`、`13230f8`、`74272a2`）。改寫前的完整歷史留在分支 `backup/before-commit-reformat`。
- `project-rules.md` 的 Commit 規則由 Conventional Commits 改為該格式，並訂為唯一權威；Step Protocol 第 5 步同步調整。訊息一律寫成檔案再 `git commit -F`，`AI_CONTEXT.md` 的 shell 備註同步修改。
- ADR-000 補上這次格式變更的說明。
- 依使用者要求以 `--no-ff` 合併到 `main`，沒有 push。

### 修改的檔案
- `.agents/harness/project-rules.md`、`AI_CONTEXT.md`、`docs/adr/000-ai-agent-structure.md`、`docs/changelog-ai.md`、`.ai/handoff.md`、`docs/current-status.md`
- 範本 `ai-project-template/`（未追蹤，不在 commit 內）：`project-rules.md` 改成同一份預設，README 更正「commit 格式屬 AOI 專屬」的說法。

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤。
- 用修改後的範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過，新的 Commit 訊息一節已帶入。
- 新 commit 訊息以腳本逐項檢查格式（標題、`測試 :`、Co-Authored-By、結尾兩行、每行寬度）。

### 未完成
- 12 份 `docs/plan/S*.md` 的「Commit 範本」仍是英文 Conventional 風格，沒有改寫；`project-rules.md` 規定它們只當內容參考，格式以規則為準。
- 確認改寫後的歷史沒問題後，刪除備份分支 `backup/before-commit-reformat`。

### 注意事項
- 結尾的 `[Test]` 與 `Build OK, Test OK` 是使用者的固定規則；專案目前沒有建置，所以它不代表本次有建置。實際驗證只看 `測試 :` 區塊，且必須如實。
- 兩份規範衝突時（repo 內文件與使用者慣例），先問使用者，不要自己挑。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把 AI 共用結構（Codex 與 Claude Code）套用到本專案，並與既有的 `CLAUDE.md` 合併：原內容搬到 `AI_CONTEXT.md`、`docs/architecture.md`、`.agents/harness/project-rules.md`，`CLAUDE.md` 改為薄入口。
- `docs/STATUS.md` 更名為 `docs/current-status.md`；14 個檔案（12 份 `docs/plan/S*.md`、`00-master-plan.md`、`S00-overview/spec.md`）的舊引用改成新路徑。
- 新增 `docs/adr/000-ai-agent-structure.md`；`docs/decisions.md` 作為 ADR 索引。
- `.gitignore` 補上 `.claude/worktrees/`、`CLAUDE.local.md`。

### 修改的檔案
- 新增：`AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/handoff.md`、`scripts/ai/`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`、`docs/adr/000-ai-agent-structure.md`
- 改寫：`CLAUDE.md`、`.gitignore`
- 更名：`docs/STATUS.md` → `docs/current-status.md`
- 引用更新：`docs/plan/S01`–`S12`、`docs/00-master-plan.md`、`docs/steps/S00-overview/spec.md`

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告（Windows PowerShell 5.1）。
- `node tools/ste-lint.mjs`：206 句，0 錯誤，0 警告。
- 舊名稱（`STATUS.md`、`CLAUDE.md 的…`）區分大小寫掃描：除了刻意保留的歷史說明，沒有殘留。
- Claude Code 實測：`AI_CONTEXT.md`、`project-rules.md`（含 Step Protocol）、`AGENTS.md`、`architecture.md` 都經 `CLAUDE.md` 的 `@` 匯入載入。
- 未驗證：Codex 端。這台的 Codex CLI 0.136.0 無法執行 `codex exec`（解析模型清單失敗），所以 `AGENTS.md` 的必讀清單流程沒有實測。

### 未完成
- 變更已 commit（`fd4fd2a`），並依使用者要求以 `--no-ff` 合併到 `main`。沒有 push，是否推到遠端由使用者決定。
- `ai-project-template/` 仍在 repo 內且未追蹤，不在這次 commit，要放哪裡由使用者決定。

### 注意事項
- 分層規則原本每個 Claude session 都載入，所以 `CLAUDE.md` 額外匯入 `docs/architecture.md`（`AGENTS.md` 也列入必讀）。不要移除，否則護欄會變弱。
- 舊版 `CLAUDE.md` 在 git 歷史（commit `28414b1`）。
- `.claude/settings.json` 的 deny 規則在 Claude Code 信任此資料夾後才生效。
