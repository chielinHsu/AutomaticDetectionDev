# 系統架構

> 描述「現在的結構」，原地修改。為什麼這樣設計、否決過什麼，記在 `decisions.md`（索引）與 `docs/adr/`，不要寫在這裡。

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
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

`src/` 尚未建立（S02 起）。模組表在建立程式碼後補在這裡。
