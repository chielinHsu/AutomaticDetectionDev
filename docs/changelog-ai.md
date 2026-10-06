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
