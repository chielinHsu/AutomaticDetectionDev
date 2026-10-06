# 系統架構

> 描述「現在的結構」，原地修改。為什麼這樣設計、否決過什麼，記在 `decisions.md`（索引）與 `docs/adr/`，不要寫在這裡。

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-06 | Claude Code (Claude Opus 5.5) | S07：依賴表沒有變。模組表：`aoi_backend_opencv` 多了範本建立器、搜尋器、樣本庫比對器（流程 B）、小分數圖的 NCC、金字塔與工廠；`aoi_app_core` 在 `AOI_WITH_OPENCV=ON` 時把 opencv 註冊成可用、新增 `workerThreads()`（JobRunner 與 OpenCV 共用的執行緒數）；domain 的 `Board::significantBits` 與 `decideSignificantBits`（有效位元數的覆寫）、application 的 `InspectionDocument::setSignificantBits`、storage 的配方 schema v2、presentation 的參數面板「影像位元數」；新增 `tests/backend/`。設計決策：[ADR-016](adr/016-opencv-coarse-to-fine-search.md)，ADR-014、ADR-015 的 S07 後續 |
| 2026-10-06 | Claude Code (Claude Opus 5.5) | S06：依賴表新增 `infrastructure/image_qt`（Qt Core＋Qt Gui，不可以用 Widgets 與視覺函式庫；`check-deps` 新增 `qt-gui` 類別與 2 個 fixture，共 15 個）。模組表新增 `aoi_image_qt`、`aoi_storage`、`aoi_backend_opencv`（`AOI_WITH_OPENCV=ON` 才建置；S06 起有影像轉換與影像來源）、domain 的 `image_buffer`／`image_copy`、application 的 `image/ImageSourceChain`、`document/RecipeSaver`；presentation 移除 `ImageLoader`、`ImageViewAdapter`（開圖改走 `IImageSource`）；新增 `tests/infra/`、`tests/support/`。設計決策：[ADR-014](adr/014-image-ownership-and-copies.md)、[ADR-015](adr/015-recipe-and-library-storage.md) |
| 2026-10-06 | Claude Code (Claude Opus 5.5) | S05：domain 第一次有 .cpp（`domain/lattice/`：晶格估計、預測、正規方程累加器、NMS、空間格子），`aoi_domain` 由 INTERFACE 改 STATIC；`application/usecases/SimpleGrid` 刪除；新增 `tests/domain/`。依賴表沒有變（domain 仍只用標準庫）。晶格的接受規則見 [ADR-013](adr/013-lattice-acceptance-rules.md) |
| 2026-10-06 | Claude Code (Claude Sonnet 5.5) | S04：`presentation` 第一次連到 `application`（依賴表沒有變，只是 `aoi_presentation` 多連 `aoi_application`）。模組表新增 `application/document/`、`application/interaction/`、`domain/box_expander.h`、`aoi_threading_qt`（`QtThreadPoolExecutor`）、`presentation/presenters/`、`objects/`、`params/`，移除 `ShellController`；`composeCore` 多一個 `ExecutorKind`。互動規則放在 application 層（[ADR-011](adr/011-interaction-logic-in-application-layer.md)）；虛線與點線改用填色矩形畫（[ADR-012](adr/012-dashed-frames-as-fill-rects.md)） |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | S03：`application` 與 `infrastructure` 有程式了。依賴表新增 `infrastructure/backend_fake`、`infrastructure/threading` 兩列，`app` 的禁止欄加上「直接使用視覺函式庫」，規則表的機器可讀版在 `tools/check-deps.mjs`（改一邊要改另一邊）；模組表補上 `aoi_application`、`aoi_backend_fake`、`aoi_perf`、`aoi_threading`、`aoi_app_core`；新增「檔案命名」 |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | S02：補上「模組」表（`src/` 已建立）：domain 只有 `image_view.h`，presentation 有畫布、工具、shell、theme、i18n；說明 CMake target 如何表達依賴方向 |
| 2026-10-05 | Claude Code (Claude Sonnet 5.5) | 把分層與依賴規則表從 `CLAUDE.md` 搬到這裡，內容未改 |
| 2026-10-05 | 範本 | 建立 |

## 總覽

五層架構，內層不可 include 外層的標頭檔。完整設計理由、更多圖在 `docs/00-master-plan.md` 第 2 節與 `docs/steps/S00-overview/diagram.md`。

## 分層與依賴規則（Dependency Rule）

```
app ──► presentation ──► application ──► domain
 │                           ▲              ▲
 └──────► infrastructure ────┴──────────────┘   (實作 application 的 ports)
```

| 層（`src/` 下） | 可以依賴 | 禁止 |
|---|---|---|
| `domain` | C++ 標準庫 | Qt、OpenCV、HALCON、任何 I/O |
| `application` | `domain` | Qt、OpenCV、HALCON |
| `infrastructure/backend_opencv` | `application`(ports)、`domain`、OpenCV | Qt Widgets、HALCON、其他 adapter |
| `infrastructure/backend_halcon` | `application`(ports)、`domain`、HALCON | Qt Widgets、OpenCV、其他 adapter |
| `infrastructure/backend_fake` | `application`(ports)、`domain`、標準庫 | Qt、OpenCV、HALCON、其他 adapter |
| `infrastructure/image_qt` | `application`(ports)、`domain`、Qt Core、Qt Gui（`QImage`、`QImageReader`） | Qt Widgets、視覺函式庫、其他 adapter |
| `infrastructure/storage`、`perf`、`threading` | `application`、`domain`、Qt Core | Qt Gui、Qt Widgets、視覺函式庫 |
| `presentation` | `application`、`domain`、Qt Widgets | OpenCV、HALCON |
| `app` | 全部（只做組裝） | 業務規則、繪圖、直接使用視覺函式庫（只能經 adapter 的工廠） |

`tools/check-deps.mjs`（S03 建立）會在 CTest 檢查 `#include` 方向，違規即失敗：它的規則表（`RULES`）是上表的機器可讀版，**改一邊要改另一邊**。它只掃 `src/`（`tests/`、`AiInspTool/`、`build/` 不掃）；`src/` 底下不在五層裡的資料夾、`infrastructure/` 底下沒有登記的 adapter 資料夾，一律算違規（新增 adapter 要先在規則表與上表登記）。15 個 fixture（`tests/core/fixtures/deps/`）與 `--self-test` 證明它抓得到違規。Qt 的裸標頭名分三類：Core 白名單、Gui 白名單（S06）、其他一律算 Widgets（寧可誤報）。限制：只看 `#include` 文字；CMake 的 `target_link_libraries` 是另一道防線。

## 模組

目前（S07）`src/` 有五層的程式碼，`presentation` 透過 Presenter 接上 `application`；它依然不 include `infrastructure`（具體的 back end、執行緒池與影像來源由 `app/` 組裝後，以 use case、`JobRunner` 與 `IImageSource` 的參考交給 Presenter 與主視窗）。依賴方向由 CMake 的 `target_link_libraries` 表達，違規的 `#include` 由 `tools/check-deps.mjs` 擋下。

| CMake target | 目錄 | 層 | 內容 | 依賴 |
|---|---|---|---|---|
| `aoi_domain`（STATIC） | `src/domain/` | domain | 型別多半只有標頭：`geometry.h`（座標與角度慣例、`Rect`、`Pose2D`、`RectObj`）、`image_view.h`（`ImageView`＋校正值 `umPerPixel`；S06 定案所有權規則、新增 `Bgr24`／`Gray16`、`subView`）、`image_buffer.h/.cpp`（`ImageBuffer`：自己擁有像素、緊密排列、對齊 64 byte）、`image_copy.h/.cpp`（複製計數，ADR-014）、`match.h`（`Match`、`Candidate`、`InspectionObject`）、`object_state.h`（`ObjectState`、`NoMatchReason`）、`search_params.h`（`SearchParams`、`SearchRegion`）、`component.h`（Board › Component › Box 的型別）、`recipe.h`、`box_expander.h`（`BoxGroup` ＋ 位姿 → 具體的框；`effectiveUmPerPixel`）。S05 起 `lattice/` 有 .cpp：`LatticeEstimator`（找陣列：可能多組、接受規則見 ADR-013）、`LatticePredictor`（缺格、由近到遠）、`LatticeAccumulator`（正規方程，加點 O(1)）、`NonMaxSuppression`／`OverlapIndex`（旋轉矩形的重疊、空間雜湊）、`SpatialGrid`、分群策略 `IVectorClustering`；內部：`basis_finder`、`lattice_acceptance`、`cell_components`、`lattice_detail.h`、`flat_map.h` | 標準庫 |
| `aoi_application`（STATIC） | `src/application/` | application | `Error.h`、`Result.h`、`CancelToken.h`；`ports/`（九個窄介面）；`jobs/JobRunner`（latest-wins）；`usecases/GenerateObjectsFromSample`（S05 起：種子先 NMS、晶格用 domain 的 `LatticeEstimator`、缺格由近到遠、驗證後 O(1) 重新擬合）、`ProposalView`（`classify`）；`backend/BackendRegistry`；`image/ImageSourceChain`（S06：幾個 `IImageSource` 依優先順序）；`document/`：`UndoStack`（樣板）、`ProposalSession`、`InspectionDocument`、`RecipeSaver`（S06：背景存檔，同一個檔案依序寫）；`interaction/`：`InteractionStateMachine`、`InspectionSession`（互動的所有規則，沒有 Qt） | `aoi_domain` |
| `aoi_backend_fake`（STATIC） | `src/infrastructure/backend_fake/` | infrastructure | 假 back end：`FakeGrid`（可重現的合成網格）、`FakeBackend`（範本建立器、搜尋器、工廠）；不看影像、只用標準庫 | `aoi_application` |
| `aoi_image_qt`（STATIC） | `src/infrastructure/image_qt/` | infrastructure | S06：`QtImages`（`fromQImage`、`wrapAsQImage`、`toCanonicalQImage`）、`QtImageSource`（`IImageSource` 的 `QImageReader` 版；這台機器的 Qt 沒有 TIFF） | `aoi_application`、Qt Gui |
| `aoi_storage`（STATIC） | `src/infrastructure/storage/` | infrastructure | S06：`JsonCodec`（domain 型別 ⇄ JSON）、`JsonFile`（有版本的文件檔：讀、原子寫入、遷移）、`JsonRecipeStore`（`IRecipeStore`）、`JsonLibraryStore`（`ILibraryStore`，一個檔案一層）、`LayeredLibraryStore`（TRI 的分層：專案 → 使用者 → 系統）；ADR-015 | `aoi_application`、Qt Core |
| `aoi_backend_opencv`（STATIC，`AOI_WITH_OPENCV=ON` 才有） | `src/infrastructure/backend_opencv/` | infrastructure | S06：`OpenCvImages`（`fromMat`、`borrowAsMat`；S07 加 `toGray8`：轉 8 bit 灰階，依有效位元數換算、計數）、`OpenCvImageSource`（`imdecode`，讀 TIFF 與 16 bit）。S07：`OpenCvTemplateBuilder`（金字塔 × 旋轉範本）、`OpenCvTemplate`、`OpenCvPatternSearcher`（由粗到細的 NCC、視窗清單、爬坡、拋物線次像素、domain 的 NMS）、`OpenCvLibraryMatcher`（`ILibraryMatcher`，流程 B；UI 還沒接）、`OpenCvCorrelation`（小的分數圖自己算）、`OpenCvPyramid`（座標換算）、`OpenCvSearchOptions`、`OpenCvBackend`（工廠、`setOpenCvThreads`）；ADR-016 | `aoi_application`、OpenCV（`opencv_world`） |
| `aoi_perf`（STATIC） | `src/infrastructure/perf/` | infrastructure | `PerfProbe`：`IPerfProbe` 的實作（環狀緩衝、JSON 輸出） | `aoi_application` |
| `aoi_threading`（STATIC） | `src/infrastructure/threading/` | infrastructure | `ThreadPoolExecutor`、`InlineExecutor`：`IExecutor` 的實作（標準庫） | `aoi_application` |
| `aoi_threading_qt`（STATIC） | `src/infrastructure/threading/` | infrastructure | `QtThreadPoolExecutor`：`IExecutor` 的 `QThreadPool` 版（S04）；只用 Qt Core | `aoi_application`、Qt Core |
| `aoi_presentation`（STATIC） | `src/presentation/` | presentation | `canvas/`：`ImageCanvas`（`QGraphicsView`）、`ImageItem`、`RoiLayerItem`（批次繪製）、`RoiGraphicsItem`、`BoxFrames`、`OverlayStyle`、View／Controller 介面；`tools/`：`ITool`、`ToolRegistry`、四個工具（`SelectTool` 含拖曳移動）；`shell/`：`MainWindow`（Presenter 的 View：`IStatusView`、`IMessageView`、`IHistoryView`）、`MessageBar`（提示、吐司、卡片、進度列）、`EmptyCard`（S06：開圖改走 `IImageSource`，S02 的暫代 `ImageLoader`、`ImageViewAdapter` 移除）；`canvas/ImageViewMetaType.h`（`ImageView` 在訊號裡傳遞）；`presenters/`：`AutoRoiPresenter`、`ParamsPresenter`、`AppPresenters`（接線）、`SceneBuilder`、`MessageTexts`、`UiPoster`、`ViewInterfaces.h`、`ControllerInterfaces.h`；`objects/`：`ObjectsModel`、`ObjectsPanel`、`ObjectRows.h`；`params/`：`ParamsPanel`；`theme/`：`ThemeTokens`、`ThemeManager`；`i18n/`：`JsonTranslator`、`Translations` | `aoi_domain`、`aoi_application`、Qt Widgets |
| `aoi_presentation_dev`（STATIC） | `src/presentation/dev/` | （開發用） | `FakeScene`：合成板子與框，供測試、效能量測與 `--demo` 使用；不是產品功能 | `aoi_presentation` |
| `aoi_app_core`（STATIC） | `src/app/CompositionRoot.*` | app | `makeBackendRegistry`（註冊 fake；opencv 在 `AOI_WITH_OPENCV=ON` 時是可用的〔S07〕，否則「不可用」並說怎麼編進來；halcon「不可用」）、`workerThreads()`（核心數 − 1：JobRunner 的 worker 與 OpenCV 的平行迴圈共用，ADR-016）、`composeCore`（back end ＋ 執行緒池〔`ExecutorKind`：`QtThreadPool` 或 `StdThreads`〕＋ use case ＋ `JobRunner` ＋ probe）、`makeImageSource`（S06：Qt 在前、OpenCV 在後，ADR-014）；獨立成 library 讓測試能驗證組裝 | `aoi_application`、`aoi_backend_fake`、`aoi_image_qt`、`aoi_perf`、`aoi_threading`、`aoi_threading_qt`（`AOI_WITH_OPENCV=ON` 時加 `aoi_backend_opencv`） |
| `aoi`（EXECUTABLE） | `src/app/main.cpp` | app | 組裝（主題、語言、視窗、核心、`AppPresenters`、命令列 `--backend`） | `aoi_presentation`、`aoi_app_core`（`AOI_DEMO_SCENE` 開啟時加 `aoi_presentation_dev`） |
| `tst_*`、`perf_*` | `tests/ui/` | （測試） | Qt Test；UI 的單元測試、Presenter 的整合測試（`tst_presenter`，走產品的接線）與效能量測（`perf_ui_baseline`、`perf_ui_s04`） | `aoi_presentation`、`aoi_presentation_dev`、`aoi_app_core` |
| `tst_lattice*`、`tst_nms`、`perf_lattice`、`s00_demo_fixture_is_current` | `tests/domain/` | （測試） | S05：晶格估計（含 S03 `tst_grid` 的規格）、1 029 種情境掃描、累加器與預測器、NMS、與 S00 示範頁交叉驗證（fixture 由 `tools/s00-demo-fixture.mjs` 執行頁面原本的 JavaScript 產生）、說明頁的資料檢查（`tst_lattice_explainer`）、T3 微基準 | `aoi_domain` |
| `tst_*`、`perf_core`、`check_deps*` | `tests/core/` | （測試） | 核心的 Qt Test（[ADR-003](adr/003-test-framework.md)）、命令列測試 `tst_cli`、效能量測 `perf_core`；S04 加了互動規則的測試（`tst_undo_stack`、`tst_state_machine`、`tst_box_expander`、`tst_proposal_session`、`tst_document`、`tst_session`）與 `tst_qt_executor`；依賴檢查的 CTest 項目與 15 個 fixture；S06 加 `tst_image_chain`、`tst_recipe_saver` | `aoi_application`、`aoi_backend_fake`、`aoi_perf`、`aoi_threading`、`aoi_threading_qt`、`aoi_app_core` |
| `tst_opencv_*`、`perf_opencv*` | `tests/backend/` | （測試） | S07（`AOI_WITH_OPENCV=ON` 才有）：`tst_opencv_backend`（範本建立器與搜尋器的契約，S09 契約測試的草稿；座標、角度正負號、格式、取消、可重入、複製次數、小分數圖的 NCC）、`tst_opencv_end_to_end`（組裝根的 OpenCV back end ＋ use case，在合成板子上和標準答案比；有效位元數的覆寫）、`tst_opencv_library_matcher`；效能量測 `perf_opencv`（T1、T2a、T4、T6、T7 與曲線）、`perf_opencv_real`（使用者的真實影像，`AOI_REAL_IMAGE_DIR` 有設才跑；label `perf`） | `aoi_backend_opencv`、`aoi_backend_fake`、`aoi_app_core`、`aoi_test_support` |
| `tst_*`、`perf_storage` | `tests/infra/` | （測試） | S06：`tst_image_qt`、`tst_recipe_store`（範例檔在 `fixtures/`：v1、S07 的 v2）、`tst_library_store`、`tst_library_layers`、`tst_synthetic_board`；`AOI_WITH_OPENCV=ON` 時加 `tst_opencv_image`、`tst_image_sources_agree`（兩個影像來源的 Liskov 契約）；效能量測 `perf_storage`（label `perf`） | `aoi_image_qt`、`aoi_storage`、`aoi_test_support`（與 `aoi_backend_opencv`） |
| `aoi_test_support`（STATIC） | `tests/support/` | （測試） | S06：合成板子產生器 `synthetic_board`（標準庫；同 seed 同輸出、附標準答案 JSON）與 `synthetic_board_io`（存成 PNG＋JSON，Qt） | `aoi_domain`、`aoi_image_qt` |

畫布的設計細節（為什麼分 Item／Layer／Tool、為什麼用整數像素的填色矩形畫框）見 [`docs/steps/S02-ui-shell/spec.md`](steps/S02-ui-shell/spec.md) 與 [`diagram.md`](steps/S02-ui-shell/diagram.md)。互動的設計（狀態機、Command 與復原、worker ↔ UI 執行緒、忙碌與取消）見 [`docs/steps/S04-presenter/spec.md`](steps/S04-presenter/spec.md) 與 [`diagram.md`](steps/S04-presenter/diagram.md)。晶格估計的流程、接受規則與已知會失敗的情況見 [`docs/steps/S05-lattice/spec.md`](steps/S05-lattice/spec.md) 與 [`diagram.md`](steps/S05-lattice/diagram.md)。影像的所有權、轉換、配方與樣本庫的儲存見 [`docs/steps/S06-image-storage/spec.md`](steps/S06-image-storage/spec.md) 與 [`diagram.md`](steps/S06-image-storage/diagram.md)。

## 檔案命名

- `domain`：檔名用 snake_case（標頭與 .cpp 都是，例如 `image_view.h`、`lattice/lattice_estimator.cpp`），一個檔案可以放幾個相關的小型別；只在 domain 內部用的工具放 `aoi::detail` 命名空間（例如 `lattice/lattice_detail.h`）。
- `application`、`infrastructure`、`presentation`、`app`：一個類別一個檔案，檔名與類別同名（PascalCase，例如 `JobRunner.h`、`FakeBackend.cpp`）。
- 命名空間：`aoi`（domain、application）、`aoi::ui`（presentation）、`aoi::dev`（開發用）、`aoi::fake`、`aoi::infra`（infrastructure 的 adapter）、`aoi::app`（組裝根）。

座標與角度慣例（影像像素、y 向下、角度為度、螢幕上逆時針為正）在 `src/domain/geometry.h`，理由見 [ADR-010](adr/010-coordinate-and-angle-convention.md)。
