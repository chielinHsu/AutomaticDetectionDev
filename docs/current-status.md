# 目前狀態（進度表）

新 session 開工前先讀這份，再讀 `docs/plan/Sxx-*.md`。步驟完成後由該步驟的 session 更新。原檔名 `STATUS.md`，2026-10-05 更名為本檔以符合共用結構（見 `docs/adr/000-ai-agent-structure.md`）。各步驟的完整內容在 `docs/steps/Sxx-*/` 與 `docs/changelog-ai.md`，本檔只留重點。

- 更新日期：2026-10-08
- 階段：S00–S08 完成；**2026-10-07 建置改成只用 qmake**（S09 之前的 session A，[ADR-018](adr/018-qmake-only-build.md)；分支 `build/qmake-only` 以 `--no-ff` 合併到 `main`，沒有 tag）。**2026-10-08 S09 之前的 session B 完成**（授權提醒改成卡片＋狀態列的標示、back end 選單可以在執行中切換、介面的版面改用 Designer 的 `.ui`；[ADR-021](adr/021-runtime-backend-switching.md)、[ADR-022](adr/022-designer-forms.md)；分支 `feat/backend-switch-ui-forms` 以 `--no-ff` 合併到 `main`，沒有 tag）。**下一個是 S09**。S01–S08 都已用 `--no-ff` 合併到 `main` 並打 tag（`step-01-done`…`step-08-done`）；AI 共用結構、commit 格式規則與 S04 之後的〔適配寬度〕也已合併到 `main`（全部只在本機，**沒有 push**）。S06 起用 Qt 6.12.0。**等使用者決定的事**：（1）[ADR-009](adr/009-ui-toolkit-review-after-s02.md)：I1 在預算邊緣、真實視窗超標；（2）**U3 超出預算**（S04）；（3）T3 要不要涵蓋散佈的點（S05）；（4）**存檔／開啟配方的 UI 放進哪一步**（S06；分層樣本庫各層的檔案位置、流程 B 的 UI 也在那一步一起定）；（5）S06 的指標（開圖、存檔）要不要有預算；（6）S07：真實影像上 use case 的種子門檻與預設的分數門檻（0.70）偏低、晶格在空槽補假缺格、T7（流程 B）的預算（見「S07 的產出與發現」）；（7）**S08 新增**：HALCON 的影像來源（`read_image`）要不要做、授權提醒的規則是否符合期待、兩個 back end 的分數門檻要不要分開（見「S08 的產出與發現」）

| 步驟 | 層級 | 主題 | 狀態 | 分支 | tag |
|---|---|---|---|---|---|
| S00 | 基礎 | 版控 + 總體計畫 + 架構導覽 | ✅ 完成 | `step/00-master-plan` | `step-00-done` |
| S01 | 介面 | UX 規格與可點擊原型（含樣本庫與 Board › Component › Box 階層） | ✅ 完成 | `step/01-ux-spec` | `step-01-done` |
| S02 | 介面 | Qt UI 骨架與影像畫布 | ✅ 完成（⚠ I1 待使用者決定） | `step/02-ui-shell` | `step-02-done` |
| S03 | 系統 | 核心：ports、use case、DI、執行緒、PerfProbe | ✅ 完成 | `step/03-core` | `step-03-done` |
| S04 | 系統 | Presenter、互動狀態機、復原/重做 | ✅ 完成（⚠ U3 超標、待使用者決定） | `step/04-presenter` | `step-04-done` |
| S05 | 底層 | Domain：晶格估計與補漏（純 C++） | ✅ 完成 | `step/05-lattice` | `step-05-done` |
| S06 | 底層 | 影像緩衝、轉接器、Recipe 儲存 | ✅ 完成 | `step/06-image-storage` | `step-06-done` |
| S07 | 底層 | OpenCV back end | ✅ 完成（⚠ 真實影像的門檻與晶格待使用者決定） | `step/07-opencv` | `step-07-done` |
| S08 | 底層 | HALCON back end | ✅ 完成（⚠ 影像來源與授權提醒的規則待使用者確認） | `step/08-halcon` | `step-08-done` |
| S09 | 底層 | 契約測試與 A/B 比較 | ⏳ 下一個 | `step/09-contract-ab` | |
| S10 | 驗證 | 效能測試 harness 與回歸閘門 | ⬜ | `step/10-perf-harness` | |
| S11 | 驗證 | 效能優化（先量再改） | ⬜ | `step/11-optimize` | |
| S12 | 發行 | 匯出、打包、最終文件 | ⬜ | `step/12-release` | |

圖例：✅ 完成　⏳ 下一個　⬜ 尚未開始　⚠️ 有未解決問題

## S01–S05（2026-10-05／06）

- **S01**：`docs/ux/ux-spec.md`、`tokens.json`、`copy.json`；ADR-001（Widgets + QGraphicsView）、ADR-002（復原模型）、ADR-006（樣本庫與 Board › Component › Box）。範圍擴大成兩種流程並存（教樣本、框任意區域與樣本庫比對）。使用者決定：UI 預算照原數字；庫與板子是**複本**（TRI 的 Merge）；庫存 **µm**＋影像校正值；接受／捨棄後 `Ctrl+Z` 回 Preview；`Esc` 捨棄不確認；訊息不蓋在影像上。只有文件與原型的驗證，沒有建置。
- **S02**：CMake＋presets（`/W4 /WX`）、影像畫布（零複製）、批次繪製層、四個工具、訊息列、主題與文案、`FakeScene`；ADR-007、008、009。**I1（5 000 框平移縮放）在預算邊緣**：offscreen 13 次 p95 14.1–20.7 ms（預算 16.7）、真實視窗 22.1 ms（ADR-009 待決定）。量測要釘核心、關電源節流（`MeasurementHygiene.h`）；整數像素的填色矩形比 cosmetic 描邊快約 4 倍。
- **S03**：domain 型別與座標慣例（ADR-010）、九個 port、`Result`／`CancelToken`、`JobRunner`（latest-wins）、use case 與 `classify`、`BackendRegistry` 與 `--backend`、假 back end、`PerfProbe`、`tools/check-deps.mjs`；ADR-003。取消的延遲上限是工作兩次看旗標之間的最長時間（p95 約 16–17 ms，Windows 計時器解析度）。
- **S04**：互動規則在沒有 Qt 的 application 層（ADR-011）：`InteractionStateMachine`、`InspectionSession`、`ProposalSession`、`InspectionDocument`、`UndoStack`；`box_expander.h`；`QtThreadPoolExecutor`；`AutoRoiPresenter`、`ParamsPresenter`、物件樹、參數面板、`AppPresenters`；拖曳移動與方向鍵微調；ADR-012（虛線畫成填色矩形）。效能：U2、I3（餘裕只有 16–38%）、I4、I6 在預算內；**U3 超標：5 832 個元件 p95 46.4–60.7 ms（預算 16）**，成本是整個畫布的重繪，不是重新過濾。之後依使用者要求加了〔適配寬度〕（`W`，分支 `feat/fit-width`，沒有 tag）。
- **S05**：domain 第一次有 .cpp（`src/domain/lattice/`）：`LatticeEstimator`、`LatticePredictor`、`LatticeAccumulator`（O(1)）、`NonMaxSuppression`、`SpatialGrid`；ADR-013（寧可沒有晶格也不要錯的晶格）；`SimpleGrid` 刪除。1 029 種情境掃描、與 S00 示範頁交叉驗證 36 組一致。**T3（1 000 點）p95 0.65–1.19 ms，預算 2 ms，通過**；散佈的點 p95 3.1–4.2 ms 不在預算裡（待決定）。門檻要用真實影像校準（S07）；非矩形陣列沒有測。

## S06 的產出與發現（2026-10-06）

- **做了什麼**：`ImageView` 的所有權規則定案（ADR-014）：像素交出去就不改、owner 讓它活著、子影像共用 owner、做不到零複製就計數（`countImageCopy`）；`Bgr24`、`Gray16`、`significantBits`、`subView`、`ImageBuffer`。新 adapter `infrastructure/image_qt`（Qt Core＋Gui；`check-deps` 新增 `qt-gui` 類別）、`backend_opencv`（**OpenCV 4.9.0 第一次連結成功**，preset `release-opencv`／`debug-opencv`）。影像來源只交出 `Gray8`、`Gray16`、`Bgrx32`；組裝順序 **Qt 在前、OpenCV 在後**（計畫寫 Qt 當後備，改的理由是複製次數）。開圖改走 `IImageSource`（`ImageLoader`、`ImageViewAdapter` 刪除）。storage：JSON 的配方與樣本庫（ADR-015：格式標記、`schemaVersion`、逐版遷移、原子寫入、讀得嚴格、id 永不重複使用）；`RecipeSaver`（背景存檔）；`ImageSourceChain`；合成板子產生器（`tests/support/`）。細節：`docs/steps/S06-image-storage/`。
- **使用者在 S06 的決定**：（1）樣本庫照 **TRI 的分層**：專案 → 使用者 → 唯讀的系統（`LayeredLibraryStore`）；（2）**這一步不接存檔／開啟配方的 UI**，但它們是必要功能（見待辦）；（3）影像**也有 12 bit**：像素照原樣交給搜尋，畫面顯示前轉 8 bit，位元數看最大值推測；（4）**S06 就改用 Qt 6.12.0**。
- **效能（Qt 6.12.0，Release，連跑 3 次的範圍；細節見 `docs/steps/S06-image-storage/perf.md`）**：S06 的指標沒有預算。Qt 解碼常見格式 0 次複製、OpenCV 讀彩色檔 1 次（計數，單元測試斷言）。5 MP 開圖（中位數）：BMP 28.8–32.9 ms、PNG 92.0–120 ms、JPEG 44.7–54.5 ms；25 MP 的 PNG 451–564 ms。5 000 個元件的配方：讀 29–39 ms、存 93.5–99.3 ms（另有 1 次 483 ms，原因不明）、背景存檔佔 UI 0.35–0.38 ms。有預算的指標在 6.12.0 重量：U4（p95 BMP 33.5–47.7、PNG 141–160、JPEG 51.6–63.9 ms，預算 200）與其他都在預算內，**U3 仍超標**（p95 19.5–29.4 ms，預算 16）。6.8.0 對 6.12.0 只比了一次，看不出 Qt 版本的差別。**預算數字沒有改。**
- **要留意的事**：（1）開圖仍在 UI 執行緒解碼（25 MP 的 PNG 約 0.5 秒停住）。（2）已知例外：16 階灰色表的 BMP（Qt 給 `Bgrx32`、OpenCV 給 `Gray8`）；灰階＋alpha 的 PNG 沒有驗證。（3）有效位元數是推測的，沒有覆寫的設定（配方或相機）。（4）分層樣本庫（TRI 的規則已做好）還沒有組裝進程式：三層各是哪個檔案（路徑）還沒定，**使用者決定等接樣本庫 UI 的那一步，和存檔／開啟配方的 UI 一起定**（建議值在 ADR-015）；存一筆要整份重寫。（5）HALCON 的轉換在 S08 做了，HALCON 的影像來源沒有做。ADR-014、ADR-015；兩步的驗證紀錄在 `docs/changelog-ai.md`。

## S07 的產出與發現（2026-10-06）

- **做了什麼**：OpenCV back end（`src/infrastructure/backend_opencv/`，ADR-016）：`OpenCvTemplateBuilder`（金字塔 × 旋轉範本，含周圍影像轉、不留空白的角）、`OpenCvPatternSearcher`（由粗到細的 NCC：頂層全部角度 → 往下逐層精修 ± 2 像素、爬坡 → 第 0 層拋物線內插位置與角度 → domain 的 NMS；視窗清單只轉換、只掃視窗附近）、`OpenCvCorrelation`（小的分數圖自己算：整數點積＋積分影像）、`OpenCvLibraryMatcher`（`ILibraryMatcher`，流程 B；**UI 沒有接**，使用者決定）、`OpenCvBackend`（工廠、`setOpenCvThreads`）。組裝根在 `AOI_WITH_OPENCV=ON` 時把 opencv 註冊成可用（`--backend opencv`），執行緒數和 JobRunner 共用 `app::workerThreads()`（核心數 − 1）。**use case、port、晶格沒有為了 OpenCV 改**（開放封閉）。
- **使用者在 S07 的回答與決定**：（1）**12 bit 的影像存成 16 bit 容器、值 0..4095**（不是緊密打包的 TIFF）；（2）**有效位元數的覆寫在 S07 做**，做法由使用者選：存在板子裡（`Board::significantBits`，**配方 schema v2**，v1 由遷移補 0）、**換影像沿用**、UI 是**參數面板的下拉選單「影像位元數」**（自動（偵測：N bit）／8／10／12／14／16 bit）；（3）流程 B 只做 adapter、測試與 T7，**UI 先不接**；（4）提供兩張去識別化的真實影像（**不入版控**；路徑在 `.ai/handoff.md`）。 **有效位元數**：domain 的 `decideSignificantBits`（0 ＝ 自動；**比推測的小就不收**，影像裡有值會被截掉）；套用只改 view 的欄位（零複製）。畫布、use case 的對比檢查、OpenCV 搜尋器都讀 view 的位元數，所以 Presenter 在開圖與改設定時套上就夠了。這是 S07 對 domain／application 唯一的修改（`Board::significantBits`、`InspectionDocument::setSignificantBits`），為了使用者指定的功能，不是為了 OpenCV。
- **效能（3 次，細節見 `docs/steps/S07-opencv/perf.md`）**：T1 p95 0.34–0.47 ms（預算 30）、**T2a** 5 MP 種子搜尋 p95 **67–72 ms**（預算 150）、**T4** 200 個視窗 p95 **31–33 ms**（預算 60）、**T6a 召回率 100%**（6 673／6 673）、**T6b** 位置誤差 p95 **0.106 px**（預算 0.5）。第一版的 T2a、T4 超過預算：精修的小分數圖 `matchTemplate` 仍走 DFT，改成自己算之後才通過（T2a 193 → 64 ms，ADR-016）。use case 整條（5 MP）67–70 ms；角度 ±180° 約是 ±5° 的 4 倍；25 MP 種子搜尋 0.27–0.29 秒；流程 B 每個候選類型 8–9 ms（T7，線性）。U3 仍超標（S04 起）。**limit 一個都沒改**，T1、T2a、T4、T6a、T6b 標 `calibrated`。
- **真實影像的發現（要使用者決定）**：（1）HA（高角度光）的種子分數分兩群：≥ 0.85 幾乎都是針腳，0.70–0.85 多半是焊墊的邊緣；use case 的 `seedMinScore` 0.70 與預設的分數門檻 0.70 會把它們當成找到的，晶格因此只剩一維、信心 0.28。**要不要提高（例如 0.80–0.85）**？（2）LA（低角度光）的晶格在兩個空槽裡補了約 120 個假缺格（S05 已知失敗 #11，第一次在真實板子確認）。**要不要讓晶格不跨過空的區域**？兩件事都在 application／domain，S07 沒有改。整條 use case 在 24.4 MP 上 0.11–0.21 秒。
- **已知限制**：角度誤差約是第 0 層步進的 0.2 倍（48×28 的範本最大 0.42°，範本角落位移約 0.2 像素）；範本有一部分在影像外的元件找不到；強度相關法對光照、反光、縮放敏感；流程 B 的範本每次重建、只有尺寸的預篩選，小的類型可能在大元件的一部分上得到高分。 **新增設計決定**：ADR-016（OpenCV 搜尋）；ADR-014、ADR-015 各加「後續（S07）」一節（12 bit 的存法與覆寫、配方 v2）。`docs/plan/S08`–`S12` 開頭各加了「S07 完成後的提醒」。

## S08 的產出與發現（2026-10-07）

- **開工時的事實與使用者的決定**：這台機器的 HALCON 24.11.2 只有 runtime。實測授權：`license.dat` 是 runtime（RTL，CodeMeter `3-7614034`，插著），`license--DEVELOP-.dat` 是 SDK（`3-7081033`，**沒插**）；HALCON 用 runtime 授權照樣能跑、不報錯。使用者決定：（1）開發檔從 CV_Algorithm 專案複製到本專案（`third_party/halcon/`，24.11.2，標頭＋`halconcpp.lib`；那個專案也放在版控）；（2）**兩種 dongle 要能分辨，授權錯了要提醒**。
- **做了什麼**：HALCON back end（`src/infrastructure/backend_halcon/`，ADR-017）：`create_shape_model`／`find_shape_model` 的範本建立器與搜尋器、`find_shape_models` 的樣本庫比對器（流程 B，UI 仍不接）、`ImageView` ⇄ `HImage`（緊密排列零複製）、位姿換算、`interrupt_operator` 的取消、`set_system('thread_num', 核心數 − 1)`。`halconcpp.dll` **延遲載入**：沒有 HALCON 的電腦照樣啟動，halcon 在選單停用並說原因；`--backend=halcon` 在沒有 runtime 或授權時結束碼 2、原因寫到 stderr。**授權的判斷**（`judgeHalconLicense`）：不能用就 `BackendUnavailable`；能用但「用 runtime 授權、SDK 授權檔的 dongle 不在」→ 提醒（stderr、訊息列的吐司、back end 選單的提示；`copy.json` 新增 2 個 key）。back end 選單：不可用的項目停用、加 ✕、提示說原因。preset `release-halcon`／`debug-halcon`（OpenCV＋HALCON）。application、domain、port 沒有為了 HALCON 改。
- **ADR-010 的 HALCON 端實測並鎖住**：角度的正方向相同（只換單位）；位置要用說明裡 `affine_trans_pixel` 的修正（`x ＝ Column ＋ 0.5(sin＋cos)`、`y ＝ Row ＋ 0.5(cos−sin)`；±30° 時修正後平均誤差 0.018 px、沒修正 0.303 px）。 **HALCON 的 'auto' 角度步進不能用**：48×28 的樣本給 5.2°，±5° 只剩三個角度，傾斜 3° 的元件有一半回報 5°。改成「範本角落每步移動 0.5 px」＋「模型範圍兩端各多 2 步」（AS-* 的量測）。0° 附近 ±0.5° 的元件有一部分回報接近 0°（最大誤差 0.52°），每種設定都一樣，是 HALCON 本身的行為。 **先量再改**：`Gray16` 換 8 bit 的第一版逐像素 `lround`，5 MP 要 213 ms；改成查表約 8 ms。
- **效能與準確度（3 次，細節見 `docs/steps/S08-halcon/perf.md`；同一次也量了 OpenCV）**：T1 p95 7.0–22.0 ms（預算 30）、T4 p95 28.6–46.6 ms（預算 60）、**T6a 召回率 100%**、**T6b p95 0.48 px**（預算 0.5，很接近；OpenCV 0.11）——都通過。**T2b**（5 MP 種子搜尋，預算 80）p95 31.5、35.4 ms，**第 1 次 82.2 ms 超過**：那一次整台機器變慢（HALCON 的 T1、T7 與 OpenCV 的 T7 慢 1.5–2.4 倍，S02 的 U4-PNG 也超過、`perf_ui_baseline` 逾時）；補跑的第 4 次 29.2 ms。同一次的 OpenCV T2a p95 80.8–84.7 ms：HALCON 的種子搜尋快 2–3 倍，但時間比較不穩。流程 B 一次 `find_shape_models` 每個類型 3.0–7.0 ms（N ＝ 1000），OpenCV 10.8–16.9。授權第一次檢查 1.55–1.87 秒；取消延遲 p95 19–30 ms。**limit 一個都沒改**，T2b 標 `calibrated`。**真實影像**：LA 兩邊的晶格相同（都在空槽補了假缺格）；**HA 上 HALCON 用一根針腳當樣本只找到少數針腳**（分數 < 0.70；推測是樣本裡周圍焊墊的輪廓），OpenCV 找到大部分（也框到焊墊邊緣）——S09 的 A/B 重點。U3 仍超標（S04 起）。
- **已知限制**：強雜訊時 HALCON 的位置與角度比 OpenCV 不穩（shape model 只看邊緣點）；建模型約 10 ms、不能中斷；授權第一次檢查約 1.6 秒（在建立 back end 時）；範本有一部分在影像外的元件找不到；**HALCON 的影像來源沒有做**（只有 HALCON 沒有 OpenCV 的建置讀不了 TIFF）；`create_ncc_model` 的對照沒有做。 **新增設計決定**：ADR-017（HALCON back end）；ADR-010、ADR-014 各加「後續（S08）」一節。`docs/plan/S09`–`S12` 開頭各加了「S08 完成後的提醒」。

## S09 之前的 session A 與 B（2026-10-07／08）

- **session A：建置只用 qmake**（[ADR-018](adr/018-qmake-only-build.md)，使用者的決定）：Qt Creator 開 `AiInspTool/AiInspTool.pro`（所有程式庫、程式與 Qt Test），CMake 移除，執行檔 `AiInspTool.exe`，OpenCV 與 HALCON 預設編進去；8 種組合與 Qt Creator 的三種組態都驗證過（數字在 ADR-018 與 `docs/changelog-ai.md`）。HALCON 曾在 2026-10-07 15:06 到 10-08 13:20 拿不到授權（錯誤 2300，原因不明）；判斷用 `test.ps1 -Filter 'tst_halcon_backend'`，不要用 `hhostid`。
- **session B 的使用者決定**：（1）HALCON 授權的提醒改成**訊息列的卡片**（按〔關閉〕才收起；被預覽、無匹配、錯誤的卡片蓋過之後再出現）＋**狀態列最左邊一直標示**（「HALCON · runtime 授權」，滑鼠移上去看完整說明）；（2）**啟動時用上次用的 back end**（沒有記錄時 fake；`--backend` 照舊優先而且嚴格）；（3）畫面上有提案時切換 back end，**用新的重新搜尋同一個區域**；（4）Designer 的 `.ui` 的範圍：主視窗外框、樣本庫面板、物件面板、比對參數面板、空白卡片、訊息列（畫布與物件表的 model 維持程式碼）。
- **做了什麼**：第 1 件——`Backend::notices`（提醒跟著 back end 交出去）、`BackendPresenter`、`IBackendView`、`MessageBar` 的提醒卡片（優先順序：進度列 ＞ 卡片 ＞ 提醒卡片 ＞ 提示），狀態列的摘要不再重複 back end 名稱（ADR-017 的「後續（2026-10-08）」）。第 2 件——application 的 `SearchEngine`（back end ＋ use case，`shared_ptr` 共用：搜尋工作持有一份，舊的 adapter 等工作結束才釋放）與 `BackendSwitch`（規則，沒有 Qt），背景執行緒建立、切換中選單停用與「切換到 …」、失敗時訊息列的卡片並回到原本的、`QSettings` 的 INI 記住上次（`%APPDATA%AOIAutoCadCreated.ini`，`AOI_SETTINGS_DIR` 可以換；ADR-021）。第 3 件——五個表單（`shell/MainWindow.ui`、`MessageBar.ui`、`EmptyCard.ui`、`objects/ObjectsPanel.ui`、`params/ParamsPanel.ui`），畫布、訊息列、兩個面板是提升元件，文字一律不翻譯的佔位（ADR-022）。
- **驗證**：預設的建置 54／54（新增 `tst_backend_switch`；之前 53），`-NoHalcon` 51、`-NoOpenCv` 49、兩個都關 46、Debug 54、Profile 54；HALCON 的測試都沒有跳過。使用者在 Qt Creator 的 Designer 打開五個表單確認過。真的 HALCON 背景切換：總共 1.3 秒，`selectBackend` 0 ms 返回，UI 事件迴圈最長停頓 12 ms。UI 效能前後交替量 4 輪：I1、U3 沒有變差，U1 有一輪受機器負載干擾（ADR-022 的表）。數字與 Designer 的確認見 `docs/changelog-ai.md`。
- **已知限制**：HALCON 的授權探測整個程式只做一次——第一次切到 HALCON 失敗（dongle 沒插）之後，插上再切仍然失敗，要重新啟動程式；從 HALCON 換走之後它仍在這個行程裡初始化著，`SESSIONS=1` 的授權會不會被佔住沒有驗證；搜尋時 back end 無法使用的錯誤卡片標題沿用 S08 的寫法（把錯誤訊息當成名稱），沒有改。

## 預算校準狀態

`perf/budgets.json` 頂層仍是 `status: "proposed"`（T5 等 S10）。**S08 把 T2b 標 `calibrated`**（limit 沒有改，新增 `searchCalibrationS08`；HALCON 的 T1、T4、T6、T7 的數字記在 `docs/steps/S08-halcon/perf.md` 與 `perf/baseline/casparhsu1-halcon.json`，那些 id 的 `measured` 仍是 S07 OpenCV 的值）。T3 標 `calibrated`（S05）；**S07 把 T1、T2a、T4、T6a、T6b 標 `calibrated`**（limit 沒有改；新增 `searchCalibrationS07`；T7 的預算只在 `docs/steps/S07-opencv/perf.md` 提案，沒有寫進檔案）。UI 類：U1a、U1b、U4、I2、I5 標 `calibrated`，I1 標 `calibrated-at-limit`；S04 起 U2、I3、I4、I6 也標 `calibrated`，**U3 標 `measured-over-limit`**。**S06 沒有改 `budgets.json`**：S06 的指標（載入、複製次數、配方存讀）沒有預算，只記在 `docs/steps/S06-image-storage/perf.md` 與 `perf/baseline/casparhsu1-storage.json`；U4 在 Qt 6.12.0 重量過，記在 `perf.md`。S02–S05 的基準線（`perf/baseline/casparhsu1*.json`）是 Qt 6.8.0 的數字，只在這台機器可比較。

## 未解決事項

- **真實影像上的門檻與晶格（S07 發現，等使用者決定）**：（1）use case 的 `seedMinScore` 與預設的分數門檻（都是 0.70）在使用者的影像上會放進焊墊邊緣（0.70–0.85）；（2）晶格在非矩形陣列的空槽裡補假缺格（S05 已知失敗 #11）。證據與疊圖的說明在 `docs/steps/S07-opencv/perf.md` §4；兩件事都在 application／domain。
- **T7（流程 B 的樣本庫比對）的預算**：實測每個候選類型 8–9 ms、線性；提案在 `docs/steps/S07-opencv/perf.md` §2，要不要寫進 `perf/budgets.json` 是使用者的決定。
- **存檔與開啟配方的 UI（必要功能，沒有排進任何一步）**：儲存層與背景存檔在 S06 做好了，畫面上的〔儲存〕仍停用，也沒有〔開啟配方〕；開啟配方還需要記住影像路徑（S07 的 schema v2 只加了 `board.significantBits`）。需要使用者決定放進哪一步。**樣本庫 UI 也在同一步**：使用者 2026-10-06 決定，分層樣本庫三層的檔案位置（專案層、使用者層、系統層各放哪裡）等到那一步和這兩個 UI 一起定；**流程 B 的 UI（`finishRegionScan` 接 `OpenCvLibraryMatcher`）也等那一步**（S07 的 `Backend` 結構沒有 `libraryMatcher` 欄位）。
- **S04 沒有涵蓋的 UI**：`LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）沒有任何一步的 plan 檔涵蓋；流程 B（區域掃描）的結果固定是「樣本庫是空的」（S07 做好了 OpenCV 的比對器，但接受提案時類型沒有存參考影像、樣本庫也還沒組裝，所以畫面沒接）；接受提案時的元件類型是暫時的（`X1…`）；沒有校正值的影像暫以 1 µm ＝ 1 像素展開框；晶格的信心與「建議檢查」還沒有顯示。
- ADR-006 未決：實例覆寫、Panel 多連板、角度正負號、窗的演算法與參數、自由公式。庫的分層 S06 已照 TRI 決定並實作（`LayeredLibraryStore`）；各層的檔案位置等接樣本庫 UI 的那一步（見上一條）。新預算 T7（區域掃描隨樣本庫大小 N）等 S07 實測後再提。
- **HALCON（S08 之後）**：開發檔在 `third_party/halcon`（`%HALCONROOT%` 仍只有 runtime）。ADR-010 的 HALCON 端、ADR-006「每個條目一個 shape model、一次找多個」都在 S08 實測確認。**等使用者決定**：（1）HALCON 的影像來源（`read_image`）要不要做：現在 `release-halcon` 靠 OpenCV 讀 TIFF，只有 HALCON 的建置讀不了 TIFF；（2）授權提醒的規則：「用 runtime 授權、而 SDK 授權檔的 dongle 不在」才提醒，只有 runtime 授權的電腦（部署）不提醒——是否符合期待；（3）兩個 back end 的分數是不同的尺（NCC 對邊緣方向的一致度），同一個門檻 0.70 的意義不同，要不要分開（S09 的 A/B 一起看）。`nativeGray16`（HALCON 直接用 16 bit，零複製、更快）要不要當預設，S09 決定。
- 設定記憶（主題、字級、語言）、減少動態、高 DPI 的實際外觀：**沒有做**。`QBENCHMARK` 沒有用到也沒有驗證（ADR-003）。
- AI 共用結構與 `ai-project-template/`（2026-10-05 套用並合併，見 `docs/adr/000-ai-agent-structure.md`）：stage 時一律明確列出路徑；Codex 端的流程沒有在本機端到端測過（該機器的 Codex CLI 無法執行 `codex exec`）。
- 模型與推理強度路由（2026-10-08，Claude Code 專用；[ADR-019](adr/019-model-routing.md)、[ADR-020](adr/020-adaptive-model-effort-routing.md)，都以 `--no-ff` 合併到 `main`，沒有 push）：`MODEL_ROUTING.md`、五個 subagent（frontmatter 的 `model` 與 `effort`）、`UserPromptSubmit` hook（評分與升降級）。實測五個 subagent 的 model 與 effort 都照設定套用；同一組簡單任務五個 agent 都答對，費用差 35–40 倍，幾乎都是啟動成本（`MODEL_ROUTING.md` 第 9 節）。沒有 Git Bash 的電腦沒有驗證；更難的任務的正確率比較沒有做。
