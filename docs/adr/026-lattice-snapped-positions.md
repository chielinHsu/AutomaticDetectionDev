# ADR-026 CAD 框吸附到陣列的格點：晶格位姿由 use case 算、用哪一個位置由提案決定

- 日期：2026-10-15
- 決定者：使用者回報問題並在三個做法之中選了建議的做法（2026-10-15 的對話）；Claude Code (Claude Fable 5.1) 整理現況、提出選項與實作
- 狀態：採用

## 背景

使用者用 ADR-025 的版本建立 PCB 料件的 CAD 框之後回報：「建立出來的 CAD 框大小有一致，但與鄰近 CAD 框並沒有一致，CAD 框的本質是料件的理論位置。」

從程式碼確認的現況：

- `GenerateObjectsFromSample` 的種子（`Seed`）與視窗驗證找到的（`Verified`）候選，位置都是 back end 各自匹配出來的次像素位置（OpenCV 的 NCC 爬坡＋拋物線、HALCON 的 `find_shape_model`），每一個帶自己的誤差；只有視窗裡沒找到的 `Predicted` 放在晶格的格點（重新擬合後的基底）。所以同一個陣列裡，推算的框排得整齊、找到的框各自偏一點。
- 晶格（`domain/lattice`）本來就算出每個內點佔哪一格（`LatticeModel::inliers`、`cells`）、內點的容差（`tolerancePx` ＝ 0.25 × 最短格距）與殘差，但 use case 只拿它來補缺格，沒有用在找到的元件的位置上。
- `mergeSampleProposals`、`ProposalSession::objects`、`InspectionDocument::accept` 都原樣沿用候選的位姿。畫面上沒有任何地方顯示晶格的 rms 或偏差。
- 真實影像（S07，24 MP 的針腳）晶格的 rms 是 4–6 px：匹配誤差加上實際的貼裝偏移。CAD 要的是格點，兩者都不要。
- 契約測試（`tst_contract_end_to_end` 的「jitter 2 px」板子：元件刻意不在格點上）與效能預算 T6b 量的是**匹配位置**和實際位置的差（≤ 0.5 px）。`Candidate::pose` 若改成格點，這些測試在不放寬容差的前提下一定失敗，而且 A/B 比較就看不出 back end 的準確度了。

## 決定（使用者 2026-10-15）

1. **兩個位姿並存**。`Candidate::pose` 仍是匹配位置（契約、A/B、效能閘門看它）；新增 `latticePose`（晶格的格點，角度 ＝ 樣本的角度）與 `latticeDeviationPx`（匹配位置離最近格點多遠）。`BoardCandidate` 跟著帶。
2. **晶格位姿由 use case 算（domain 的晶格）**。`GenerateObjectsFromSample` 在視窗驗證之後，用每組晶格**最終的基底**（驗證找到的加進去重新擬合後的）為每個成員填格點：樣本與種子裡的內點、視窗驗證找到而且在容差內的、`Predicted`（格點就是它的位置）。不是成員但落在某組晶格索引範圍內的（例如離格點 12.5 px 的種子、視窗裡離預測位置太遠的匹配），只記下離最近格點多遠（偏離陣列）；不在任何晶格範圍內的（單獨的元件、沒有晶格的板子）兩個欄位都沒有。所以「吸附」只對內點做：門檻就是晶格既有的內點容差，沒有新的數字。
3. **用哪一個位置在 application 的 `ProposalSession` 決定**（`PositionMode`）：`Lattice`（預設）＝ 有晶格位姿的用格點當 `rect.pose`，其餘用匹配位置；`Matched` ＝ 一律匹配位置。`classify`、`objects()`、`poseOf` 同一條規則；接受時文件存的就是顯示的位姿，`accept` 不必改。切換和分數門檻一樣：只重新分類、編號不變、不進堆疊、不重新搜尋；`InspectionSession::setPositionMode` 在 `Preview`／`NoMatch` 生效，其他狀態記下來給下一份提案；復原回到預覽時位置模式也回到當時的（和門檻同一個機制）；切換後舊的「接受」不能再重做（`dropRedo`，同門檻）。已經接受到板子上的元件不動。
4. **參數面板加「位置」**：〔吸附到陣列（CAD 理論位置）〕／〔用匹配位置〕（`IParamsController::setSnapToLattice` → `IParamsSink::applySnapToLattice`，立刻交出去、不防抖）。
5. **標示**：預覽卡片多一行「N 個不在陣列的格點上（單獨的元件或偏離陣列），位置是匹配到的」（`ProposalObjects::offLattice`：`Found`、`Inferred` 之中沒有晶格位姿的，不隨模式變）；選取資訊的表單多「位置」一列：陣列的格點（匹配位置偏差 d px）／匹配位置（離陣列的格點 d px；成員但現在用匹配位置）／匹配位置（離最近格點 d px，偏離陣列）／匹配位置（不在任何陣列）／手動移過的位置；板子上的元件顯示「—」（`ProposalSession::latticeOf`、`SelectionInfo` 的 `inLattice`、`moved`、`onLattice`、`latticeDeviationPx`）。
6. **樣本不自動移動**：樣本是使用者畫的 Manual 元件，`accept` 不碰；移動樣本會改下一次的範本，形成回饋。晶格的原點是最小平方法擬合的，樣本的殘差通常只有次像素。要對齊可以用表單的中心 X、Y。
7. **跨樣本不對齊**（第一版的已知限制）：每個樣本自己的晶格；同一個陣列被兩個樣本找到時，留下的候選各帶自己晶格的格點，相鄰可能差到次像素到 1 px。實際使用時不同樣本多半是不同零件；要做的話是合併後對同類型再擬合一次，另開一步。

## 證據

- 事實（`GenerateObjectsFromSample.cpp` 第 3、5 階段）：`Seed` 是 `m.pose`、`Verified` 是 `best[k]->pose` 原樣放進候選；只有 `Predicted` 用 `basis->at(cell)`。
- 事實（`ProposalSession::objects`、`InspectionDocument::accept`）：位姿原樣沿用；`grep` 整個 `presentation/` 沒有用到 `GridSummary` 的 rms 或信心。
- 事實（`tests/contract/tst_contract_end_to_end.cpp`、`ContractTolerances.h`）：「jitter 2 px」板子的 `positionMaxPx` 是 0.5 px，對照的是抖動後的實際位置。
- 量測（`docs/steps/S07-opencv/perf.md` §4）：真實影像 HA 一維 rms 5.86 px、LA 二維 rms 4.18 px。
- 測試（這次加的）：`tst_generate_objects` 四項（成員的格點、角度與偏差；離格點 12.5 px 的種子保留匹配位置、只有偏差；沒有晶格時兩個欄位都沒有；一排吸到同一條線上；`pose` 原樣）、`tst_proposal_session` 兩項（兩種模式、編號不變、不是修改；`poseOf` 與移動、`latticeOf`）、`tst_session` 兩項（預覽中切換、接受存格點、復原回到當時的模式、Idle 裡記下來給下一份、切換後舊的接受不能重做）、`tst_presenter` 一項（切換不重新搜尋、選取與表單跟著更新、接受後板子上的元件顯示「—」）、`tst_params` 兩項、`tst_messages` 兩項。契約測試與 perf 不用改。

## 否決的替代方案

- **(a) 晶格被接受時把全部成員的 `Candidate::pose` 改成格點**：契約測試（jitter 2 px）與 T6b 會失敗，除非放寬容差（交接單禁止）；A/B 比較與 perf_matrix 就看不出 back end 的準確度。改成「兩個位姿並存、讀取端選」之後，(a) 的效果一樣有（預設就是吸附），但量測的管道不受影響。
- **(b) 偏差小於某個新容差才吸附**：估計器本來就只把殘差 ≤ 內點容差的點算成內點，「不是內點」就是偏差大；用既有的容差，不加新的數字。非內點保留匹配位置並標示——就是這次的做法。
- **在接受時（文件）才算格點**：文件沒有晶格（晶格是 use case 搜尋時的產物），要在文件重算一次晶格等於把 use case 的流程搬一份；而且預覽就要看到對齊的結果。
- **吸附連樣本一起移**：回饋（下一次搜尋的範本從新位置裁）、而且 `accept` 的規則是不碰 Manual 的元件（ADR-025 §3）。
- **把位置模式做成文件的設定並存進配方**：目前連配方的 UI 都沒有（待決定（4））；先做成 session 內的設定，預設吸附。
- **用新的 `ObjectState` 標「偏離陣列」**：要改 `tokens.json`、樣式與物件樹；先用卡片的計數與表單的文字。

## 後果

- `Candidate`、`BoardCandidate`、`InspectionObject` 多欄位；`PositionMode` 在 `domain/match.h`；`classify` 多一個參數；`ProposalSession` 多 `positionMode`、`latticeOf`（`LatticeRelation`）；`ProposalObjects`、`ProposalView`、`SceneCounts` 多 `offLattice`；`InspectionSession` 多 `setPositionMode`、`SelectionInfo` 多四個欄位。
- `IParamsController`、`IParamsSink` 各多一個方法（`IParamsSink` 變成 6 個：超過 ux-spec §12 的「≤ 5」一條；它是 Presenter 之間的窄介面，不是 View 的，先接受）。`ParamsState`、`SelectionInfoView` 多欄位；`ParamsPanel.ui` 多一列、`ObjectsPanel.ui` 多一列；`copy.json` 新增 13 個 key。
- 同一張影像再搜尋一次：格點由最小平方法決定、結果是決定性的，位置只動次像素，比匹配位置各自跳穩。
- 使用者在預覽中移過的物件不再算在格點上（表單顯示「手動移過的位置」）。
- 效能：每個候選多兩個 `optional`、第 6 階段 O(n) 的指派；沒有量（基準線的重建等使用者指示）。
- 沒有做：樣本自己的偏差沒有顯示（樣本不在合併後的提案裡）；跨樣本的對齊；位置模式不存檔；畫布上沒有用顏色標「偏離陣列」。
