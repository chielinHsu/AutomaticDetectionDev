# R05 狀態

> 快照，不是歷史：只寫「現在」（歷史在 `changelog.md`，3.0.0 之前在 `docs/legacy/changelog-ai.md`；細項狀態在 `plan.md` 的細項表）。

- 更新日期：2026-10-27
- 狀態：進行中（R05.1、R05.2 完成；tag `r05.1-done`、`r05.2-done`，只在本機）
- 分支：下一步從 `main` 開 `refactor/r05-3-match-region`

## 目前細項

- R05.3 比對區與特徵擷取（`match_region.cpp`、`feature_extract.cpp`），尚未開始。

## 阻礙

- 無。

## 待使用者決定（R05.5 時問）

- `AOI_DOMAIN_IMPL` 預設改 `rust` 之後，C++ 的重複實作（`src/domain/lattice/legacy/` 等）與 `cpp` 選項要不要刪。
- 效能預算與基準線是否因 Rust 版重建（逐點呼叫累加器每次多約 11 ns，FULL-10000 約 480 → 1 050 ms；資訊性指標）。

## 下一步

1. R05.3：依 `plan.md` 的「Start prompt（R05.2 起的共通格式）」開新 session，先量 before 再搬。
2. R05.4、R05.5。

## 接續資訊

- 做法照 R05.1／R05.2 的模式：C++ 原樣搬到 `src/domain/lattice/legacy/`（`aoi::legacy`），公開 .cpp 轉呼叫 legacy、`*_rs.cpp` 經 C ABI 呼叫 Rust；C 標頭 `src/domain/ffi/aoi_domain_rs.h` 與 `ffi*.rs` 一起改並把 `AOI_RS_ABI_VERSION` 加 1（目前 2）。細節與數字在 ADR-031 的「後續」。
- `aoi::legacy` 裡呼叫同名函式要寫 `legacy::`（ADL）；Rust 的整數運算照 MSVC 用 `wrapping_*`，`std::min/max/clamp` 的 NaN 行為與 `static_cast<int>` 超出範圍的結果用 `cpp.rs`。
- 差異測試 `tst_domain_rs_diff`（只在 rust 建置）164 項：整數與索引完全相同，浮點數 `agree()` 相對 1e-12（理由在檔頭）；R05.2 時約 177 萬個浮點值一位不差。
- `AiInspTool/` 底下有 2026-10-27 13:00 的完整原地建置（不是 R05 產生的，沒有動）：改到有原地 `.moc` 的測試檔時，新的測試函式可能不執行，先看 log 的項數。
- Debug＋Cpp 遇過一次 `LNK1236`（`.obj` 寫壞），刪掉那個 `.obj` 重建即可。效能量測時不要有其他建置在跑；before／after 交替。
- 對時間敏感的測試（`tst_cli` 的 HALCON、`tst_presenter`、`tst_contract_searcher` 的取消延遲）在機器滿載時會失敗，單獨重跑；Debug 的 `tst_presenter` 2 秒忙碌等待本來就不穩定。
- 範圍外：application、presentation、OpenCV、HALCON。
