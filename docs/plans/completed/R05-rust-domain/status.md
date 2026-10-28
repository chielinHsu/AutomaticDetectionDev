# R05 狀態

> 快照，不是歷史：只寫「現在」（歷史在 `changelog.md`，3.0.0 之前在 `docs/legacy/changelog-ai.md`；細項狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-28
- 狀態：完成（R05.1–R05.5；tag `r05.1-done`–`r05.5-done`，只在本機）
- 分支：沒有進行中的分支

## 目前細項

- 無（R05.5 收尾完成：`AOI_DOMAIN_IMPL` 預設 `rust`，`cpp` 選項與 C++ 的實作保留）。

## 阻礙

- 無。

## 待使用者決定

- 第二階段（application 層搬進 Rust、FFI 要不要改用 cxx）：使用者 2026-10-28 決定先不做，登記在 `docs/current-status.md` 的待決定；要做時開新計畫，連回本計畫。
- `perf_match_region_real` 的影像 `FovImages1\0_0_HA.tif` 在 2026-10-20 換過，預設的點擊位置與主體框對不上（標準答案只找到 1 根端子，搬之前就失敗）：要更新 `AOI_REAL_MATCH_CLICK`／`AOI_REAL_MATCH_BODY` 的預設，或指向原本的影像。到 R05 完成為止，真實影像的召回與位置**沒有量到**。
- `tst_repeatability::realFiveRuns` 要的原始五份 CSV（2026-10-14、703 Box、`Pin_29_29`）在這台機器的 D: 找不到：要用時告知位置（設 `AOI_REPEATABILITY_DATA`）。

## 下一步

- 本計畫沒有下一步。之後的相關工作（第二階段、刪掉 `cpp` 選項）開新計畫。

## 接續資訊

- `AOI_DOMAIN_IMPL`：預設 `rust`（`aoi_options.pri`）；`test.ps1`／`perf.ps1` 的 `-DomainImpl Cpp` 用 C++ 的實作，建置資料夾多 `-cpp`。沒有 cargo 的機器（或 Qt Creator 沒讀到新的 PATH）要加 `AOI_DOMAIN_IMPL=cpp`。
- 差異測試 `tst_domain_rs_diff`（只在 rust 建置）171 項；改 domain 的演算法時兩份實作要一起改，或在新計畫裡決定刪掉 C++ 那一份。
- 建置資料夾不要改名（`tst_cli` 把執行檔的路徑編進物件檔），要換就刪掉重建；不要在 `AiInspTool/` 原地執行 qmake（2026-10-27 的原地建置已搬到 `build/r05-5-insource-backup/`）。
- 效能基準線 `perf/baseline/casparhsu1-harness.json` 在 R05.5 用 Rust 的預設建置重建（最安靜的三次量測，挑法在 ADR-031「後續（R05.5）」）；逐次呼叫的微基準（`lattice/INC-*`、`FULL-*`、`NMS-PAIR-20000`）比 C++ 慢是已知的 FFI 成本（ADR-031「後續（R05.5）」）。
