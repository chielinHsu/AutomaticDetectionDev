# S09 — 契約測試與 A/B 比較：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S09-contract-ab.md`](../../plan/S09-contract-ab.md)（含 S01、S03、S06、S07、S08、session B 的提醒）、port 的契約（`ITemplateBuilder.h`、`IPatternSearcher.h`、`ILibraryMatcher.h`）、ADR-010、ADR-016、ADR-017、ADR-021。
> 本檔講「為什麼這樣做」；接受的差異與哪些算 bug 在 [ADR-004](../../adr/004-backend-divergence.md)；圖在 [`diagram.md`](diagram.md)；實測數字在 [`perf.md`](perf.md)。

<!--STE-->
## Purpose
This step proves that the back ends are interchangeable. One test suite runs on each back end.
This step also adds an A/B view. The view shows the results of two back ends on the same sample.
<!--/STE-->

**繁中**：用同一組契約測試跑每個 back end（fake、OpenCV、HALCON），證明它們可以互換（Liskov）；再做一個 A/B 比較的視窗，讓同一個樣本在兩個 back end 上的結果並排、差異看得見。（S09 收工時 HALCON 讀不到 dongle：HALCON 的部分在第一版上通過過，最後一版與 A/B 並排的 HALCON 那一邊未測，見 `perf.md` §6。）

<!--STE-->
## Reason
A common interface does not guarantee a common behavior.
The coordinates, the angles and the scores can be different, and the compiler cannot find it.
Before this step, each back end had its own copy of the contract tests. The copies were almost the same.
A copy can change and the other copies cannot see the change.
<!--/STE-->

**繁中**：介面相同不代表行為相同：座標差半個像素、角度正負號相反、分數超出 0..1、視窗搜尋回傳視窗外的元件——都編譯得過。S03、S07、S08 各寫了一份契約的草稿（`tst_fake_backend`、`tst_opencv_backend`、`tst_halcon_backend`），兩份端到端測試除了名字幾乎一樣（`diff` 只差 back end 的 id 與 HALCON 強雜訊的角度上限）。各自一份，就會各自漂移。

<!--STE-->
## Effect
One suite runs on each registered back end. Each back end is one row in the log.
A back end that is not in the build, or that has no license, shows SKIPPED with the reason.
The tolerances are in one file, with the evidence for each number.
The A/B view shows three groups: found by both, found only by A, and found only by B.
For each difference, the view tells why the other back end has no match.
<!--/STE-->

**繁中**：三個契約測試執行檔（`tests/contract/`），每個測試函式「一個 back end 一列」；沒編進來或沒有授權的那一列是 **SKIPPED 並寫出原因**（S09 開發途中 HALCON 讀不到 dongle，這一點實際發生過，log 見 `perf.md`）。容差只在 `tests/contract/ContractTolerances.h`，每個數字寫明依據。A/B 比較視窗：兩邊都找到＝綠圓、只有 A＝藍方、只有 B＝橘三角（形狀也不同，不只靠顏色），旁邊是各階段的耗時與數量，差異清單說明另一邊為什麼沒有（分數低於它的門檻、只是晶格推算、附近沒有任何候選）。

<!--STE-->
## The contract suite
The suite gets the back ends from the composition root. This is the same list that the program uses.
Each test gets one back end, and it creates the back end only one time.
Some tests check the contract. These tests run on all back ends, also on the fake back end.
Other tests check the accuracy. These tests run only on back ends that read the pixels.
The fake back end does not read the pixels. It gives the answer of its synthetic grid.
<!--/STE-->

**繁中**：資料列來自 `app::makeBackendRegistry()`（產品用的同一份）；每個 back end 在測試行程裡只建一次（HALCON 第一次約 2.8 秒）。兩種測試分開：**契約**（port 標頭寫的那些）每個 back end 都跑，包括假的；**準確度與座標慣例**只跑會讀像素的（假 back end 的結果來自 FakeGrid，不看影像——它的世界由 `tst_fake_backend` 驗證）。這樣假 back end 在完整的建置裡不會永遠有一列 SKIPPED（「跳過」只用在真的少了東西的時候）。

| 執行檔 | 測什麼 |
|---|---|
| `tst_contract_searcher` | `ITemplateBuilder`、`IPatternSearcher`：錯誤碼、分數與排序、分數下限、最多幾個、角度範圍、搜尋區域、空結果、取消（I4 之內）、可重入；座標原點、子影像的原點、角度正負號、角度掃描、三種影像格式、乾淨板子的準確度 |
| `tst_contract_end_to_end` | use case ＋ 每個會讀像素的 back end，8 種合成板子（T6a、T6b 的單元版本）；有效位元數的覆寫 |
| `tst_contract_library_matcher` | `ILibraryMatcher`：辨識正確率、候選排序、位姿誤差、樣本庫大小 N、區域、空的與不能用的候選、錯誤與取消；兩個 back end 結果一致 |

## 每一條契約：違反時會發生什麼壞事

S09 計畫的要求。「誰保證」欄是 port 標頭（或 ADR）的文字；「測試」欄是 `tests/contract/` 裡的函式。

| 契約 | 誰保證 | 違反時會發生什麼壞事 | 測試 |
|---|---|---|---|
| 失敗是 `Error`，不丟例外；影像無效、樣本不在影像內（含 NaN）、參數不合法（分數不在 0..1、角度範圍不是有限值或顛倒）回 `InvalidArgument` | `ITemplateBuilder.h`、`IPatternSearcher.h` | 例外穿過 port 讓 worker 結束、畫面停在搜尋中；不合法的參數被當成「找不到」，使用者會去調門檻，方向完全錯 | `theBuilderRefusesBadInput`、`theSearcherRefusesBadInput` |
| 別家的範本回 `BackendMismatch`（看型別，不是只看名字） | `IPatternSearcher.h` | 執行中換 back end（ADR-021）時舊的範本遇到新的搜尋器：只比名字就會把別的型別當成自己的（未定義行為、當機） | `aForeignTemplateIsRefused` |
| 範本的大小 ＝ 樣本框（對齊像素，差 ≤ 1 px） | `ITemplate::size` | 每個候選的主體框用這個大小：框會太大或太小，CAD 錯 | `theTemplateHasTheSampleSizeAndItsBackEnd` |
| 平的樣本：範本建得出來，回報 `LowContrast`；用它搜尋是成功、空的 | `ITemplate::sampleProblem` | 使用者看到「找不到」而不是「樣本對比太低」，不知道要重框 | `aFlatSampleIsLowContrast` |
| 分數在 [minScore, 1] | `IPatternSearcher.h` | `classify` 與門檻滑桿假設 0..1；超出就是錯的 Found／Rejected | `matchesAreScoredSortedAndInsideTheImage`、`minScoreIsAHardFloor` |
| 依分數由高到低，同分依 y、再依 x；最多 maxMatches 個、而且是最好的那些 | `IPatternSearcher.h` | 截斷時留下錯的元件；同一個輸入兩次結果順序不同，無法重現 | `matchesAreScoredSortedAndInsideTheImage`、`maxMatchesKeepsTheBestOnes` |
| 角度在 [angleMin, angleMax] | `IPatternSearcher.h` | 晶格與框的展開用這個角度：每個框都歪 | `theAngleRangeIsRespected` |
| 中心在搜尋區域內（視窗搜尋只回視窗內的） | `IPatternSearcher.h`、`search_params.h` | 晶格驗證的視窗回傳鄰居：同一個元件被算兩次、缺格看起來被補上了 | `aWindowSearchOnlyReturnsCentersInsideTheWindows`、`windowAndWholeImageAgree` |
| 找不到 ＝ 成功、空的 | `IPatternSearcher.h` | 「無匹配」變成錯誤卡片，use case 停下來，給不出「只有一個」「門檻太高」的建議 | `noWindowsMeansNothingAndNoError`、`anEmptyBoardGivesAnEmptyResult` |
| 取消：盡快回 `Cancelled`，S09 定為 I4 的 100 ms 之內 | `IPatternSearcher.h`、`perf/budgets.json` 的 I4 | UI 不等 worker（latest-wins），但停不下來的 worker 佔住執行緒，下一次搜尋要排隊 | `cancellingDuringASearchStopsItInTime` |
| 可以被多個 worker 同時呼叫，結果相同 | `IPatternSearcher.h`、`ITemplateBuilder.h` | latest-wins 時新舊工作用同一個搜尋器重疊：結果互相污染 | `severalThreadsGetTheSameResult` |
| 座標原點：像素 (i, j) 的中心在 (i ＋ 0.5, j ＋ 0.5)；子影像的座標以子影像的左上角為原點 | ADR-010（`geometry.h`） | 每個框偏半個像素；在子影像上搜尋的結果放錯位置 | `aCleanBoardIsFoundCompletelyAndPrecisely`、`aSubViewHasItsOwnOrigin` |
| 角度：螢幕上逆時針為正 | ADR-010 | 每個轉過的框都左右顛倒 | `theAngleSignFollowsTheConvention`、`everyAngleInTheRangeIsRecovered` |
| 三種正規格式（Gray8、12 bit Gray16、Bgrx32）給同樣的結果 | ADR-014 | 換相機（或換存檔格式）結果就變 | `allCanonicalFormatsGiveTheSameMatches` |
| 每個元件的第一個類型是對的；typeScores 由高到低、在 [minScore, 1]；每比對一個樣本回報一次進度；只掃區域；沒有能用的候選 ＝ `LibraryEmpty`、沒找到 ＝ `NoLibraryMatch`；取消 | `ILibraryMatcher.h` | 整個類型的元件拿到錯的 CAD 框（流程 B 的元件類型錯，框群組跟著錯） | `tst_contract_library_matcher` |

<!--STE-->
## What the suite found
The fake back end accepted an angle range that is not valid. It found nothing and gave no error.
The other back ends gave an error. We changed the fake back end to give the same error.
A search range that is wider than the template range is a real difference.
The contract does not tell what to do. Two back ends give an error, and one back end filters the matches.
We accept this difference because the use case never makes such a search.
<!--/STE-->

**繁中**：契約測試第一次跑就抓到一個 Liskov 的差異：假 back end 收到 NaN 或顛倒的角度範圍時「成功、找不到」，OpenCV 與 HALCON 回 `InvalidArgument`——改假 back end（範本建立器與搜尋器都檢查），和另外兩個一致。另一個差異是 port 沒有規定的：搜尋的角度範圍比範本的寬時，OpenCV、HALCON 回 `InvalidArgument`，假的照樣過濾；use case 永遠用同一組參數建範本與搜尋，看不到這個差別——接受，記在 ADR-004，契約測試接受兩種行為（錯誤，或結果都在範圍內）。其餘每一條，三個 back end 都一樣。

<!--STE-->
## How we know that the suite can find errors
A test that never fails proves nothing. So we broke the code on purpose, one change at a time.
We removed the window filter of OpenCV. We reversed the sort of the fake back end.
We removed the angle check of the fake back end. We changed the sign of the OpenCV angle.
The suite failed each time, on the correct back end. Then we restored the code.
<!--/STE-->

**繁中**：變異測試（一次改一個地方、只重建那個程式庫與契約測試、確認失敗、改回來）：拿掉 OpenCV 的「中心在視窗內」過濾 → `aWindowSearchOnly…(opencv)` 失敗（中心在視窗外 1.5 px 的元件被回傳）；假 back end 的排序反過來 → `matchesAreScoredSorted…(fake)` 失敗；假 back end 的角度檢查拿掉 → `theSearcherRefusesBadInput(fake)` 失敗；OpenCV 的角度正負號反過來 → `theAngleSignFollowsTheConvention(opencv)` 三列都失敗（角度誤差 6.2–9.2°）。每次都只有被改的那個 back end 的那一列失敗。途中發現：在成功的 `Result` 上呼叫 `error()` 會丟例外、整個測試執行檔中止，後面的列都沒有跑——所以契約測試改用 `QCOMPARE_ERROR`（先確認是錯誤，再比錯誤碼）。

<!--STE-->
## The tolerances
The contract is the same for each back end. The accuracy is not part of the contract.
Each back end has its own limits for the accuracy. All the limits are in one file.
Each limit has a reason: a budget, or a measurement from an earlier step.
If a limit is different for one back end, ADR-004 tells why.
<!--/STE-->

**繁中**：`tests/contract/ContractTolerances.h` 是唯一的地方：乾淨板子的位置 0.25 px（也是座標慣例的檢查：差半個像素一定超過）與角度 0.3°、傾斜板子 0.35 px／0.35°、角度掃描「範本角落的位移 ≤ 0.3 px」、視窗與整張影像 0.05 px、子影像 0.01 px、端到端 T6b 的 0.5 px 與角度 0.6°、樣本庫比對 0.5 px、取消 ＝ I4 的 100 ms（有一個測試檢查它和 `perf/budgets.json` 一致）。只有一個上限因 back end 而不同：**強雜訊時 HALCON 的角度**改用「框的角落位移 ≤ 0.5 px」（S08 實測 0.84–0.90°；shape model 只看邊緣點），理由在 ADR-004。

<!--STE-->
## The A/B view
The user teaches a sample in the main window. Then the user opens the A/B view.
The view runs the two back ends one after the other, on the same image and the same sample.
It does not run them at the same time. Thus each back end can use all the threads, and the times are comparable.
The two scores do not use the same scale. Thus each back end has its own threshold in the view.
A change of a threshold compares the results again. It does not search again.
The view does not change the board and does not go into the undo history.
<!--/STE-->

**繁中**：主視窗教好樣本之後（預覽、無匹配時），檢視選單或工具列的〔A/B 比較…〕打開另一個視窗：A 預設是主視窗目前的 back end，B 是註冊順序裡最完整的另一個（主視窗用的是假的而且有兩個以上時，比 OpenCV 與 HALCON）。兩個 back end **依序**跑（不同時跑：每一邊都用得到所有執行緒，耗時才能直接比），在比較自己的 `JobRunner` 上——和主視窗共用執行緒池，但不會互相作廢。配對與分類是 application 層的純函式 `compareProposals`（容差內由近到遠一對一配對，容差 ＝ max(3 px, 元件短邊 × 0.25)），畫面與效能量測（`perf_ab`）用同一份定義。兩邊的分數不是同一把尺（NCC 對邊緣方向的一致度），所以門檻各自一個；改門檻只重新比較。比較不改板子、不進復原堆疊，主視窗照常可以用。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | 配對與分類（`BackendComparison`，application）、背景執行與畫面（`ComparePresenter`）、繪製（`CompareOverlayItem`）、版面（`CompareWindow` ＋ `.ui`）各一個類別；契約、容差、資料列的來源各一個檔案 | `tst_backend_comparison`、`tst_compare` 分開測；容差只在一個標頭 |
| O | 加 A/B 比較沒有改 use case、port、domain；新的 back end 只要在組裝根註冊，契約測試自動多一列 | `git diff` 沒有動 `application/usecases/`、`ports/`、`domain/` |
| L | 同一組契約測試對 fake、OpenCV、HALCON 各跑一次；差異只在 ADR-004 接受的那幾條 | `tst_contract_*` 每一列；變異測試 4 個都被抓到 |
| I | `ICompareSource`（1 個方法）、`ICompareEntryView`（1）、`ICompareView`（4）、`ICompareController`（4）；主視窗只看到入口 | 標頭 |
| D | `ComparePresenter` 只認識 application 的 `SearchEngine` 工廠與窄介面；具體的 back end 由組裝根給 | `check-deps`；`tst_compare` 用測試台的假 back end |

## 如何驗證

- [x] 建置通過：Release、Debug、`-NoOpenCv`、`-NoHalcon`、兩個都關（數字見 `docs/changelog-ai.md` 本步驟那一筆）
- [x] 測試通過（fake 與 OpenCV 的列）：`tst_contract_searcher`、`tst_contract_end_to_end`、`tst_contract_library_matcher`、`tst_backend_comparison`、`tst_compare` 與既有的全部
- [ ] **HALCON 的列在最後一版的程式上沒有跑過**：2026-10-09 12:39 起 HALCON 讀不到 dongle（錯誤 2300）；最後一次三個契約測試含 HALCON 全部通過是 12:3x 的第一版。使用者決定 dongle 相關的部分記下來、等之後再測（`perf.md` §6）
- [x] 無授權的環境：契約測試的 HALCON 列是 SKIPPED（附原因），不是 PASSED——這次實際發生，log 見 `perf.md` §1
- [x] `node tools/ste-lint.mjs --strict` 無 error
- [x] `perf.md` 的 UI 指標已量測並與 `perf/budgets.json` 比對（U1a、U1b）；A/B 並排只有 OpenCV 那一邊（HALCON 未測）
