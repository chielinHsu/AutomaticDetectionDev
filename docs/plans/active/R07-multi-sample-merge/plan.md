---
id: R07
title: 多樣本搜尋的合併與驗證規則：樣本越多結果不應越差
type: 修正
status: 進行中
created: 2026-10-28
updated: 2026-10-29
depends_on: []
related: [ADR-025, ADR-026]
branch: fix/r07-multi-sample
---

# R07 — 多樣本搜尋的合併與驗證規則：樣本越多結果不應越差

> 2026-10-28 建立。使用者回報（AdvancePin 的 `0_0_HA.tif`，HALCON）：畫 3 個樣本按〔搜尋〕，找到 332、拒絕 307、「310 個位置被不只一個樣本找到」、「271 個不在陣列的格點上」，畫面上一堆和樣本無關的框；樣本越多越差。

## 目標

同一塊板子上畫幾個樣本（同一種零件的幾個實例、或幾種零件各一個）按〔搜尋〕，結果不比只畫一個樣本差：不多出低分的候選、同一個位置不被幾個樣本互相搶、畫面上不出現使用者沒教過的位置。

## 範圍

- 先量再改（S11 規則）：用使用者的真實影像，1、2、3 個樣本各搜尋一次，倒出候選的來源（Seed／Verified／Predicted）、分數分布、合併與晶格的數字（R07.1）。
- 依數字改 `GenerateObjectsFromSample`（驗證下限）與 `mergeSampleProposals`（合併規則）；規則變動寫進 ADR-025 的「後續」或新 ADR。
- 契約與端到端測試照舊通過；真實影像的量測當 before／after 的證據。

## 不做

- 不接流程 B（樣本庫比對）、不做無樣本的元件提案、不做深度學習（另開計畫）。
- 不改 back end 的搜尋器（HALCON／OpenCV 的分數與參數）。
- 不改 UI 的面板配置（問題 2 另開計畫）。

## 細項

細項表是這個計畫進度的單一事實來源。新展開的細項先標 ⬜；開始 ⏳；完成 ✅ 並補分支／commit 與驗證；有未解決問題 ⚠️；取消 ⛔ 並寫原因。完成的列不刪除。

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| R07.1 | 量測工具 `tests/backend/perf_multi_sample_real`：真實影像上 1／2／3 個樣本的候選來源、分數分布、合併數、晶格；疊圖；before 的數字記進 `status.md` | ✅ | `fix/r07-multi-sample` | `test.ps1 -Perf -Filter perf_multi_sample_real`（`AOI_REAL_IMAGE_DIR` 指到 AdvancePin）兩個 back end 都跑過，數字見下方「量測（before）」 |
| R07.2 | （使用者 2026-10-28 決定：改晶格估計）晶格的索引區域不再是外接矩形：索引的外接矩形 ∩ 影像空間裡沿 u 的外接矩形 ∩ 有兩側證據的空格（`lattice_region.h/.cpp`、Rust 的 `region.rs`；`cellCount`、涵蓋率、`holes` 用它，規則 2 用「證據數」；C ABI 版本 5）；ADR-013 的「後續（R07.2）」 | ✅ | `fix/r07-multi-sample` | `cargo test`／clippy／fmt 通過；`tst_lattice*`（含 1 029 種情境）、`tst_domain_rs_diff` 通過；完整的 `test.ps1`（Rust 預設與 `-DomainImpl Cpp`）見 `changelog.md` |
| R07.3 | 回歸測試：`tst_lattice` 加三個交錯陣列的測試、差異測試加 12 種交錯情境、Rust 的 `region.rs` 單元測試；`LatticeTestSupport` 的標準答案用同一條證據規則；說明頁的資料重新產生；真實影像的 after 數字（下方） | ✅ | `fix/r07-multi-sample` | 同上 |
| R07.4 | 原本的 R07.2（驗證下限相對於種子分數、合併時同類型優先與衝突標記、同一種零件的幾個樣本只擬合一組晶格）：等 R07.2 的 after 數字再決定要不要做 | ⛔ | | 取消（2026-10-29，使用者同意）：R07.2 的 after 數字顯示 3 個樣本沒有比 1 個差（離格 251 → 0、合併後沒有多出框），主因是晶格不是合併規則；需要時另開計畫 |
| R07.5 | 重現並區分四個回歸失敗的建置產物、Rust/C++、FFI 與標準答案原因；保留 R07 原有工作 | ✅ | 未提交 | PARTIALLY VERIFIED：3 組為過期 C++ 產物；Lattice 為範圍與缺件證據的測資問題；紅→綠各兩次，詳見 tasks/regression-repair.md |
| R07.6 | 依證據修復根因並鎖定回歸；不放寬既有斷言、不回復交錯陣列／長槽功能 | ✅ | 未提交 | PARTIALLY VERIFIED：Rust／C++ Lattice 各 77 passed；精確驗證 6／15 個缺格，另補裁切、無證據角落與棋盤缺件案例；未改演算法 |
| R07.7 | 重建受影響目標、完整回歸、Socket 真實圖操作及配方存讀；更新交接 | ✅ | 未提交；使用者明確禁止 commit／push | PARTIALLY VERIFIED：67/67 執行成功；Socket 10/0/0；保留 7 組既有 skip 與 2 個 XFAIL；交接已更新 |
| R07.9 | 低分的補漏候選要有足夠的鄰居（使用者 2026-10-29 決定）：`GenerateOptions::minNeighboursForWeakHoles`（預設 4；一維兩側都要有）、`GridSummary::unsupported`；鄰居不夠的不回報、不修正晶格；假世界的標準答案 `Oracle::supported`，新增兩個腳本測試；ADR-035 的「後續（R07.9）」 | ✅ | `fix/r07-multi-sample`（commit 見 `changelog.md`） | VERIFIED：完整 `test.ps1`（Release、Rust）66／67，只有 `tst_presenter` 的格數 32 → 28，改後通過；真實影像 HALCON 拒絕 216 → 180（unsupported 37）、OpenCV 42 → 9（33），找到不變 |
| R07.8 | 影像驗證不到的推算格預設不回報（使用者 2026-10-29 決定）：`GenerateOptions::reportUnverifiedHoles`（預設 false）、`GridSummary::unverified`；核心與契約測試打開選項驗證補漏的機制，`tst_presenter` 與新測試驗證預設；量測工具 `perf_multi_sample_real` 多 `AOI_MULTI_MATCH`（矩形／自動擷取的遮罩）、`AOI_MULTI_DUMP`；ADR-035 | ✅ | `fix/r07-multi-sample`（commit 見 `changelog.md`） | VERIFIED：完整 `test.ps1`（Release、Rust）66／67 → 改 `tst_presenter` 的格數後通過，最後 `-NoBuild` 67／67；真實影像 HALCON 推算 13 → 0、OpenCV 7 → 0，其餘數字不變 |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

## 診斷（2026-10-28，從程式碼讀出）

- 每個樣本各跑一條完整的流程（`SearchBoardWithSamples.cpp:39`）：整張影像種子搜尋（`seedMinScore` 0.70）→ 擬合自己的晶格 → 預測缺格 → 視窗驗證（`candidateFloor` 0.30）。候選數隨樣本數線性增加。
- 驗證下限固定 0.30（`GenerateObjectsFromSample.h:33`）：缺格視窗（1.5× 範本）裡分數 ≥ 0.30 的任何東西都成為 Verified 候選。
- 合併只看位置（`BoardProposal.cpp:112–129`）：同一位置分數最高者勝出，不看類型；同一種零件的幾個樣本在 ADR-025 下是幾個類型，互相搶位置。
- 門檻只在 `classify` 套，被拒絕的候選仍畫在畫布上。

## 量測（before，2026-10-28，`perf_multi_sample_real`）

影像 `20261007_4677_AdvancePin_Proj\FovImages\0_0_HA.tif`，第一個樣本 (1621, 328, 100×100) 框一根端子，第二、第三個樣本從第一個的種子（分數 ≥ 0.90）挑離得最遠的。預設的 `GenerateOptions`、`SearchParams`。

| back end | 樣本數 | 候選（seed／verified／predicted） | 合併壓掉 | 門檻 0.87：找到／拒絕／不在格點 | 門檻 0.69：找到／拒絕 |
|---|---|---|---|---|---|
| HALCON | 1 | 214（210／4／0） | 0 | 90／124／75 | 210／4 |
| HALCON | 2 | 256（241／15／0） | 181 | 107／149／87 | 245／11 |
| HALCON | 3 | 282（265／17／0） | 423 | 164／118／132 | 271／11 |
| OpenCV | 1 | 362（360／2／0） | 0 | 242／120／198 | 360／2 |
| OpenCV | 3 | 366（365／1／0） | 710 | 309／57／251 | 365／1 |

- **樣本越多沒有越差**：3 個樣本合併後的候選只比 1 個樣本多 30%（HALCON）／1%（OpenCV），分數 < 0.70 的 Verified 候選 HALCON 最多 13 個、OpenCV 2 個；疊圖（`build/qmake-release/test-work/multi-0_0_HA-*.png`）在長槽與右側鏡像的區塊都沒有框。使用者畫面上的 639 個候選無法用這組樣本重現（樣本位置與參數不明）。
- **真正的問題是晶格**：每個樣本的晶格都只有一維（u ≈ (0, −154)，沒有 v；`lattices 4`），所以缺格一個都沒補（predicted 0），而且 214 個候選有 180 個「不在格點上」。這塊板子是**交錯排列**（相鄰兩欄差半個格距：u ＝ (0, 154)、v ＝ (266, 77)）。
- 診斷（工具裡的 `diagnoseLattices`）：給估計器一個**完美的** 10×6 交錯陣列，它只擬合出 24 個內點的二維（36 格、12 個缺格）加三條一維；少一半的點時擬合成子晶格 (266, 77)／(0, 308)。放寬 `maxGapCells`、`minRowConsistency` 都沒有改變。原因（從 `lattice_acceptance` 與 `LatticeModel::cellCount` 讀出）：缺格數、涵蓋率與預測都用**索引範圍的外接矩形**（`minI..maxI × minJ..maxJ`），交錯陣列的矩形區塊在索引空間是平行四邊形，外接矩形多出一半以上的空格 → 規則 2（缺格 < 內點 − 4）擋掉二維 → 退回一維。這是 ADR-013 規則 2、6 的定義問題，不是參數。

## 量測（after，2026-10-28，R07.2 之後，同一組樣本）

| back end | 樣本數 | 候選（seed／verified／predicted） | 晶格 | 門檻 0.87：找到／拒絕／不在格點 |
|---|---|---|---|---|
| OpenCV | 1 | 374（360／14／0） | 一組二維，u ＝ (0.3, −154)、v ＝ (133, 77)，涵蓋率 0.96 | 242／132／**0**（before 198） |
| OpenCV | 3 | 376（365／11／0） | 同上 | 309／67／**0**（before 251） |
| HALCON | 1 | 214（210／4／0） | 仍是 4 組一維（種子只有 210 個，規則 2 在界線上） | 90／124／180（同 before） |
| HALCON | 3 | 333（264／69／0） | 第三個樣本（250 個種子）是一組二維，補回 80 個 | 164／169／44（before 132） |

- 驗證找到的候選（verified）都是視窗裡真的端子（predicted 0 ＝ 每個缺格都找到東西）；長槽裡沒有任何框（疊圖 `build/qmake-release/test-work/multi-0_0_HA-*.png`）。
- HALCON 的召回（210／約 360）是另一個問題（搜尋器的參數），不在本計畫。

## R07.8 的量測（2026-10-29，使用者實際用的影像）

影像 `20261014_4677_AdvancePin_LeftTopRegion_Proj\FovImages\0_0_HA.tif`（整張都是端子，右下角一個長槽；和 before／after 用的不是同一張），樣本 (427, 634, 100×100)＝使用者畫面上中心 (477, 684) 的 X1。分區：長槽內部 (4340, 3000)–(5314, 3813)、鋸齒帶＝內部往外 90 px、其餘＝真端子。每一格是「分數 ≥ 0.70 的數量／有分數的數量（中位數）」；槽內是沒有影像支持的推算格。

| 範本 | HALCON 真端子 | HALCON 鋸齒 | HALCON 槽內推算 | OpenCV 真端子 | OpenCV 鋸齒 | OpenCV 槽內推算 |
|---|---|---|---|---|---|---|
| 整框 100×100（使用者的設定） | 855／1068（0.89） | 0，最高 0.70 | 4（13 個 unverified） | 1081／1112（0.92） | 1，最高 0.71 | 7（＋5 個 0.45–0.69） |
| 矩形比對區（使用者圖四的紅框 429,659 82×43） | 2250（遍地誤報） | 6 | 0 | 1771（誤報） | 1 | 0 |
| 點端子自動擷取的遮罩（Otsu 142，688 px，41×21）外擴 0 | **1059／1087（0.79）**，離格 0 | 1，最高 **0.48** | 1 | 1191／1209（0.84），約 100 個離格誤報 | 4 | 11 |
| 同上，外擴 8 px（57×37） | 1075／1113（0.81） | 0，最高 0.56 | 8 | 1991（誤報） | 8 | 0 |

- 樣本框裡沒有完整的焊墊（上緣與左下角的白色是鄰居焊墊的邊），自動擷取抓到的是端子。
- 結論：槽內的推算格和範本無關（是角落規則補的）→ R07.8 不回報；HALCON 建議 `M` 點端子（外擴 0），門檻留 0.70；OpenCV 維持整框。
- R07.8 之後（整框）：HALCON `predicted 0 (unverified 13)`、OpenCV `predicted 0 (unverified 7)`，其餘數字不變。
- 工具：`AOI_MULTI_MATCH`、`AOI_MULTI_DUMP`；分區統計的腳本在 session 的 scratchpad（`slot_stats.mjs`，不入版控）；log、CSV、疊圖在 `build/qmake-release/test-work/lt-*`、`after-*`。

## 決策

- 本次（2026-10-28，Codex）追加四個回歸失敗的修復：先檢查靜態庫與測試是否為同一份來源，再查 Rust/C++ 與 FFI；沿用 ADR-013 的交錯陣列及長槽規則、ADR-031 的雙實作契約。以現有最小失敗測試作為回饋迴圈；只在實際證明標準答案錯誤時才修正它。變更前後流程與驗證記錄放在本計畫 tasks/regression-repair.md。本次不提交、不推送。
- R07.1 的結論：多樣本的合併規則不是主因；主因是晶格估計不支援交錯排列（索引範圍用外接矩形）與陣列中間的大片空區（長槽）。使用者 2026-10-28 決定改晶格估計（R07.2）；設計與代價記在 ADR-013 的「後續（2026-10-28，R07.2）」。
- R07.4（原本的合併規則）：after 的數字顯示 3 個樣本沒有比 1 個差（不在格點 0、沒有低分的垃圾候選），先不做；使用者再看到問題時再開。

## 完成定義

- [ ] 建置通過、`scripts/dev/test.ps1` 預設建置全部通過（含契約測試）
- [ ] 真實影像的 after 數字：3 個樣本的低分候選與「不在格點上」的數量不高於 1 個樣本
- [ ] 細項表全部 ✅ 或 ⛔；`status.md`、`changelog.md` 已更新
- [ ] 完成後資料夾搬到 `docs/plans/completed/`，`INDEX.md` 已更新

## Commit 範本

格式一律依 `project-rules.md`「Commit 訊息」；這裡只列內容參考。

```
[Caspar]fix: 多樣本搜尋的驗證下限與合併規則

使用者回報樣本越多匹配越差。原因是每個樣本獨立搜尋、
驗證下限固定 0.30、合併只看位置。改成…

測試 :
…

Co-Authored-By: Claude <模型名稱>

[Test]
Build OK, Test OK
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續專案。回覆一律使用繁體中文（zh-TW），完成時總結修改內容。
請先讀 AGENTS.md、docs/plans/INDEX.md，以及計畫 docs/plans/active/R07-multi-sample-merge/ 的 plan.md 與 status.md，
只做 R07 的下一個細項。完成後更新該計畫的細項表、status.md、changelog.md（ai-handoff skill），
計畫層級的狀態改變時才改 INDEX.md，並依 project-rules.md 的 Git 規則提交。
```
