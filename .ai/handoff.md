# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-13

## 正在做

無。**S11（效能優化，先量再改）完成**（分支 `step/11-optimize`，`--no-ff` 合併到 `main`，tag `step-11-done`，沒有 push）。**接下來是 S12**（這個 session 收工時用 `spawn_task` 開；Start prompt 在 `docs/plan/S12-release.md` 底部）。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、`docs/steps/S11-optimize/`、ADR-024）

- 挑選：S10 報表裡只有 U3（門檻滑桿 → 重繪，5 832 個元件）超出預算；搜尋類與互動類都在預算內，所以只做 U3，搜尋類列成候選沒有改（perf.md §5）。
- 先量：`perf_ui_s04` 的新診斷把一次重繪拆成影像 1.1 ms、框層 12.5 ms；場景重建 3.5–5.5 ms；背景有已接受的元件再多 9 ms。
- 三個優化、三個 commit：（1）`RoiLayerItem` 小框用字形快取畫（像素和填色矩形相同，測試逐像素比對；dpr ≠ 1 或框超過 96 px 照舊）：框層 12.5 → 1.15 ms；（2）`ComponentView::label()`、`displayName()` 延後格式化：場景重建 3.46 → 0.89 ms，U3 進入預算；（3）`InspectionDocument::revision()` ＋ `DocumentSceneCache`：U3-bg 17.8 → 7.2 ms。最後 U3 中位數 6.5／p95 7.6 ms（預算 16）；I1 p95 4.5、I3 5–7、I6 3.0 ms。
- `perf/budgets.json` 的 U3 改 `calibrated`（limit 沒有改）、`perf_ui_s04` 的 `knownOver("U3")` 拿掉：U3 超標從現在起會讓測試失敗、閘門也會擋。基準線重建：三次都 0 項超出預算，U3 p95 6.5–10.9 ms。

## 建置與測試

- `test.ps1` 預設 57／57（HALCON 0 跳過；XFAIL 2 列和之前相同：opencv B1、`tst_image_sources_agree` 的 1 列）；`-Config Debug` **57／57**（HALCON 0 跳過；跳過與 XFAIL 的清單和 Release 相同）。
- 效能的數字（每個優化之後各量一次、同一天）在 `docs/steps/S11-optimize/perf.md` §3；這台機器同一份程式兩次差約 2 ms，改善的幅度（5、5、10 ms）遠大於雜訊。

## 不要動

- `RoiLayerItem` 的字形路徑與 `BoxFrames` 要產生相同的像素（`tst_items` 的 `glyphsGiveTheSamePixelsAsFilledRects`）；`kGlyphMaxSide`、`kGlyphCacheLimit` 是量過的起點，要改先量。
- `DocumentSceneCache` 的鍵只有修訂號與標籤：改板子內容的新路徑一律做成 Command（經 `InspectionDocument::notify`），否則畫面會過期。
- `HalconSearchOptions` 的預設（使用者確認過）；`OpenCvSearchOptions` 的預設；`ContractTolerances.h` 的數字；`tst_contract_end_to_end` 的 opencv B1 `QEXPECT_FAIL`（D6）。
- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key；`perf/budgets.json` 的 limit；`knownOver("I1")` 等 ADR-009 的決定。
- `third_party/halcon/`；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約的語意、`ImageView`、`SearchParams`。
- `AiInspTool/main.cpp`、`mainwindow.*`；`aoi_target.pri` 的 `/IGNORE:4199`；表單的文字不要改成可翻譯（ADR-022）。
- `perf/baseline/casparhsu1-harness.json`：只用 `perf.ps1 -WriteBaseline` 重建，不要手改。

## 已知問題與風險

- 字形快取只在 devicePixelRatio ＝ 1 生效（這台 96 DPI）；高 DPI 的機器上 U3 會回到填色矩形的速度（S04 的數字）。S12 在目標機器上看一次。
- U3 的 `knownOver` 已拿掉：整台機器變慢時（S10 第 1 次那種，處理器 54–67%）`perf_ui_s04` 可能因 U3 超標失敗，看報表上處理器的實際速度再判斷。
- `perf.ps1 -Window`、`-Profile full`、真實影像（`AOI_REAL_IMAGE_DIR`）仍沒有跑過；I1 真實視窗的數字（ADR-009）沒有重量。
- 等使用者決定（ADR-004、S09、S10）：分數門檻要不要依 back end 分開、`nativeGray16`、AB-U3 與 T7 的預算、I1（ADR-009）、真實影像 HA 的位置、use case 驗證階段的漏洞（`current-status.md` 待決定（10））。

## 驗證（實際執行）

- `test.ps1` 預設 Release 57／57；`-Config Debug` **57／57**（HALCON 0 跳過；跳過與 XFAIL 的清單和 Release 相同）；`node tools/ste-lint.mjs` 979 sentences、0 errors；`check-ai-structure.ps1` OK（既有警告：`current-status.md` 超過 80 行）。
- 每個優化之後 `test.ps1 -Perf -NoBuild -Filter perf_ui_s04`，優化 1 之後 `perf_ui_baseline`；收工時 `perf.ps1 -NoBuild -Runs 3 -WriteBaseline`：三次都 0 項超出預算、0 個測試失敗；對舊基準線的閘門三次 FAIL，是 UI 的改善與機器的飄移（perf.md §6）；新基準線 641 個指標。
