# ADR-031 Rust 混合架構：domain 的演算法先搬進 Rust crate，C++ 介面不變

- 日期：2026-10-27
- 決定者：使用者（範圍：混合架構、第一階段只搬 domain 的演算法、C++ 介面不變、用既有測試做差異驗證）；Claude Code (Claude Opus 5.5)（技術做法：C ABI、零外部 crate、qmake 呼叫 cargo、兩套實作並存的驗證方式）
- 狀態：採用（技術做法 2026-10-27 在 R05.1 實測確認，沒有推翻任何一項；補充與細化見「後續（2026-10-27，R05.1 實測）」；R05.2 晶格見「後續（2026-10-27，R05.2 晶格）」；R05.3 比對區與自動擷取見「後續（2026-10-27，R05.3 比對區與自動擷取）」）

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

## 後續（2026-10-27，R05.2 晶格）

R05.2 把晶格的其餘部分（`lattice_accumulator`、`spatial_grid`、`vector_clustering`、`basis_finder`、`cell_components`、`lattice_acceptance`、`lattice_estimator`、`lattice_predictor`）照 R05.1 的模式搬進 Rust（`rust/aoi_domain_rs/src/lattice/`，逐檔對應 C++）。「決定」的做法沒有推翻；細化如下。

- **估計一次 FFI 呼叫**：`LatticeEstimator::fit`／`fitAll` 在 rust 建置裡呼叫一次 `aoi_rs_lattice_fit`，找基底、RANSAC、精修、連通、接受規則、定形式都在 Rust 那一側，結果是 handle（`aoi_rs_lattice_fit_summary`、`aoi_rs_lattice_fit_model` 讀、`aoi_rs_lattice_fit_free` 釋放）。其他公開的類別與 `detail::` 函式（`SpatialGrid`、兩個分群策略、`BasisFinder`、`largestComponent`、接受規則、`LatticePredictor`、`LatticeAccumulator`）也各自轉呼叫 Rust，給直接用它們的呼叫端（application 的 use case 用累加器與預測器；測試直接測規則）。長度不固定的輸出一律是 handle；長度有上限的由呼叫端配置。C ABI 版本 2。
- **分群策略**：內建的 `GridVoteClustering`、`GreedyClustering` 用列舉＋參數（`AOI_RS_CLUSTER_GRID_VOTE`、`GREEDY`）；其他的（測試注入的 `CountingClustering` 等）用 C 函式指標＋context 回呼（`lattice/lattice_rs_bridge.cpp` 的 `ClusteringBridge`）。判斷用 `dynamic_cast` 到 `final` 的內建類別，參數經 friend `detail::ClusteringAccess` 讀（公開介面不變）。回呼裡的 C++ 例外不穿過 Rust 的堆疊：trampoline 用 `catch (...)` 記下 `std::exception_ptr`、回 `AOI_RS_CALLBACK_FAILED`，Rust 中止這次估計，C++ 的包裝重新丟出原本的例外（`tst_domain_rs_diff` 測 `std::runtime_error` 原樣回到呼叫端）。回呼的輸出違反 `IVectorClustering` 的契約（群數比向量多、標號不小於群數）時，C++ 的實作是未定義行為，rust 建置明確丟 `std::logic_error`。
- **公開標頭只改私有部分**：`SpatialGrid` 改 pimpl（兩種建置的標頭相同；不可複製、可以移動，`cellSize()` 不再 inline；專案裡沒有複製它的地方）。Rust 的空間格子自己保存一份點（不借用 span），契約「points 要活得比它久」對 C++ 的實作仍然成立。`LatticeAccumulator` 是值型別（放在 `LatticeModel::equations`，會被複製），所以不用 handle：私有的 17 個欄位合成一個 `detail::LatticeSums`（定義在同一個標頭），兩種實作都直接對它操作——C++ 的運算改成對這個結構的自由函式（`legacy::accumulate`、`twoDimensional`、`solve`、`residualSumOfSquares`），Rust 直接拿它的位址（欄位和 C 的 `AoiRsLatticeSums` 一一相同，`lattice_rs_bridge.cpp` 逐欄 `static_assert(offsetof…)`）。另有 friend `detail::LatticeAccumulatorAccess`（`lattice/lattice_accumulator_access.h`）讓 legacy 的估計器與 Rust 的結果換進 `LatticeModel::equations`。**先做的版本每次呼叫把狀態換成 legacy 物件或 C 結構再換回來，逐點加入的量測（`INC`／`FULL`）cpp 建置慢 2–4 倍、rust 建置慢 3–9 倍；改成零複製之後 cpp 建置回到和搬之前相同**（數字見「效能」）。
- **legacy 的範圍**：8 個 .cpp 用 `git mv` 搬到 `src/domain/lattice/legacy/`（歷史跟著走），運算一行沒改（累加器只把成員 `n_` 換成 `sums.n`，見上一點），只改命名空間（`aoi::legacy`，檔頭 `using namespace aoi::detail` 取得 `lattice_detail.h` 的小工具）與限定名稱：呼叫搬過來的同名函式一律寫 `legacy::`（引數是 `aoi::detail::LatticeCandidate` 時，ADL 也會找到 `aoi::detail` 的同名函式）。legacy 的估計器用 legacy 的累加器、找基底、連通與接受規則，結果放進公開的 `LatticeModel`（`equations` 經 friend 換過去），所以差異測試可以直接比兩邊的 `LatticeFit`。`LatticeEstimator::run` 在 legacy 裡是自由函式 `legacy::fitLattices`；建構子、`fit`、`fitAll` 留在公開的包裝（cpp 與 rust 各一份，十行）。
- **Rust 照 C++ 寫**：運算順序、`std::min`／`max`／`clamp` 的 NaN 行為、MSVC `static_cast<int>` 超出範圍得到 `INT_MIN`（R05.1 的這些小函式搬到 `cpp.rs` 共用）、`std::abs(INT_MIN)`、有號整數溢位（Rust 用 `wrapping_*`，Debug 的 Rust 才不會 panic）、C++ 比較函式換成 `Ordering`（`by_less`）、`std::pair` 的 `operator<`（`pair_less`，−0 與 +0 相等）、`nth_element` 用 `total_cmp`（點都是有限值，只差在零的正負號，選出的值算出的距離相同）。配置失敗一律 `try_reserve`（`fallible.rs`），回 `AOI_RS_NO_MEMORY`、C++ 丟 `std::bad_alloc`。
- **唯一刻意的差別**：`largestComponent` 的雜湊表那一條路依 (j, i) 排序，C++ 的 `std::sort` 對「同一個格子出現兩次以上」的輸入不保證相等元素的順序，Rust 用穩定排序。估計器的候選每格只有一個點，所以估計的結果不受影響；差異測試的連通比對只用每格一次的輸入（寫在測試與 `cell_components.rs`）。
- **差異測試（`tst_domain_rs_diff`，Release、rust 建置）**：164 項全部通過。整數、索引、格子、布林、沒有晶格的原因完全相同；浮點數的容差仍是相對 1e-12（理由寫在測試的檔頭），**實際全部一位不差**：累加器 265 790 個值、空間格子 13 968 次查詢（56 060 個值）、分群 34 916 個值、找基底與接受規則 265 組點與 1 410 個候選、估計 1 147 組點（tst_lattice_scenarios 的 1 029 種同 seed＋兩組晶格、一排、間距差 20 倍、散佈、很少的點、重複與非有限值、遠離原點、隨機選項與 Greedy）得到 2 130 組晶格與 59 個「沒有晶格」（88 033 個值）、預測 7 080 次（1 296 768 個值）。所以信心的 `exp` 與分群排序的 `hypot`（兩邊都呼叫 CRT）在這些輸入上也相同。自訂策略的回呼：呼叫次數與結果和 C++ 的估計器相同（內層是 C++ 或 Rust 的策略都一樣）。
- **驗證**：`test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65（完整執行時兩種的 Debug 都只有已知不穩定的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce`（Debug 的 2 秒忙碌等待，進度表「待使用者決定」第 5 項）失敗，單獨重跑通過；Cpp 單獨重跑 4 次 2 次通過，耗時 164–270 s 隨另一個 session 的建置負載變動。Debug＋Cpp 第一次建置遇到一次 `LNK1236`（`lattice_accumulator_legacy.obj` 寫壞），刪掉那個 .obj 重建後正常）；建置記錄 0 個 `LNK4` 警告。跳過的都是環境因素（截圖、EXR、重複性的真實資料、「全部 back end 都編進來」），HALCON 有授權、沒有跳過。對時間敏感的測試在另一個 session 同時建置、機器滿載時失敗過（`tst_cli` 的多行程 HALCON、`tst_presenter` 的切換到 HALCON 30 秒與 Debug 的 2 秒忙碌等待、`tst_contract_searcher` 的取消延遲 147 ms 對 100 ms），機器空下來後單獨重跑都通過；和晶格無關（記憶與 `docs/current-status.md` 的已知不穩定測試）。`check-deps` 0 違規、`ste-lint` 0 error、cargo 的 4 項通過。
- **效能**（Release、這台機器、資訊性指標、沒有預算；搬之前的兩個執行檔備份起來，before／after 交替各 3 次，數字是每次的中位數，ms）：cpp 建置的 after 與 before 在雜訊內相同（T3 0.60–0.71 對 0.61–0.71、FULL-10000 444–561 對 520–683，雜訊見記憶的「效能量測的雜訊」）。rust 建置的晶格擬合變快：T3 0.57–1.47（before：只有 NMS 在 Rust）→ 0.46–0.66，T3-fit 0.54–1.01 → 0.51–0.64，L-1000-random 2.98–4.65 → 1.80–2.31，L-1000-row 0.50–0.78 → 0.34–0.45，L-10000 4.8–8.9 → 4.7–5.3。逐點呼叫累加器的微基準在 rust 建置變慢：INC-1000 0.040–0.046 → 0.070–0.096、FULL-1000 4.4–5.4 → 10.4–15.7、FULL-10000 478–794 → 1 036–1 079（5 千萬次 `add`，每次多約 11 ns：跨 FFI 不能 inline，和 R05.1 量到的 `NMS-PAIR` 逐次呼叫成本同一個量級）。use case 一次搜尋只對累加器加幾千個點，影響是微秒級；有預算的 T3 變快。
- **原始碼樹裡又有原地建置**：`AiInspTool/` 底下有 2026-10-27 13:00–13:07 的一次完整原地建置（`Makefile`、`.obj`、`.moc`、`bin/AiInspTool.exe`；R05.1 搬走舊的之後才出現，不是 R05.2 的建置產生的，可能是 Qt Creator 的建置資料夾設成原始碼資料夾）。R05.2 沒有動它（可能是使用者的建置）。它不影響這次的結果（`tst_domain_rs_diff` 沒有原地的 `.moc`，新的測試函式確實執行了，log 有 164 項），但 R05.1 記過的問題仍在：之後改到有原地 `.moc` 的測試檔（例如 `tst_match_region`、`tst_lattice*`）時，新加的測試函式可能不執行，要先看 log 的項數。

## 後續（2026-10-27，R05.3 比對區與自動擷取）

R05.3 把 `match_region.cpp`（`PixelMask::count`、`runsFromMask`、`maskFromRuns`、`matchSpecFor`、`matchSettingsFromPixels`）與 `feature_extract.cpp`（`pickBrightBlob`：Otsu、最近的亮塊、8 連通填色；`growMask`）照 R05.1／R05.2 的模式搬進 Rust（`rust/aoi_domain_rs/src/match_region.rs`、`feature_extract.rs`、C ABI 在 `ffi_match.rs`，版本 3）。「決定」的做法沒有推翻；細化如下。

- **legacy 的位置**：兩個 .cpp 用 `git mv` 搬到 `src/domain/legacy/`（不在 `lattice/` 底下，因為它們本來就在 domain 的根目錄），運算一行沒改，只改命名空間（`aoi::legacy`）、`PixelMask::count` 變成自由函式 `legacy::maskCount`、呼叫同名函式寫 `legacy::`（ADL）。四個 `operator==` 是型別本身的比較（「留在 C++：domain 的型別」那一條），分到兩種建置都編的 `src/domain/match_region_equality.cpp`，legacy 裡沒有它們（差異測試的執行檔同時連結兩種實作，`aoi::operator==` 只能有一份）。
- **零複製的輸入**：遮罩的 bits（`AoiRsPixelMaskView` 指向 `std::vector` 的資料）、影像的像素（`AoiRsImage` 指向 `ImageView::data`，Rust 依 `valid()` 的規則算出可以讀的長度）、runs（`aoi::MaskRun` 與 `AoiRsMaskRun` 逐欄相同，`match_region_rs.cpp` 逐欄 `static_assert(offsetof…)`，直接交出 `std::vector<MaskRun>` 的位址）。`PixelFormat` 的數值是 C ABI 的一部分（`AOI_RS_PIXEL_*`，`static_assert`）。有效位元數由 C++ 的 `effectiveBits(image)` 算好傳過去（`image_depth` 留在 C++）。
- **輸出**：長度不固定的遮罩與 runs 是 handle（`aoi_rs_pixel_mask_get`／`aoi_rs_mask_runs_get`，null ＝ 空的遮罩或沒有遮罩），C++ 的 `rs::takeMask`／`takeRuns` 複製一次後釋放；`maskFromRuns` 的 bits 長度已知（寬 × 高），由呼叫端配置、Rust 直接寫（寬或高 ≤ 0 時包裝直接回空的遮罩，和 C++ 相同）。`MatchSpec::mode`、`MatchSettings::mode` 由包裝照抄。
- **MSVC 的 `std::lround`**：實測（MSVC 19.44，/O2）NaN、±∞、±1e20、2147483647.5、−2147483648.5 都得到 **0**（不是 `static_cast<int>` 的 `INT_MIN`）；`0.49999999999999994` 是 0（不是 floor(x + 0.5)）。`cpp.rs` 加 `msvc_lround`（`f64::round` ＋ 超出 `long` 時 0）與它的單元測試。亮度的 `lround`、遮罩取樣的外接矩形都用它。
- **差異測試（`tst_domain_rs_diff`，Release、rust 建置）**：169 項全部通過（新增 5 項）。遮罩（位置、大小、每個 bit）、runs、Otsu 的門檻、找不找得到完全相同；浮點數（`matchSpecFor` 的矩形 15 144 個值、`matchSettingsFromPixels` 的 µm 17 576 個值）**全部一位不差**。輸入：4 000 個遮罩與 4 000 份不合法的 runs（超出格子、反過來、重疊、寬高 ≤ 0、bits 的個數不對）；5 000 個 `matchSpecFor`（1 963 個有遮罩，校正值 0、負的、NaN、∞）；5 000 個 `matchSettingsFromPixels`（2 869 個有遮罩）；2 500 張隨機影像（五種格式、stride 有填充、Gray16 的列起點不對齊、有效位元數 0／8–16／20）× 3 次點擊，6 212 次找到亮塊；4 000 次外擴（半徑 −3..12，裁切框部分在外、空的、NaN）。比對函式 `agree()` 原本把兩邊都是 −∞ 判成不同（∞ − ∞ ＝ NaN），加上「完全相等就一致」（只會讓相同的值一致，沒有放寬容差）。
- **真實影像**：`perf_match_region_real` 的影像 `FovImages1\0_0_HA.tif` 在 2026-10-20 換過（ADR-028 的量測是 2026-10-15），預設的點擊位置現在落在 47 × 60 的亮塊上、標準答案只找到 1 根端子，`initTestCase` 在搬之前的執行檔就失敗——**召回與位置沒有量到**（要使用者更新 `AOI_REAL_MATCH_CLICK`／`BODY` 或換回原本的影像）。能比的只有真實影像上的擷取：before（C++）與 after（Rust）的亮塊、像素數、重心、Otsu 門檻完全相同（47 × 60 at (2118, 1955)、1 482 px、(2135.52, 1988.76)、150）。
- **效能**（Release、這台機器、資訊性指標、沒有預算；新的 `tests/domain/perf_match_region`，搬之前的執行檔備份起來，before／after 交替各 3 次，數字是每次的中位數，ms）：cpp 建置的 after 和 before 在雜訊內相同。rust 建置（before 是 C++ 的比對區）：`MR-PICK-148` 0.93–1.19 → 0.95–1.07、`MR-PICK-600` 16.4–18.0 → 17.5–17.7（相同）；`MR-GROW-200x160-r12` 8.2–10.5 → 7.0–7.2、`MR-RUNS-600` 0.34–0.38 → 0.23–0.27、`MR-SETTINGS-148` 0.020–0.023 → 0.017–0.018（較快）；`MR-SPEC-148` 0.089–0.128 → 0.12–0.17、`MR-SPEC-148-resample` 0.25–0.28 → 0.39–0.41（較慢，約每個輸出像素多 2 ns：最近鄰取樣的內層迴圈每次呼叫 `at()` 都重算 `is_empty()` 與邊界檢查，C++ 那一份被 inline；一個樣本只算一次，微秒級）；`MR-COUNT-10000`（一萬次小遮罩的 `count`）0.256–0.269 → 0.32–0.39（每次呼叫多約 6 ns，跨 FFI，和 R05.1、R05.2 量到的同一個量級）。
- **驗證**：`test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65，0 個 `LNK4` 警告，跳過的都是環境因素（和 R05.2 相同的 6 個），HALCON 有授權、沒有跳過；Debug＋Cpp 65／65、Debug＋Rust 65／66（只有已知不穩定的 `tst_presenter` 2 秒忙碌等待失敗，單獨重跑通過）。cargo 的 4 項（36 個單元測試）、`check-deps`、`ste-lint` 通過。

## 後續（2026-10-27，R05.4 重複性的樣本統計）

R05.4 把 `repeatability.cpp` 的 `summarize`（有限值的個數、最小、最大、Welford 的平均、Range、樣本標準差 n − 1、P/T ＝ 6σ ÷ SPEC）照 R05.1–R05.3 的模式搬進 Rust（`rust/aoi_domain_rs/src/repeatability.rs`，C ABI 在 `ffi_repeatability.rs`，版本 4）。「決定」的做法沒有推翻；細化如下。

- **搬什麼**：只有 `summarize` 是演算法。`valueOf`（`Statistics` 的存取）與 `defaultMetrics`（預設的指標表：11 個指標的 SPEC，是資料）照「型別留在 C++」那一條，分到兩種建置都編的 `src/domain/repeatability_types.cpp`（內容沒有改）；`repeatability.cpp` 用 `git mv` 搬到 `src/domain/legacy/repeatability_legacy.cpp`，運算一行沒改，只改命名空間（`aoi::legacy`）。application（`RepeatabilityAnalysis`）與 presentation 沒有改。
- **零複製與結構**：值直接借用 `std::span<const double>` 的資料（`static_assert` double 是 IEEE 754 binary64；`count` 是 0 時指標可以是 null，Rust 那一側允許）。輸出是呼叫端配置的固定大小結構 `AoiRsStatistics`（88 byte：`std::optional<double>` 拆成 `has_*` 與值），`repeatability_rs.cpp` 逐欄 `static_assert(offsetof…)`、Rust 用 `offset_of!` 檢查同樣的位置。Rust 不配置記憶體，所以沒有 `AOI_RS_NO_MEMORY`；null 指標與 panic 照舊回報、C++ 丟 `std::logic_error`。
- **Rust 照 C++ 寫**：運算順序相同（不合併成 fma），`std::min`／`std::max` 用 `cpp.rs` 的 `cpp_min`／`cpp_max`（差與平方和溢位成 NaN 時 `std::max(0.0, NaN)` 是 0，`cargo test` 有一項專門測這個），`static_cast<double>(size_t)` 對應 `as f64`。
- **差異測試（`tst_domain_rs_diff`）**：171 項（新增 2 項）。200 000 組隨機輸入（長度 0–64；像真實量測的 µm、很大（溢位）、次正規數、全部相同、±0、混著 NaN 與 ±∞；SPEC 沒有、0、−0、負的、NaN、±∞、很小、很大）：個數、`complete()`、每個 optional 有沒有值完全相同，988 059 個浮點值**全部一位不差**（Release 與 Debug 都是）。真實資料：設 `AOI_REPEATABILITY_DATA` 時讀那個資料夾與子資料夾的 BOX LEVEL 報表（測試裡簡化的讀法，domain 的測試不連 storage）；用 `D:\Benano\20261020_20261020_4677_AdvancePin_LeftTopRegion_Proj`（14 個資料夾、309 份 CSV、61 992 組 (Box, 欄)、136 萬個值）比對 371 952 個浮點值，全部一位不差。`tst_repeatability` 的 `realFiveRuns` 要的原始五份 CSV（2026-10-14、703 Box）在這台機器的 D: 找不到，那一項照舊 SKIPPED（**未驗證**）。
- **效能**（Release、這台機器、資訊性指標、沒有預算；新的 `tests/domain/perf_repeatability_stats`，搬之前的兩個執行檔（它與 `perf_repeatability`）備份起來，before／after 交替各 3 次，數字是每次的中位數，ms）：cpp 建置的 after 和 before 在雜訊內相同（RS-SUM-5x2812 0.52–0.64 對 0.52–0.55、RS-SUM-30x2812 2.0–2.6 對 2.0–2.3、RS-SUM-1000000 22–36 對 20–25）。rust 建置（before 是 C++ 的 `summarize`）**變快**：RS-SUM-5x2812（703 Box × 4 指標 × 5 次量測，2 812 次跨 FFI）0.41–0.61 → 0.20–0.66、RS-SUM-30x2812 1.69–2.48 → 0.67–1.48、RS-SUM-1000000 18.2–26.1 → 7.2–13.9（迴圈本身：Rust 的 `Option` 更新編得比 `std::optional` 好；逐次呼叫的 FFI 成本仍在，只是比省下的少）。use case 層的 RS-ANALYZE（`perf_repeatability`，4 組 × 30 次 × 703 Box × 4 指標，大部分是 map 的建立）兩種建置的 before／after 都在 179–258 ms 的雜訊內。
- **驗證**：`test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65，四次建置都是 0 個 `LNK4` 警告；跳過的都是環境因素（和 R05.3 相同的 6 個，rust 建置另加差異測試的真實資料那一項），HALCON 有授權、沒有跳過；XFAIL 只有既有的兩列。cargo 的 4 項（41 個單元測試）、`check-deps`（0 違規）、`ste-lint`（0 error）、`perf-compare --self-test` 通過。
