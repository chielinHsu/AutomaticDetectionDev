# R05 狀態

> 快照，不是歷史：只寫「現在」（歷史在 `changelog.md`，3.0.0 之前在 `docs/legacy/changelog-ai.md`；細項狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-27
- 狀態：進行中（R05.1、R05.2、R05.3 完成；tag `r05.1-done`、`r05.2-done`、`r05.3-done`，只在本機）
- 分支：下一步從 `main` 開 `refactor/r05-4-repeatability`

## 目前細項

- R05.4 重複性統計（`repeatability.cpp`），尚未開始。

## 阻礙

- 無。

## 待使用者決定

- R05.5 時問：`AOI_DOMAIN_IMPL` 預設改 `rust` 之後，C++ 的重複實作（`src/domain/legacy/`、`src/domain/lattice/legacy/`）與 `cpp` 選項要不要刪。
- R05.5 時問：效能預算與基準線是否因 Rust 版重建（逐次呼叫的小函式每次多約 5–15 ns：累加器、`PixelMask::count`；`matchSpecFor` 的遮罩取樣慢約 0.03–0.13 ms；都是資訊性指標）。
- `perf_match_region_real` 的影像 `FovImages1\0_0_HA.tif` 在 2026-10-20 換過，預設的點擊位置與 CAD 框對不上（標準答案只找到 1 根端子，搬之前就失敗）：要更新 `AOI_REAL_MATCH_CLICK`／`AOI_REAL_MATCH_BODY` 的預設，或指向原本的影像。R05.3 因此沒有量到真實影像的召回與位置。

## 下一步

1. R05.4：依 `plan.md` 的「Start prompt（R05.2 起的共通格式）」開新 session，先量 before 再搬。
2. R05.5。

## 接續資訊

- 做法照 R05.1–R05.3 的模式：C++ 原樣 `git mv` 到 legacy（domain 根目錄的檔案放 `src/domain/legacy/`，晶格放 `src/domain/lattice/legacy/`；`aoi::legacy`），公開 .cpp 轉呼叫 legacy、`*_rs.cpp` 經 C ABI 呼叫 Rust；C 標頭 `src/domain/ffi/aoi_domain_rs.h` 與 `ffi*.rs` 一起改並把 `AOI_RS_ABI_VERSION` 加 1（目前 3）。型別本身的函式（例如 `operator==`）兩種建置共用一份，不搬。細節與數字在 ADR-031 的「後續」。
- `aoi::legacy` 裡呼叫同名函式要寫 `legacy::`（ADL）；Rust 的整數運算照 MSVC 用 `wrapping_*`；`std::min/max/clamp` 的 NaN、`static_cast<int>` 超出範圍（`INT_MIN`）、`std::lround` 超出範圍（0）用 `cpp.rs`。
- 差異測試 `tst_domain_rs_diff`（只在 rust 建置）169 項：整數、遮罩與索引完全相同，浮點數 `agree()` 相對 1e-12（理由在檔頭）；到 R05.3 為止實際全部一位不差。
- 新的效能量測要開新的執行檔或加在沒有原地 `.moc` 的檔案：`AiInspTool/` 底下 2026-10-27 13:00 的原地建置留下了 `tests/domain/.moc/`（含 `perf_lattice`、`tst_match_region`），在這些檔案加新的測試函式不會執行。R05.3 因此新增 `perf_match_region`（`tools/perf-compare.mjs` 的來源 `match_region`）。
- 對時間敏感的測試（`tst_cli` 的 HALCON、`tst_presenter`、`tst_contract_searcher` 的取消延遲）在機器滿載時會失敗，單獨重跑；效能量測時不要有其他建置或 cargo 在跑（R05.3 的第一次 before 就因為同時跑 cargo 慢了 2 倍）。
- 範圍外：application、presentation、OpenCV、HALCON。
