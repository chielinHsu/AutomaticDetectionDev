# 系統架構

> 描述「現在的結構」，原地修改。為什麼這樣設計、否決過什麼，記在 `decisions.md`（索引）與 `docs/adr/`，不要寫在這裡。

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
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
| `infrastructure/storage`、`perf`、`threading` | `application`、`domain`、Qt Core | Qt Widgets、視覺函式庫 |
| `presentation` | `application`、`domain`、Qt Widgets | OpenCV、HALCON |
| `app` | 全部（只做組裝） | 業務規則、繪圖、直接使用視覺函式庫（只能經 adapter 的工廠） |

`tools/check-deps.mjs`（S03 建立）會在 CTest 檢查 `#include` 方向，違規即失敗：它的規則表（`RULES`）是上表的機器可讀版，**改一邊要改另一邊**。它只掃 `src/`（`tests/`、`AiInspTool/`、`build/` 不掃）；`src/` 底下不在五層裡的資料夾、`infrastructure/` 底下沒有登記的 adapter 資料夾，一律算違規（新增 adapter 要先在規則表與上表登記）。13 個 fixture（`tests/core/fixtures/deps/`）與 `--self-test` 證明它抓得到違規。限制：只看 `#include` 文字；CMake 的 `target_link_libraries` 是另一道防線。

## 模組

目前（S03）`src/` 有五層的程式碼，但 `presentation` 還沒有接上 `application`（S04）。依賴方向由 CMake 的 `target_link_libraries` 表達，違規的 `#include` 由 `tools/check-deps.mjs` 擋下。

| CMake target | 目錄 | 層 | 內容 | 依賴 |
|---|---|---|---|---|
| `aoi_domain`（INTERFACE） | `src/domain/` | domain | 全是標頭：`geometry.h`（座標與角度慣例、`Rect`、`Pose2D`、`RectObj`）、`image_view.h`（`ImageView`＋校正值 `umPerPixel`；所有權規則 S06 定案）、`match.h`（`Match`、`Candidate`、`InspectionObject`）、`object_state.h`（`ObjectState`、`NoMatchReason`）、`search_params.h`（`SearchParams`、`SearchRegion`）、`component.h`（Board › Component › Box 的型別）、`recipe.h` | 標準庫 |
| `aoi_application`（STATIC） | `src/application/` | application | `Error.h`、`Result.h`、`CancelToken.h`；`ports/`（九個窄介面）；`jobs/JobRunner`（latest-wins）；`usecases/GenerateObjectsFromSample`、`ProposalView`（`classify`）、`SimpleGrid`（S05 取代的暫代品）；`backend/BackendRegistry` | `aoi_domain` |
| `aoi_backend_fake`（STATIC） | `src/infrastructure/backend_fake/` | infrastructure | 假 back end：`FakeGrid`（可重現的合成網格）、`FakeBackend`（範本建立器、搜尋器、工廠）；不看影像、只用標準庫 | `aoi_application` |
| `aoi_perf`（STATIC） | `src/infrastructure/perf/` | infrastructure | `PerfProbe`：`IPerfProbe` 的實作（環狀緩衝、JSON 輸出） | `aoi_application` |
| `aoi_threading`（STATIC） | `src/infrastructure/threading/` | infrastructure | `ThreadPoolExecutor`、`InlineExecutor`：`IExecutor` 的實作 | `aoi_application` |
| `aoi_presentation`（STATIC） | `src/presentation/` | presentation | `canvas/`：`ImageCanvas`（`QGraphicsView`）、`ImageItem`、`RoiLayerItem`（批次繪製）、`RoiGraphicsItem`、`BoxFrames`、`OverlayStyle`、View／Controller 介面；`tools/`：`ITool`、`ToolRegistry`、四個工具；`shell/`：`MainWindow`、`MessageBar`、`EmptyCard`、`ShellController`（S04 前的暫代）、`ImageLoader`（S06 前的暫代）；`theme/`：`ThemeTokens`、`ThemeManager`；`i18n/`：`JsonTranslator`、`Translations` | `aoi_domain`、Qt Widgets |
| `aoi_presentation_dev`（STATIC） | `src/presentation/dev/` | （開發用） | `FakeScene`：合成板子與框，供測試、效能量測與 `--demo` 使用；不是產品功能 | `aoi_presentation` |
| `aoi_app_core`（STATIC） | `src/app/CompositionRoot.*` | app | `makeBackendRegistry`（註冊 fake，以及 opencv、halcon 的「不可用」）、`composeCore`（back end ＋ 執行緒池 ＋ use case ＋ `JobRunner` ＋ probe）；獨立成 library 讓測試能驗證組裝 | `aoi_application`、`aoi_backend_fake`、`aoi_perf`、`aoi_threading` |
| `aoi`（EXECUTABLE） | `src/app/main.cpp` | app | 組裝（主題、語言、視窗、核心、命令列 `--backend`） | `aoi_presentation`、`aoi_app_core`（`AOI_DEMO_SCENE` 開啟時加 `aoi_presentation_dev`） |
| `tst_*`、`perf_*` | `tests/ui/` | （測試） | Qt Test；UI 的單元測試與效能基準線 | `aoi_presentation`、`aoi_presentation_dev` |
| `tst_*`、`perf_core`、`check_deps*` | `tests/core/` | （測試） | 核心的 Qt Test（[ADR-003](adr/003-test-framework.md)）、命令列測試 `tst_cli`、效能量測 `perf_core`；依賴檢查的 CTest 項目與 13 個 fixture | `aoi_application`、`aoi_backend_fake`、`aoi_perf`、`aoi_threading`、`aoi_app_core` |

畫布的設計細節（為什麼分 Item／Layer／Tool、為什麼用整數像素的填色矩形畫框）見 [`docs/steps/S02-ui-shell/spec.md`](steps/S02-ui-shell/spec.md) 與 [`diagram.md`](steps/S02-ui-shell/diagram.md)。

## 檔案命名

- `domain`：標頭用 snake_case（`image_view.h`、`geometry.h`），一個檔案可以放幾個相關的小型別。
- `application`、`infrastructure`、`presentation`、`app`：一個類別一個檔案，檔名與類別同名（PascalCase，例如 `JobRunner.h`、`FakeBackend.cpp`）。
- 命名空間：`aoi`（domain、application）、`aoi::ui`（presentation）、`aoi::dev`（開發用）、`aoi::fake`、`aoi::infra`（infrastructure 的 adapter）、`aoi::app`（組裝根）。

座標與角度慣例（影像像素、y 向下、角度為度、螢幕上逆時針為正）在 `src/domain/geometry.h`，理由見 [ADR-010](adr/010-coordinate-and-angle-convention.md)。
