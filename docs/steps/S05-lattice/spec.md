# S05 — 晶格估計與補漏：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S05-lattice.md`](../../plan/S05-lattice.md)、S00 示範頁（[`../S00-overview/explainer.html`](../S00-overview/explainer.html)）的演算法、S03 的暫代版 `SimpleGrid` 與它的測試。本檔只講「為什麼這樣做」；實測數字在 [`perf.md`](perf.md)；決策在 [ADR-013](../../adr/013-lattice-acceptance-rules.md)；圖在 [`diagram.md`](diagram.md)。

<!--STE-->
## Purpose
This step builds the array rules in plain C++.
The estimator finds the array that the found parts make.
The predictor gives the positions of the missing parts.
The suppressor merges two matches on one part into one match.
<!--/STE-->

**繁中**：用純 C++ 做「陣列規律」：估計器從找到的元件找出它們排成的陣列（晶格），預測器給出缺件的位置，NMS 把同一個元件的重複匹配合併成一個。OpenCV 與 HALCON 兩個 back end 共用。

<!--STE-->
## Reason
A search does not find every part.
Parts in an array have the same pitch and the same angle.
The pitch tells where the other parts must be.
Plain code is easy to test with exact numbers.
A wrong lattice is worse than no lattice, because it shows many false parts.
<!--/STE-->

**繁中**：搜尋不會找到每一個元件；陣列裡的元件間距、角度都相同，間距告訴我們其他元件一定在哪裡。純 C++ 可以用精確數字測試。**錯的晶格比沒有晶格更糟**：它會在畫面上補出一片假的元件——這一步大部分的規則都是為了擋住錯的晶格。

<!--STE-->
## Effect
The estimator fits 1000 points in less than 1.2 ms on this computer. The budget is 2 ms.
The old stand-in took 97 to 134 ms for 4900 points. The estimator takes about 3 ms.
Inside the tested range, every part goes to the correct cell, and no hole is false.
Random points do not make a cloud of false holes.
<!--/STE-->

**繁中**：1 000 點的擬合在這台機器 p95 0.65–1.19 ms（同一個建置連跑三次的範圍，見 `perf.md`；預算 T3 ＝ 2 ms）。約 4 900 點時，S03 暫代版的擬合階段要 97–134 ms（中位數）；現在估計器本身 2.2–2.9 ms，use case 的同一個階段（含 NMS 與缺格）3.3–7.0 ms。在測過的範圍內（每軸均勻雜訊 ≤ 0.1 格距、缺件 ≤ 40%、離群點 ≤ 20 個、7 個角度，420 種組合），每個點都在對的格子、沒有假的缺格、範圍內真的缺件全部補到。散佈、不成陣列的點不會補出一片假缺格。

<!--STE-->
## How the estimator finds the array
First, it measures the vectors from each point to its nearest neighbours.
It groups these vectors and takes the shortest frequent vector as u.
Then it finds v, the second direction, between the rows along u.
It tests a few points as the origin and keeps the best one.
It grows the fit from the origin and doubles the range each time.
A least squares fit refines the lattice at each step.
<!--/STE-->

**繁中**：（1）每個點到最近 4 個鄰居的位移向量，分群（網格投票）後取「票數夠多、最短」的當 u；（2）v：一般陣列的 v 已經在鄰居的方向裡；間距差很多時（例如 20 倍），沿 u 把點分成列，看相鄰兩列之間的位移；（3）RANSAC：試幾個點當原點，數附近有多少點落在格點上；（4）由原點附近開始、範圍每次加倍：指派最近的格子 → 最小平方法（每個軸 3 個參數，和 S00 示範頁相同）。範圍逐步擴大，基底的小誤差才不會在陣列遠端累積成整格的錯位。

<!--STE-->
## Rules that stop a wrong lattice
A large gap splits the parts into two lattices.
A second row needs at least two parts, and most of its parts must sit on nodes.
A lattice must not predict more holes than its evidence supports.
A confidence below 0.15 gives no lattice. Below 0.6, the user must check the result.
<!--/STE-->

**繁中**：（1）連通：格子之間空了超過 2 格就拆成兩組（兩個同樣間距的陣列不會被當成一個）；（2）二維要成立：第二列至少 2 個點，而且第二列以後的點大多真的落在格點上、相鄰兩點的間隔是整數格（擋住「間距不同的幾排」與「斜著穿過的另一排」）；（3）補缺格要有證據：缺格數 ＜ 內點數 − 3（一維）／ − 4（二維）；（4）信心 ＝ 證據 × 精度 × 涵蓋率 × 一致性，低於 0.15 就當作沒有晶格，低於 0.6 標成「建議檢查」。每一條規則都對應下面「已知會失敗的情況」裡的一個情況與它的測試。

<!--STE-->
## Prediction and the refit after verification
The predictor gives the empty cells inside the found range.
It does not extend the array, because it cannot tell a missing edge from the end of the array.
The use case verifies all holes in one batch.
Then it adds each verified part to the normal equations. Each addition costs constant time.
The inferred parts move to the refined positions.
<!--/STE-->

**繁中**：預測器只補「已找到的點的索引範圍之內」的空格，不往外推（整排缺在陣列邊上時，分不出「缺件」與「陣列到此為止」）；由近到遠排序，上限內留近的。use case 仍是一次批次驗證；驗證找到的元件加進正規方程累加器（每個 O(1)，等於 S00 示範頁的 Refit while verifying），沒找到的推算位置跟著修正。

<!--STE-->
## What this step does not do
This step does not change the screen.
The confidence and the suggestion to check are in the result, but the screen does not show them yet.
The use case does not verify in many rounds. The real back end must show the cost first.
<!--/STE-->

**繁中**：這一步不改畫面（`presentation/` 沒動）：信心與「建議檢查」放在 use case 的結果（`GridSummary`），畫面還沒顯示。use case 不做多回合的漸進驗證（每回合一次視窗搜尋）：值不值得，要等 S07 量到真實 back end 每次呼叫的成本再決定；domain 已經提供由近到遠的順序與 O(1) 的重新擬合。

## 元件與檔案

| 檔案（`src/domain/lattice/`） | 內容 | 公開？ |
|---|---|---|
| `lattice_basis.h` | `LatticeIndex`、`LatticeBasis`（`at`、`coordinates`、`nearestCell`） | 是 |
| `lattice_accumulator.h/.cpp` | `LatticeAccumulator`：正規方程累加器，加點、拿掉點、求解、殘差平方和都是 O(1)；總和相對於參考點與參考格子（數值穩定） | 是 |
| `lattice_model.h` | `LatticeModel`（基底、rms、容差、涵蓋率、一致性、信心、建議檢查、索引範圍、內點與格子、正規方程）、`NoLatticeReason`、`LatticeFit` | 是 |
| `lattice_confidence.h` | `assessLattice`：信心的四個因子與它們的式子 | 是 |
| `lattice_estimator.h/.cpp` | `LatticeOptions`、`LatticeEstimator`（`fit`、`fitAll`）：擬合流程 | 是 |
| `lattice_predictor.h/.cpp` | `LatticePredictor`：索引範圍內的格點、缺格、影像內所有格點、由近到遠排序 | 是 |
| `non_max_suppression.h/.cpp` | `overlapArea`（旋轉矩形，凸多邊形裁切）、`overlapRatio`、`OverlapIndex`（空間雜湊）、`NonMaxSuppression` | 是 |
| `vector_clustering.h/.cpp` | 策略介面 `IVectorClustering`；`GridVoteClustering`（預設，O(m)）、`GreedyClustering`（S03 的做法，對照用） | 是 |
| `spatial_grid.h/.cpp` | `SpatialGrid`：均勻格子的空間索引，找最近的 k 個鄰居 | 是 |
| `basis_finder.h/.cpp` | 找 u 的候選與 v（兩條路） | 內部 |
| `lattice_acceptance.h/.cpp` | 接受規則：二維成立、證據、缺格、一致性 | 內部 |
| `cell_components.h/.cpp` | 格子的連通（逐列掃描標號＋並查集） | 內部 |
| `lattice_detail.h`、`flat_map.h` | 內部小工具：指派器（先算好反矩陣）、格子表、雜湊表、亂數 | 內部 |

`lattice_estimator.cpp` 432 行（超過專案「300 行要說明」的門檻）：只剩擬合流程本身（原點、逐步精修、指派、連通後的重新指派、定形式與品質），找基底、接受規則、連通都已拆出去；流程的步驟彼此依賴，再拆會把同一個流程分散到幾個檔案。

use case（`application/usecases/GenerateObjectsFromSample`）的改變：
- `SimpleGrid.*` 刪除；改用 `LatticeEstimator::fitAll`（可能多組晶格）與 `LatticePredictor::holes`。
- **新增**：種子搜尋的結果先做 NMS（主體框大小、IoU > 0.5 算同一個元件）；缺格若與已有的候選（或前一組晶格的缺格）重疊就不補；缺格由近到遠排序（離樣本），超過 `maxPredictions` 留近的；驗證找到的元件加進晶格重新擬合，推算的位置用修正後的。
- `GenerateOptions` 多了 `lattice`（`LatticeOptions`）與 `nms`（`NmsOptions`）；`GridSummary` 多了 `lattices`、`coverage`、`confidence`、`suggestCheck`、`noLattice`。
- **行為差異**：候選的順序從「依晶格索引」改成「離樣本由近到遠」（`SampleProposal` 的註解已改）；S03／S04 的 use case 與 Presenter 測試全部照舊通過，數量（Verified、Predicted）不變。

## 演算法細節（給要改它的人）

1. **找方向**（`BasisFinder::directions`）：最多抽 256 個點，每個點最近 4 個鄰居（`SpatialGrid`，期望 O(1)／點）的位移向量折到同一個半平面，用分群策略分群。票數 ≥ 最高票一半的群，由短到長；第一個是 u。
2. **找 v**（`secondDirection`）：先在方向裡找「與 u 的列距 ≥ 0.8·|u|、列距最小」的；找不到時（最近的鄰居都在同一列）沿 u 分列（垂直座標差 > 0.4·|u| 就是新的一列），相鄰兩列（與下下一列）之間「沿 u 最近的點」的位移對 u 取餘後分群；一群的票數是它**指向幾個不同的點**（一整排點都指向同一個離群點時，取餘後的向量完全相同，但只算 1 票）。
3. **原點**（RANSAC）：第一個試座標中位數附近的點，之後用固定種子的亂數挑，最多 16 次；評分只數離原點 6 格內落在格點上的點（數全部的點會偏好「剛好不受基底誤差影響」的假設，而不是好的原點）。附近每個點都在格點上就停。
4. **逐步精修**（`grow`）：範圍 6、12、24…格，每次指派 → 最小平方法，直到蓋住所有點。指派用「先算好反矩陣」的指派器（每點幾次乘加）。
5. **指派與去重**：殘差 ≤ 容差（0.25 × 最短格距）才算內點；同一格有兩個點時留殘差小的。
6. **連通**：格子之間 Chebyshev 距離 ≤ 3（中間最多空 2 格）才相連，只留最大的一塊，再在那一塊的索引範圍內重新指派。
7. **接受**：同一個 u 先試二維、再試一維；二維被任何一條規則擋下，一維仍要試（S05 用測試找到的漏洞：三排間距不同時曾因此錯過真的那一排）。最多試 3 個 u。
8. **定形式**：Lagrange–Gauss 約化（|u| ≤ |v|、|v·û| ≤ |u|/2）、u 在右半平面、cross(u, v) > 0、索引從 0 起，最後再解一次最小平方法。約化的好處：離格點不到 0.43·|u| 的點，四捨五入一定得到最近的格點，而容差是 0.25·|u|。
9. **多組晶格**（`fitAll`）：每找到一組，就拿掉它「認領」的點（索引範圍內、離格點 2 倍容差以內，含重複點），對剩下的點再找；最多 4 組，第二組以後至少 3 個內點；結果依內點數排序。

**數值**：累加器的總和相對於參考點與參考格子（影像座標上萬、索引上百時，平方的總和不會吃掉有效位數；`tst_lattice_parts` 用 10 萬像素的座標驗證）。索引的總和是整數，「有沒有變化」用 ≥ 0.5 判斷，不必猜容差。

## 已知會失敗的情況與對策

| # | 情況 | 會怎樣（沒有對策時） | 對策 | 剩下的風險 | 測試 |
|---|---|---|---|---|---|
| 1 | 兩個方向的間距差很多（10–20 倍） | 最近的鄰居都在同一列，只看得到一排（S03 的已知限制） | 沿 u 分列、在列與列之間找 v | 這條路比較慢：1 000 點（100 × 10、間距 10 × 200）擬合的中位數 0.82–1.34 ms，一般陣列 0.56–0.85 ms（`perf.md`） | `veryDifferentPitchesAreFoundInBothDirections`、說明頁「間距差 10 倍」 |
| 2 | 散佈、不成陣列的同型零件（板上很常見） | 3–5 個點湊巧排得像晶格，補出假缺格 | 至少 3 個內點；補缺格要有證據（缺格 ＜ 內點 − 3／− 4）；信心 | 3–4 個點湊巧等距共線時會有一組「不補缺格」的一維晶格（沒有害處，標建議檢查） | `randomPointsPredictAtMostAFlaggedHole`（20 團）、`scatteredMatchesAreNotALattice` |
| 3 | 幾排平行、間距不同 | 約一半的點碰巧落在格點附近，湊成假的二維晶格、補出一大片假缺格 | 二維成立要看每一列：落在格點上的比例、相鄰間隔是不是整數格 | 間距只差約 10% 的兩排可能被投票併成一個方向，結果是沒有晶格或其中一排 | `maxLatticesLimitsTheNumberOfModels`、說明頁「三排間距不同」 |
| 4 | 另一排斜著穿過第二列 | 只有幾個點正好在列線上、其中幾個碰巧在格點上 | 每個點歸到最近的一列，整列一起判斷 | — | `aTiltedRowCrossingTheSecondRowIsNotASecondRow`（說明頁的情境抓到的） |
| 5 | 一排加一個離群點 | 長出一整排假的第二列 | 第二列至少 2 個內點；找 v 時一個群要指向 2 個以上不同的點 | — | `aRowWithOneStrayPointAboveStaysOneDimensional` |
| 6 | 兩個同樣間距的陣列，中間空一大段 | 被當成一個陣列，中間補出一片假缺格 | 連通：空隙超過 2 格就拆開 | 一個陣列裡連續缺 3 格以上，會被拆成兩組，中間不補 | `twoSeparateArraysWithTheSamePitchAreNotBridged` |
| 7 | 雜訊大（每軸均勻雜訊 > 約 0.1 格距） | 指派錯格、找到「每隔一格」的粗晶格 | 精度與一致性進信心；範圍外的失敗要安全 | 0.14 格距時 71–100% 完全正確；≥ 0.18 格距大多不完全正確，但失敗都是安全的：沒有晶格、假缺格最多 2 個，或標成建議檢查 | `tst_lattice_scenarios`（總表在 log）、`pointsBetweenTheNodesLowerTheConsistency` |
| 8 | 缺件很多（≥ 50%） | 證據不夠的缺格 | 整組二維不成立；常退回其中幾排（一維），只補那幾排裡的缺件 | 少了整個陣列的補漏 | `tst_lattice_scenarios` |
| 9 | 陣列邊上整排都沒找到 | 往外推會在板子空白處補出假的一排 | 不往索引範圍外推 | 邊上那排真的缺件、或分數低的元件找不回來 | `holesOutsideTheFoundRangeAreNotPredicted` |
| 10 | 第二排只找到很少（12 格只有 2 個） | 用 2 個點補 10 格，證據太弱 | 證據規則：退回一排 | 那一排分數低的元件不會被視窗驗證找回來（3 個就會） | `aSparseSecondRowNeedsEnoughEvidence` |
| 11 | 非矩形的陣列（L 形、環形） | 索引範圍包含空的部分，可能在空的地方補缺格 | 證據規則與信心會擋住空得多的情況 | 空的部分不大時仍可能補出假缺格：**沒有測**，S07 用真實影像確認 | — |
| 12 | 只有 2 個點、全部重疊、有 NaN | 當機或無意義的結果 | 2 點 ＝ 一組 2 點的一維晶格（不補缺格、信心低）；重疊 ＝ 沒有方向；非有限值不參與 | — | `tooFewOrDegeneratePointsGiveNoModel`、`degenerateInputsNeverCrash` |
| 13 | 同一個元件被找到兩次 | 重複的候選；晶格的同一格有兩個點 | use case 先 NMS；估計器同一格只留殘差小的 | — | `duplicateSeedMatchesAreMergedIntoOne`、`aDuplicatePointOccupiesItsCellOnce` |

退化情況的決策樹在 [`diagram.md`](diagram.md)。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | 估計（`LatticeEstimator`）、預測（`LatticePredictor`）、NMS（`NonMaxSuppression`）各一個類別；估計器內部再拆成找基底、接受規則、連通 | 每個類別有自己的測試（`tst_lattice`、`tst_lattice_parts`、`tst_nms`） |
| O | 分群方法是策略物件（`IVectorClustering`），注入估計器；換方法不改估計器 | `theInjectedClusteringStrategyIsUsed`；兩個策略跑同一組情境（`bothClusteringStrategiesFitTheSameScenarios`） |
| L | 兩個分群策略遵守同一個契約（labels 與成員數、平均一致、排序、非有限值標 −1） | `clusteringStrategiesKeepTheirContract` |
| I | 策略介面只有 1 個方法；估計器 2 個（`fit`、`fitAll`） | — |
| D | use case 只依賴 domain 的純 C++；domain 不依賴任何外層 | `node tools/check-deps.mjs` 0 違規 |

## 決策審查（engineering-workflow 第 3 節）

| 面向 | Decision | Evidence | Verification |
|---|---|---|---|
| 架構與模組介面 | domain 第一次有 .cpp：`aoi_domain` 由 INTERFACE 改 STATIC（S02 的 CMake 註解已預告）；新的公開型別都在 `domain/lattice/`；`SimpleGrid` 刪除；presentation 不動 | `src/domain/CMakeLists.txt`；`check-deps` 133 個檔案 0 違規；`git diff --stat src/presentation` 是空的 | Release／Debug 全部 CTest 通過；`AiInspTool.pro` 加入 `domain/*.cpp` 後 qmake ＋ jom 建置成功 |
| 資料契約與生命週期 | `SampleProposal` 的候選順序改成由近到遠；`GridSummary` 新增欄位；`GenerateOptions` 新增 `lattice`、`nms`；port 的契約不變 | Presenter 只用 `candidates` 與 `classify`，不依賴順序（`tst_presenter` 照舊通過） | `holesAreOrderedNearToFarAndTheCapKeepsTheNearest`；S04 的 29 個測試照舊通過 |
| 並行、順序與狀態 | 估計器建好後不可變；每次 `fit` 用自己的 `Fitter`；RANSAC 用固定種子：同樣的輸入永遠同樣的輸出，與執行緒無關 | `lattice_estimator.h` 的契約說明 | `resultsAreReproducibleAndIndependentOfInputOrder`；use case 的 `resultsAreReproducible` |
| 可靠性與錯誤處理 | 不丟例外；非有限值的點不參與；退化輸入回「沒有晶格」與原因 | `NoLatticeReason` | `degenerateInputsNeverCrash`（9 種）、`tooFewOrDegeneratePointsGiveNoModel` |
| 效能與容量 | T3 ≤ 2 ms；空間雜湊讓找鄰居與 NMS 是 O(n)；熱迴圈不呼叫 `hypot`／`lround`／`floor` | 第一版 T3 p95 3.3–3.6 ms（三次），量測後改（`perf.md`） | `perf_lattice`（Release 把 T3 當成失敗條件） |
| 測試與驗證 | 情境掃描、與 S00 示範頁交叉驗證、說明頁的資料由 C++ 算並檢查是否過期、變異測試 | `tests/domain/` | 見 `perf.md` 的變異測試表 |
| 安全與信任邊界、交付與相容性 | 不受影響：沒有新的輸入來源、沒有 I/O | — | — |

## 與計畫的差異（誠實紀錄）

- **RANSAC**：計畫寫「以 RANSAC 排除離群點」。實作的 RANSAC 只挑原點；離群點是由容差、逐步精修、連通與接受規則排除的。理由：基底由群聚就能得到，原點才是需要假設與驗證的部分；只靠 RANSAC 擋不住「湊巧落在格點上」的點（見已知會失敗的情況 2–5）。
- **多組晶格**：計畫寫「依基底向量群聚」。實作是依序擬合：每找到一組就拿掉它認領的點，對剩下的點再找。理由：同樣間距、不同位置的兩個陣列有相同的基底向量，群聚分不開。
- **漸進驗證**：domain 提供由近到遠的順序與 O(1) 的重新擬合（`LatticeAccumulator`，`perf.md` 量到每次更新約 0.04 µs），但 use case 仍是一次批次驗證、驗證後再修正推算位置。多回合驗證要多次呼叫搜尋器，值不值得等 S07 量到真實 back end 的成本再決定。
- **信心 → UI 的「建議檢查」**：信心與 `suggestCheck` 在 `GridSummary` 裡，**畫面還沒有顯示**（這一步不改 presentation）；畫面已有的「信心偏低」提示是逐框的（推算的框與分數接近門檻的框）。
- **計畫沒有、這一步加的**：NMS 用在 use case 的種子；缺格與已有候選重疊就不補；接受規則與 `NoLatticeReason`；說明頁的資料由 CTest 檢查是否過期。
- **T3 第一次量測沒有達標**（p95 3.3–3.6 ms），量出分佈後改寫熱迴圈與連通性，才達標；過程在 `perf.md`。
- **改了 S04 的一個測試檔**（`tests/ui/tst_presenter.cpp`，只改測試，產品程式沒動）：Debug 的完整測試跑出一次失敗，`drawingASampleSearchesOffTheUiThreadAndShowsTheProposal` 在進入預覽的瞬間讀狀態列，讀到的是「搜尋中…」。原因：搜尋超過測試台的忙碌門檻（40 ms）時，忙碌標示依設計至少停留 120 ms 才換成摘要；S04 的斷言假設搜尋一定少於 40 ms。S05 的 use case 多了 NMS 與晶格估計，Debug 下從放開滑鼠到預覽量到 49–72 ms（含輪詢誤差，是上限；Release 26–44 ms），已經在門檻附近。證據：把搜尋固定延遲 60 ms 時，原本的斷言 3 次都失敗；改成等最終狀態（`QTRY_VERIFY2`）後，Release 與 Debug 各 3 次都通過。同樣的假設也在 `switchingLanguageRetranslatesTheCardTheHintAndTheSummary`，一起改了。沒有量 S04 版程式在 Debug 的時間，所以 S05 加了多少時間是推測。

## 如何驗證

- [x] 建置通過（Release、Debug，`/W4 /WX`）
- [x] 測試通過（Release、Debug 各 36 個 CTest 項目）：`tst_lattice`（由 `tst_grid` 搬來並擴充）、`tst_lattice_scenarios`、`tst_lattice_parts`、`tst_lattice_rules`、`tst_nms`、`tst_lattice_crosscheck`、`tst_lattice_explainer`、`s00_demo_fixture_is_current`，以及 S04 既有的其他 28 個
- [x] `node tools/check-deps.mjs` 0 違規；`node tools/ste-lint.mjs --strict` 0 錯誤
- [x] `perf.md` 的 T3 已量測並與 `perf/budgets.json` 比對
