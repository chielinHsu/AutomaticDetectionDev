# 系統架構

> 描述「現在的結構」，原地修改。為什麼這樣設計、否決過什麼，記在 `decisions.md`（索引）與 `docs/adr/`，不要寫在這裡。

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
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
| `infrastructure/storage`、`perf` | `application`、`domain`、Qt Core | Qt Widgets、視覺函式庫 |
| `presentation` | `application`、`domain`、Qt Widgets | OpenCV、HALCON |
| `app` | 全部（只做組裝） | 業務規則、繪圖 |

`tools/check-deps.mjs`（S03 建立）會在 CTest 檢查 `#include` 方向，違規即失敗。

## 模組

目前（S02）`src/` 有三層的程式碼；`application` 與 `infrastructure` 從 S03 起才有。依賴方向由 CMake 的 `target_link_libraries` 表達，違規的 `#include` 由 `tools/check-deps.mjs`（S03）擋下。

| CMake target | 目錄 | 層 | 內容 | 依賴 |
|---|---|---|---|---|
| `aoi_domain`（INTERFACE） | `src/domain/` | domain | `image_view.h`：`ImageView`（不擁有資料的 view＋共享擁有者，最小版；S03／S06 定案所有權規則） | 標準庫 |
| `aoi_presentation`（STATIC） | `src/presentation/` | presentation | `canvas/`：`ImageCanvas`（`QGraphicsView`）、`ImageItem`、`RoiLayerItem`（批次繪製）、`RoiGraphicsItem`、`BoxFrames`、`OverlayStyle`、View／Controller 介面；`tools/`：`ITool`、`ToolRegistry`、四個工具；`shell/`：`MainWindow`、`MessageBar`、`EmptyCard`、`ShellController`（S04 前的暫代）、`ImageLoader`（S06 前的暫代）；`theme/`：`ThemeTokens`、`ThemeManager`；`i18n/`：`JsonTranslator`、`Translations` | `aoi_domain`、Qt Widgets |
| `aoi_presentation_dev`（STATIC） | `src/presentation/dev/` | （開發用） | `FakeScene`：合成板子與框，供測試、效能量測與 `--demo` 使用；不是產品功能 | `aoi_presentation` |
| `aoi`（EXECUTABLE） | `src/app/` | app | `main.cpp`：組裝（主題、語言、視窗、命令列） | `aoi_presentation`（`AOI_DEMO_SCENE` 開啟時加 `aoi_presentation_dev`） |
| `tst_*`、`perf_*` | `tests/ui/` | （測試） | Qt Test；單元測試與效能基準線 | `aoi_presentation`、`aoi_presentation_dev` |

畫布的設計細節（為什麼分 Item／Layer／Tool、為什麼用整數像素的填色矩形畫框）見 [`docs/steps/S02-ui-shell/spec.md`](steps/S02-ui-shell/spec.md) 與 [`diagram.md`](steps/S02-ui-shell/diagram.md)。
