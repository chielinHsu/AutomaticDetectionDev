# ADR-028 比對區與 CAD 框分開：範本只用樣本裡想要的特徵，搜尋器回報的仍是主體中心

- 日期：2026-10-15
- 決定者：使用者確認範圍、設計方向、格式升版與驗收條件（2026-10-15 的對話）；設計由 Claude Code 的 architect subagent（Opus）提出，Claude Code (Claude Opus 5.5) 實作與量測
- 狀態：採用
- 編號：分支 `feat/match-region` 上原本是 ADR-026；同時進行的晶格位置吸附先合併到 `main` 用了 026（操作步驟用了 027），2026-10-16 把 `main` 併進分支時改為 028，內容沒有改

## 背景

使用者的密排端子影像（`D:\Benano\20261014_4677_AdvancePin_LeftTopRegion_Proj`，記憶體的 reference-advancepin-dataset）：CAD 框 148 × 148 px，端子亮塊只佔約 2%（約 30 × 19 px），上下是大塊的亮焊墊。用整個 CAD 框建範本時，比對的幾乎都是焊墊：

- 前一個 session 的實驗（暫存程式，全解析度 `matchTemplate`、門檻 0.8）：整框召回 85.7%、位置誤差 p95 7.7／最大 16.4 px，晶片旁少一塊焊墊的那欄完全找不到；只框端子（端子外框＋12 px）召回 99.9%、誤報 3。
- 從程式碼確認：範本就是整個主體框（`GenerateObjectsFromSample.cpp` 把樣本框原樣交給 `ITemplateBuilder::build(Rect)`；HALCON 的 domain 是 `GenRectangle1` 的主體矩形），而整條流程把「範本中心」當成「元件（CAD 主體）中心」（`ITemplate::size()`、NMS、晶格、`BoardProposal`）。

## 決定

### 1. port 的語意：`Match.pose` 一律是 CAD 主體中心

- `ITemplateBuilder` 保留 `build(image, Rect)`，新增有預設實作的 `buildFromSample(image, SampleSpec, params, cancel)`（`SampleSpec{Rect body; MatchSpec match}`，domain 的 `match_region.h`）。預設實作：比對規格是預設（整框、灰階）就交給 `build`，否則明確回 `InvalidArgument`（不默默忽略）。所以 5 個測試替身與約 45 個呼叫點不必改。
- 契約（`ITemplateBuilder.h`、`IPatternSearcher.h`）：範本只用比對區的像素與特徵；搜尋器回報的位姿是**主體中心**；`ITemplate::size()` 仍是主體框；預設規格和 `build(rect)` 的結果完全相同；比對區與影像沒有交集回 `InvalidArgument`。
- 結果：use case 只改三處（對比與最小邊長量比對區、比對區裁到影像外是 `SampleOffPart`、呼叫 `buildFromSample`），之後的 NMS、晶格、合併、接受、展開框都不必知道比對區。

### 2. 資料存在元件類型上

- `ComponentType::match`（`MatchSettings`：比對區 `MatchRegion{centerXUm, centerYUm, widthUm, heightUm, mask}`、特徵 `MatchMode{Intensity, Edges}`）。座標和框群組同一套：µm、相對主體中心、未旋轉；遮罩是 run-length（`MatchMask{width, height, runs}`），格子均勻蓋住比對區的矩形。
- 不做成新的框群組種類：`expandBoxes`、`countBoxes`、畫布會把它當成輸出的檢測框，違反「CAD 框維持原樣」。
- 特徵放在類型上，不放 `SearchParams`：它取決於樣本的特徵；`SearchParams` 是全域的、契約測試與配方的 `params` 都依賴它。
- `InspectionDocument::samples()` 用 `matchSpecFor` 把類型的設定換成這個樣本的影像像素（遮罩用最近鄰取樣）；`SampleSearch`、`SampleRequest`、A/B 比較的 `CompareRequest` 都帶著它。
- 限制（P1）：樣本的角度一律是 0（`addSample`），換算不處理旋轉。

### 3. OpenCV

- 範本裁比對區（矩形，或遮罩的外接矩形）；範本記住 `originOffset ＝ 主體中心 − 比對區中心`（`OpenCvMatchGeometry`），搜尋器最後一步把範本中心換成主體中心 `c ＋ rotate(o, θ)`，用主體中心過濾區域；NMS 的框是比對區大小、放在範本中心（密排的元件主體框可能互相重疊）。視窗的邊距、頂層的 `reach` 都多加 `|o|`。整框時 `o ＝ 0`，程式路徑和之前相同（契約測試 `aDefaultSpecMatchesTheRectBuild` 逐位元比對）。
- 遮罩：每層、每個角度一張遮罩（和影像同一個 `pyrDown`、旋轉後二值化）。大的分數圖用 `cv::matchTemplate(TM_CCOEFF_NORMED, mask)`，分母是 0 的位置 OpenCV 給 inf／NaN，**設成 0**（不能夾成 1，那會變成假的峰）；小的分數圖自己算 `correlateMasked`（Σ T″·I ／ √(Σ T″² · (Σ M·I² − (Σ M·I)²／n))，T″ ＝ M·(T − 遮罩內平均)，和 OpenCV 的定義相同）。遮罩蓋住外接矩形 ≥ 95% 時當成矩形；某層遮罩剩不到 32 個像素就不用那一層（第 0 層就不夠 → `SampleTooSmall`）。
- 邊緣模式：每一層各自算 Sobel 3×3 梯度大小 × `edgeScale`（0.25）存成 8 bit，之後才旋轉與裁切（避免裁切的邊界產生假的邊）。範本記住 `edgeScale`，搜尋用同一個。
- 小範本的金字塔層數變少（例如 37 × 27 只有 2 層），粗搜變慢：這次**不先優化**（S11 的規則：先有數字），數字見「效果」。

### 4. HALCON

- domain 改成比對區：矩形是 `GenRectangle1`；遮罩是每一段一個一列高的矩形（兩端都包含，和 `halconRect1` 同一個慣例），再 `Union1`。
- **不呼叫 `set_shape_model_origin`**（和 architect 的提案不同）：模型的參考點維持 HALCON 的預設——domain 的重心——由 `AreaCenter` 取得，範本記住 `originOffset ＝ 主體中心 − 參考點`（`HalconMatchGeometry`），搜尋器自己加上 `rotate(o, θ)`，和 OpenCV 同一個做法。理由：`poseFromHalcon` 的推導（ADR-010 的 S08 後續）是針對模型座標 (0, 0) 的參考點，不必驗證 origin 移動之後是否仍成立；整框時不必算重心，和之前逐位元相同。
- `find_shape_model` 的搜尋區域限制的是參考點（說明檔 `operators_en_US.ref` 的 find_shape_model 一節：「A different origin set with set_shape_model_origin is not taken into account」），所以以主體中心為準的視窗要換成參考點的視窗：半範圍 < 90° 時平移 `−rotate(o, 中間角度)`、往外擴 `|o|·2·sin(半範圍／2) ＋ 1`，否則不平移、擴 `|o|`（`referenceWindows`）。轉換影像的範圍用參考點到比對區最遠的角落。
- 邊緣模式對 HALCON 沒有作用：shape model 本來就是邊緣（ADR-004 D7）。

### 5. 自動擷取放在 domain

`feature_extract.h`：`pickBrightBlob`（主體框內 256 階 Otsu、亮度依有效位元數換到 0..255、點在前景就取它的 8 連通塊，否則取最近的前景像素所在的塊）、`growMask`（圓盤外擴、裁到主體框）。放 domain（只用標準庫）而不是 adapter 後面的 port：遮罩是存下來的教學資料，不能因為切換 back end 而不同；範圍只有主體框，O(n)（148 × 148 不到 1 ms，`tst_match_region` 印出）。

### 6. UI

- `M` 工具（`MatchRegionTool`，`DraftMode::Match`）：在所選的那一個元件上拖一個框 ＝ 矩形比對區（裁到主體框內）；點一下 ＝ 自動擷取（外擴預設 4 px）。不經過狀態機：和 `B` 一樣是 `Idle` 的一個 Command（`CommandKind::SetMatchRegion`，`InspectionDocument::setMatchSettings`，可以 `Ctrl+Z`）。
- 選取資訊表單多一組「比對區」：狀態（整個主體框／矩形 w × h／遮罩 n px）、特徵（灰階／邊緣）、外擴（0..20 px，只有自動擷取、而且還是那一次擷取的遮罩能調；連續的調整併成一步）、〔清除比對區〕。
- 畫布：`Idle` 選了一個有比對區的元件時，用草稿框畫出比對區（遮罩畫外接矩形）。P1 不新增顏色 token；遮罩的形狀與專用的畫法留到 P2。
- 規則在 `InspectionSession`（`setMatchRect`、`pickMatchFeature`、`setMatchGrow`、`setMatchMode`、`clearMatchRegion`、`SelectionInfo` 的比對區欄位），Presenter 只做吐司與重新發布（ADR-011）。

### 7. 儲存

配方升到 v3、樣本庫升到 v2：每個類型多一個 `"match": {"mode": "intensity"|"edges", "region": null | {centerXUm, centerYUm, widthUm, heightUm, "mask": null | {width, height, runs: [[row, colBegin, colEnd], …]}}}`。v1／v2 的配方、v1 的樣本庫遷移時補整框、灰階；讀的時候嚴格檢查（格子 1..4096、每段在格子內、同一列依序不重疊、不能是空的），違反就 `FormatInvalid`。新格式的檔案舊版程式讀不了（使用者 2026-10-15 同意）。

### 8. 流程 B 不改

`ILibraryMatcher` 從參考影像整張建範本，UI 也還沒接（ADR-025 §6 的 P2）。類型上的 `match` 會跟著存進樣本庫，但比對器不讀它。

## 否決的做法

- 改 `build` 的簽名：8 個實作都要改，衍生類別還會遮蔽多載。
- 比對區做成新的框群組種類：會變成輸出的檢測框。
- 比對特徵放 `SearchParams`：全域的參數不該隨樣本變。
- 自動擷取做成 adapter 的 port：切換 back end 會擷取出不同的遮罩，關掉 OpenCV 時功能消失。
- HALCON 用 `set_shape_model_origin`：見 §4。

## 效果（量測，2026-10-15）

`tests/backend/perf_match_region_real`（效能量測類；`AOI_REAL_MATCH_IMAGE` 指到影像才跑，影像不入版控）。影像 `FovImages1\0_0_HA.tif`（5314 × 4594、16 bit 容器、12 bit），樣本 ＝ 點 (2178, 2002) 的端子，CAD 框 148 × 148（端子重心往左上 (71, 57)）。整張影像搜尋、角度 ±5°、minScore 0.5，事後依門檻過濾。

- **標準答案**：整張影像 Otsu（144）的連通塊，面積與外接矩形和樣本的端子相近的 ＝ 一根端子，重心。共 **1109** 根（前一個 session 數到 1047；差別多半是影像邊上被切到一半的端子，`real-match-truth.png` 的疊圖人工看過：每一根端子都有，晶片旁那欄也有）。**使用者還沒有目視確認**。
- **配對**：預測的主體中心減掉樣本的位移（轉過找到的角度）＝ 預測的端子重心，和標準答案在 10 px 內一對一配對。

| 列（back end、比對區） | 門檻 | 召回 | 誤報 | 誤差 p95／最大（px） | 偏差（px） | 扣掉偏差 x p95／y p95 | 搜尋時間 |
|---|---|---|---|---|---|---|---|
| OpenCV 整框 | 0.80 | 93.1% | 12 | 6.54／9.52 | (−0.60, −1.27) | 3.68／5.26 | 0.46–0.67 s |
| OpenCV 矩形（端子外框＋12 px） | 0.80 | 99.8% | 26 | 4.08／5.15 | (−1.26, −0.02) | 2.82／0.27 | 0.44–0.71 s |
| OpenCV 矩形 | 0.85 | 99.4% | 0 | 4.08／5.15 | | | |
| OpenCV 遮罩（亮塊外擴 4 px） | 0.80 | 99.5% | 7 | 4.04／5.05 | (−1.23, −0.01) | 2.81／0.25 | 2.2–3.3 s |
| **OpenCV 邊緣（矩形）** | **0.60** | **100%** | **0** | 4.06／5.16 | (−1.19, −0.10) | 2.87／0.26 | 1.4–1.7 s |
| OpenCV 邊緣 | 0.70 | 99.9% | 0 | 4.06／5.16 | | | |
| HALCON 整框 | 0.75 | 93.1% | 3 | 5.95／9.60 | (−0.65, −0.86) | 3.65／4.85 | 0.34–0.52 s |
| HALCON 矩形（＝邊緣，D7） | 0.70 | 99.0% | 16 | 4.31／5.87 | (−1.19, −0.13) | 3.15／0.34 | 0.64–0.80 s |
| HALCON 矩形 | 0.75 | 93.3% | 1 | 4.30／5.22 | | | |
| HALCON 遮罩 | 0.70 | 98.9% | 21 | 4.34／5.65 | (−1.22, −0.13) | 3.15／0.34 | 0.46–0.57 s |

兩個 back end 彼此的一致性（和標準答案無關；配對的位移扣掉平均之後）：OpenCV 矩形 vs OpenCV 邊緣 p95 0.45 px（最大 1.12）；OpenCV 矩形 vs HALCON 矩形 p95 1.20 px；OpenCV 邊緣 vs HALCON 遮罩 p95 1.18 px（最大 2.55）；整框的 OpenCV vs HALCON p95 2.45 px。

解讀：

- **召回與誤報**：整框兩個 back end 都停在 93.1%（晶片旁那欄與影像邊上的找不到）。比對區把召回拉到 99–100%。使用者的通過條件（召回 ≥ 99.5%、誤報 ≤ 5）：**OpenCV 的邊緣模式達到**（門檻 0.6：100%、0；0.7：99.9%、0），OpenCV 的遮罩在 0.8 差一點（99.5%、7）；**HALCON 沒有一個門檻同時達到**（0.7：99.0%、16；0.75：93.3%、1）——HALCON 的分數集中在 0.65–0.8，真的端子與相似的邊分不太開。
- **位置**：比對區的誤差幾乎都在 x（扣掉偏差後 y 的 p95 只有 0.25–0.34 px，x 是 2.8–3.2 px），而兩個不同的演算法彼此的 p95 是 0.45–1.2 px。標準答案的重心在 x 上不穩：端子右邊有一段暗一點的延伸，過不過 Otsu 門檻每根不同（樣本那一根：擷取的亮塊 39 寬、標準答案 31 寬，重心差 3.9 px）。所以「誤差 p95 ≤ 3 px」用這份標準答案量不到（4.0–4.3 px，其中約 1.2 px 是偏差），不是搜尋不準；整框的 y 誤差 p95 4.9–5.3 px 才是真的（焊墊把位置拉走）。要嚴格驗證位置，需要更好的標準答案（例如人工標記，或用 y 與一致性當準則）——待使用者決定。
- **時間**：小範本只有 2 層金字塔，OpenCV 的矩形和整框差不多（0.4–0.7 s），邊緣慢約 2–3 倍、遮罩慢約 4–5 倍；HALCON 都在 1 s 以內。這次不優化（S11 的規則）。
