# ADR-031 Rust 混合架構：domain 的演算法先搬進 Rust crate，C++ 介面不變

- 日期：2026-10-27
- 決定者：使用者（範圍：混合架構、第一階段只搬 domain 的演算法、C++ 介面不變、用既有測試做差異驗證）；Claude Code (Claude Opus 5.5)（技術做法：C ABI、零外部 crate、qmake 呼叫 cargo、兩套實作並存的驗證方式）
- 狀態：採用（技術做法 2026-10-27 在 R05.1 實測確認，沒有推翻任何一項；補充與細化見「後續（2026-10-27，R05.1 實測）」）

## 背景

使用者 2026-10-27 參考一段 ChatGPT 對話（影片《為什麼 AI 巨頭都在轉向 Rust？》的討論），要求用 Rust 重構本專案。該對話的結論是：Qt 是 UI 框架、Rust 是語言，兩者不互相取代；既有的 Qt＋C++ 專案建議逐步導入 Rust（Qt＋C++＋Rust 混合），OpenCV、HALCON、CUDA 這類 C++ 生態留在 C++；Rust 適合「要快又不能出錯」的中間層；AI 改寫的程式要靠編譯器、測試與差異比對驗證。

本專案的實際狀況（2026-10-27 盤點）：

- `src/` 約 2.2 萬行（非空行；domain 約 3 000、application 約 4 000、infrastructure 約 5 200、presentation 約 9 200），測試約 2.6 萬行。
- UI 是 Qt Widgets＋QGraphicsView＋Designer `.ui`（ADR-001、ADR-022）。Qt 官方的 Qt Bridge for Rust 以 QML 為主，而且仍是 Beta；CXX-Qt 把 Rust 物件做成 QObject，也不是用來取代 Widgets 的畫布。
- 建置只用 qmake＋jom（ADR-018）。ChatGPT 的範例用 CMake、Qt 6.8，本專案是 qmake、Qt 6.12.0，也沒有用到 CUDA。
- domain 層本來就只用 C++ 標準庫（`check-deps` 擋 Qt 與視覺函式庫），而且有大量可以拿來做差異驗證的測試：晶格 1 029 種情境（`tst_lattice_scenarios`）、與 S00 示範頁的交叉驗證（`tst_lattice_crosscheck`）、NMS、比對區、重複性統計、契約測試、效能量測 T3（`perf_lattice`）。
- 本機原本沒有 Rust；使用者 2026-10-27 用 rustup 安裝了 `rustc 1.99.1`、`cargo 1.99.1`，工具鏈 `stable-x86_64-pc-windows-msvc`（含 clippy、rustfmt），位置 `%USERPROFILE%\.cargo\bin`。

## 決定

### 範圍（使用者決定）

- 採混合架構。**第一階段（R05）只把 domain 的演算法實作搬進 Rust**：`lattice/` 的全部 .cpp（NMS、空間格子、正規方程累加器、分群、基底、格子分量、接受規則、估計、預測）、`feature_extract.cpp`、`match_region.cpp`、`repeatability.cpp`。
- **C++ 介面不變**：`src/domain/` 的標頭（公開的類別、函式、型別）照舊，application、infrastructure、presentation 與測試不必改呼叫方式。私有成員可以改（例如改成持有 Rust 物件的 handle）。
- 留在 C++：domain 的型別標頭（`geometry.h`、`match.h`、`component.h`、`box_expander.h` 等，全部各層共用、多半是 inline）、影像緩衝（`image_buffer`、`image_copy`、`image_depth`：跨 port 的所有權規則在 ADR-014，搬過去沒有好處），以及 application 以外的每一層（Qt Widgets、OpenCV、HALCON 的 adapter）。
- application 層要不要搬是第二階段的事，R05 完成後再請使用者決定。

### 技術做法（Claude Code 提出；R05.1 實測確認）

1. **crate 的位置與規則**：`rust/aoi_domain_rs/`（`crate-type = ["staticlib", "rlib"]`）。`[dependencies]` 必須是空的（對應「domain 只用標準庫」）；由檢查擋下。`Cargo.lock` 入版控。
2. **FFI 用 C ABI**（`extern "C"`、`#[repr(C)]`），手寫一份 C 標頭 `src/domain/ffi/aoi_domain_rs.h`（只 include `<stddef.h>`、`<stdint.h>`；`check-deps` 會把它算成 domain）。C++ 的 .cpp 變成薄的包裝：把 C++ 型別轉成 `#[repr(C)]` 的結構、呼叫 Rust、把結果放回 `std::vector`。
3. **所有權**：誰配置誰釋放。Rust 回傳的陣列或物件一律是 handle，C++ 包裝複製結果後呼叫對應的 `aoi_rs_*_free`；有狀態的類別（例如 `OverlapIndex`）持有 Rust 物件的 handle，解構時釋放（不可複製、可以移動）。
4. **panic 不跨 FFI**：每個 `extern "C"` 函式用 `std::panic::catch_unwind` 包起來，回傳狀態碼；C++ 包裝照原本的契約處理（原本 `noexcept` 的函式回傳安全值，原本可能丟 `std::bad_alloc` 的照舊丟）。panic 屬於 bug，要有測試。
5. **自訂的分群策略**：`IVectorClustering` 留在 C++（測試會注入自訂的策略）。內建的兩種（`GridVoteClustering`、`GreedyClustering`）用列舉交給 Rust；自訂的策略用 C 函式指標＋`void*` context 回呼。
6. **建置**：qmake 新增一個子專案（`AiInspTool/src/domain_rs/`），在 domain 之前呼叫 `cargo build --manifest-path rust/aoi_domain_rs/Cargo.toml --target-dir <建置>/cargo`（Release／Profile 加 `--release`），產出 `aoi_domain_rs.lib`；`aoi_deps.pri` 把它與 `cargo rustc -- --print native-static-libs` 列出的系統程式庫登記成 domain 的外部函式庫（`AOI_EXT_domain`），所以每個連到 domain 的目標都帶得到。`scripts/dev/env.ps1` 把 `%USERPROFILE%\.cargo\bin` 放進 PATH；選到 Rust 實作而找不到 cargo 時 qmake 直接報錯。
7. **選項**：qmake 變數 `AOI_DOMAIN_IMPL=cpp|rust`。遷移期間兩套實作都在 repo 裡：預設 `cpp`，每一步搬完的部分在 `rust` 下跑完整測試；R05 收尾時改預設為 `rust`，C++ 的重複實作要不要刪由使用者決定。`test.ps1` 加 `-DomainImpl Cpp|Rust`。
8. **差異驗證**（使用者要求「用既有測試做差異驗證」）：
   - 同一組既有測試在 `cpp` 與 `rust` 兩種建置各跑一次，兩邊都要全部通過（含 1 029 種晶格情境、S00 交叉驗證、契約測試）。
   - 新增一個差異測試 `tests/domain/tst_domain_rs_diff`：同一個執行檔同時連結舊的 C++ 實作（放在只給測試用的程式庫、命名空間 `aoi::legacy`）與 Rust 實作，用固定 seed 的隨機輸入逐筆比對。整數、索引、列舉的結果要完全相同；浮點數因為運算順序可能不同，容差寫在測試裡並說明理由，不可以為了通過而放寬。
   - Rust 自己的檢查：`cargo test`、`cargo clippy --all-targets -- -D warnings`、`cargo fmt --check`，加進 `test.ps1` 的檢查清單。
   - 效能先量再改（S11 規則）：搬之前用 `perf.ps1 -Filter` 量 before（T3 晶格擬合、NMS），搬之後量 after，前後交替量；變慢超過基準線範圍要回報，不可以自己放寬預算。
9. **一步一個 session**（Step Protocol）：R05.1 建置整合與 NMS 試點 → R05.2 晶格 → R05.3 比對區與特徵擷取 → R05.4 重複性統計 → R05.5 收尾。R05.1 是關卡：建置整合或 CRT 的問題解不掉時，停下來回報，不往下做。

## 否決的替代方案

- **全部改寫成 Rust**（UI 改 Slint／egui，或升級到 Qt Bridge for Rust＋QML）：等於重寫整個產品；Widgets 的畫布、Designer 表單、HALCON 都要重做（HALCON 沒有官方的 Rust 綁定）。使用者選了混合架構。
- **domain＋application 一起搬**：application 有 port、`JobRunner` 的 latest-wins、取消、執行緒與生命週期（ADR-011、ADR-021），風險與工作量大很多；先用 domain 證明建置與驗證的流程可行。
- **用 cxx／CXX-Qt 產生橋接**：要在 qmake 裡加程式碼產生的步驟，而且要從 crates.io 下載依賴（公司網路與供應鏈的風險、離線建置）。domain 的邊界是幾個 POD 結構與陣列，C ABI 就夠；邊界變複雜（第二階段的 application）時再評估 cxx。
- **改用 CMake 或讓 cargo 主導建置**（Corrosion、cxx-qt-build）：違反 ADR-018（只用 qmake、Qt Creator 開 `.pro` 就能建置與除錯）。
- **用 opencv-rust 改寫 OpenCV 的 back end**：不在第一階段；OpenCV 的 adapter 已經有契約測試，搬過去的收益不明。

## 後果

- 建置多一個工具鏈：每台建置的機器都要有 rustup 的 MSVC 工具鏈（`AI_CONTEXT.md` 的環境表要加一列）。Qt Creator 要能找到 cargo（重新開啟 Qt Creator 才會讀到新的 PATH）。
- **要在 R05.1 實測的風險**：
  - CRT：Rust 的標準庫在 MSVC 上連結 release 版的 CRT（`msvcrt`），本專案的 Debug 組態用 `/MDd`；可能出現 LNK4098 或更糟的衝突。要在 Debug、Release、Profile 三種組態都建置並跑測試。
  - `/W4 /WX` 只管 C++ 的編譯警告；連結器的警告要看清楚，不可以用關掉警告的方式蓋過。
  - 跨語言呼叫沒有 inline：`OverlapIndex::overlapsAny` 這種每個框都呼叫的小函式可能變慢，要量。
  - 浮點數：Rust 與 MSVC 的 `f64` 運算一樣遵守 IEEE 754，但運算順序、`fma`、`sqrt` 以外的數學函式（`atan2`、`cos`）的結果可能差在最後一位。差異測試要分出「完全相同」與「容差內」。
- `check-deps` 不掃 Rust：Rust 這一側的「只用標準庫」由「`[dependencies]` 必須空」的檢查保證。
- 分層、依賴表與 `docs/architecture.md` 要在 R05.1 一起更新（依賴表、`aoi_deps.pri`、`check-deps` 的 `RULES` 三邊一起改）。
- 第二階段（application 層）要不要做、FFI 要不要改用 cxx，R05 完成後由使用者決定。

## 後續（2026-10-27，R05.1 實測）

R05.1（建置整合與 NMS 試點）照「決定」的做法完成，關卡通過；沒有推翻任何一項，只有下面幾點補充或細化。

- **CRT（Debug 的 `/MDd`）**：不衝突。`rustc --print native-static-libs` 列出 `kernel32 ntdll userenv ws2_32 dbghelp` 與 `/defaultlib:msvcrt`，但 `dumpbin /directives` 看 `aoi_domain_rs.lib`（dev 與 release 兩種）**沒有任何 `/DEFAULTLIB` 指示**：`/defaultlib:msvcrt` 只是 rustc 給的建議，不在程式庫裡。所以 Debug 的程式連到 C++ 自己選的除錯版 CRT（Rust 的程式碼也用它），沒有 LNK4098。Release、Debug、Profile 三種組態的建置記錄都是 0 個 `LNK4` 警告，連結器的警告沒有關掉任何一個（`aoi_target.pri` 只加了 Rust 的 lib 與上面五個系統程式庫）。能混用的前提是沒有 CRT 的物件跨過邊界：Rust 用自己的配置器（Windows 的 `HeapAlloc`），handle 由 Rust 釋放、陣列由呼叫端配置——就是「誰配置誰釋放」那一條。
- **系統程式庫的清單**寫死在 `AiInspTool/qmake/aoi_rust.pri`（qmake 每個子專案都會讀選項，不能每次都跑 cargo）；`tools/check-rust.mjs` 比對 rustc 實際印出的清單（cargo 會重播快取的訊息，沒有重新編譯也印得出來），換 Rust 版本時清單變了就失敗。
- **qmake 整合**：`AiInspTool/src/domain_rs/domain_rs.pro`（`TEMPLATE = aux`，`first` 依賴一個永遠執行的 `cargo build --lib --locked --offline [--release]` 目標，`--target-dir <建置>/cargo`），`src/src.pro` 讓 domain 依賴它；Rust 的 lib 以外部函式庫 `rust` 登記在 domain 上（`AOI_EXT_domain`），`PRE_TARGETDEPS` 讓改了 `.rs` 時重新連結。cargo 用完整路徑找（`AOI_CARGO`、`%CARGO_HOME%`、`%USERPROFILE%\.cargo\bin`），所以安裝 Rust 之前就開著的 Qt Creator 也建置得起來：用 Qt Creator 的 Profile 參數、PATH 拿掉 cargo 的模擬建置成功。**Qt Creator 的 GUI 本身沒有實際操作驗證**（要使用者在 qmake 步驟的「其他引數」加 `AOI_DOMAIN_IMPL=rust` 試一次）。
- **所有權的細化**：輸出大小有上限的陣列（`aoi_rs_nms_keep` 的索引，最多和輸入一樣多）由呼叫端配置，Rust 只寫進去，不必回傳 handle；有狀態的物件（`OverlapIndex`）才用 handle。配置失敗用 `try_reserve` 回 `AOI_RS_NO_MEMORY`（C++ 包裝丟 `std::bad_alloc`），不讓 Rust 直接結束行程。C 標頭有 `AOI_RS_ABI_VERSION`，包裝第一次呼叫時比對。
- **公開標頭只改私有部分**：`OverlapIndex` 改成 pimpl（`std::unique_ptr<Impl>`），兩種建置的標頭完全相同（不靠巨集切換成員，沒有 ODR 的風險）；因此變成不可複製、可以移動（專案裡沒有複製它的地方），`size()` 不再 inline。C++ 的實作搬到 `src/domain/lattice/legacy/`（命名空間 `aoi::legacy`）：cpp 建置時 `aoi_domain` 編它並轉呼叫，rust 建置時只有測試用的 `aoi_domain_legacy` 編它——C++ 的程式碼只有一份。坑：`aoi::legacy` 裡未限定的 `overlapArea(a, b)` 會因 ADL（引數是 `aoi::RectObj`）同時找到 `aoi::overlapArea` 而模稜兩可，要寫 `legacy::overlapArea`。
- **浮點數**：Rust 照 C++ 的運算順序寫，並模仿 `std::min`／`std::max`／`std::clamp` 遇到 NaN 的行為與 MSVC `static_cast<int>` 超出範圍得到 `INT_MIN` 的行為。差異測試 220 010 對隨機旋轉矩形（含 0°、90°、45° 等特殊角度、零寬、負高、只碰到邊、非有限值），重疊面積與兩種比例**全部一位不差**（Release 與 Debug 都是），所以 Rust 的 `sin`／`cos` 與 MSVC 的結果在這些輸入上相同；測試的容差仍是相對 1e-12（理由寫在測試裡），一位不差的筆數印在 log。NMS 的索引（3 種場景 × 4 種數量 × 6 種容差 × 2 種量法）與 `OverlapIndex` 的回答（6 種格子大小）完全相同。
- **panic**：`aoi_rs_test_panic` 在 Rust 裡 panic，C++ 拿到 `AOI_RS_PANIC`、行程繼續（`tst_domain_rs_diff` 與 `cargo test` 都測）。
- **效能**（Release、這台機器、資訊性指標、沒有預算；交替各 3 次的中位數，before 是搬之前的 C++）：NMS-10000（`keep` 一次呼叫）before 9.2–11.2 ms → cpp 8.5–10.9、rust 5.3–10.0；NMS-1000 0.82–0.92 → cpp 0.70–0.96、rust 0.46–0.62；NMS-IDX-10000（use case 那樣一個框一次 `overlapsAny`＋`add`，每次跨 FFI）before 7.2–10.2 → cpp 6.4–7.7、rust 5.0–12.0（第一次 12.0 是暖機與雜訊，後兩次 5.0–5.6）；NMS-PAIR-20000（2 萬次 `overlapRatio`／`overlapArea`，多半是快速排除）before 0.22–0.29 → cpp 0.19–0.22、rust 0.27–0.49：**逐次呼叫的小函式每次多約 5–15 ns**（跨 FFI 沒有 inline、要複製結構），對 use case 的總時間（毫秒級）看不出來。T3 不受影響（晶格還沒搬）。
- **建置時間**：crate 乾淨建置（release）1.5 s；只改一個 `.rs` 的增量建置 12.4 s（cargo 重編＋75 個執行檔重新連結）；沒有改動時每次建置呼叫 cargo 的成本約 0.1 s。
- **原始碼樹裡的舊產生檔**：`AiInspTool/` 底下有 2026-10-16 原地建置留下的 `.moc`／`.uic`／`.rcc`，qmake 的相依掃描會抓到它們而不重新 moc（新加的 Qt Test 函式不執行）；R05.1 把它們搬到 `build/r05-insource-generated-backup/`。之後不要在 `AiInspTool/` 原地執行 qmake。
