# S05 — 圖解

五張圖：擬合流程、資料結構、退化情況決策樹、use case 修改前後、二維成立的判斷。文字說明在 [`spec.md`](spec.md)。

## 1. 擬合流程（`LatticeEstimator::fitAll`）

```mermaid
flowchart TD
  IN["輸入的點<br/>（種子搜尋找到的中心）"] --> FIN["丟掉非有限值的點"]
  FIN --> RUN["Fitter：找一組晶格"]
  subgraph ONE["一組晶格（lattice_estimator.cpp）"]
    DIR["找方向 BasisFinder::directions<br/>每點最近 4 個鄰居的位移 → 分群（策略）<br/>票數 ≥ 最高票一半，由短到長"]
    DIR --> U["u ＝ 第 t 個方向（t ＝ 1..3）"]
    U --> V{"v？<br/>先在方向裡找；找不到就沿 u 分列，<br/>看相鄰兩列之間的位移"}
    V -- "有" --> H2["二維假設：RANSAC 原點 → 逐步精修<br/>（6、12、24…格，指派 → 最小平方法）<br/>→ 去重 → 連通 → 重新指派"]
    H2 --> OK2{"二維成立？<br/>第二列 ≥ 2 點、可疑的列不多"}
    OK2 -- "成立" --> ACC2{"接受？<br/>證據、缺格、信心"}
    ACC2 -- "是" --> MODEL["晶格：約化、定方向、<br/>索引從 0、品質與信心"]
    OK2 -- "不成立" --> H1
    ACC2 -- "否" --> H1
    V -- "沒有（只有一列）" --> H1["一維假設（同樣的流程）"]
    H1 --> ACC1{"接受？"}
    ACC1 -- "是" --> MODEL
    ACC1 -- "否" --> NEXT{"還有 u 的候選？"}
    NEXT -- "有" --> U
    NEXT -- "沒有" --> NONE["沒有晶格（原因）"]
  end
  RUN --> DIR
  MODEL --> CLAIM["拿掉這組晶格認領的點<br/>（索引範圍內、2 倍容差內）"]
  CLAIM --> MORE{"剩下 ≥ 3 點、<br/>未滿 4 組？"}
  MORE -- "是" --> RUN
  MORE -- "否" --> SORT["依內點數排序：主晶格在第一個"]
  NONE --> SORT
```

讀這張圖要看到兩件事：（1）同一個 u 先試二維、二維被任何一條規則擋下時仍要試一維（S05 用測試找到的漏洞：沒有這一步，三排間距不同時會錯過真的那一排）；（2）多組晶格是「依序擬合、拿掉認領的點」，不是把基底向量分群——同樣間距、不同位置的兩個陣列基底相同，分群分不開。

## 2. 資料結構

```mermaid
classDiagram
  class LatticeEstimator {
    +fit(points) optional~LatticeModel~
    +fitAll(points) LatticeFit
    +options() LatticeOptions
  }
  class IVectorClustering {
    <<interface>>
    +cluster(vectors, scale) VectorClusters
  }
  class GridVoteClustering
  class GreedyClustering
  class LatticeFit {
    +models vector~LatticeModel~
    +reason NoLatticeReason
    +rejectedConfidence double
  }
  class LatticeModel {
    +basis LatticeBasis
    +rmsPx, tolerancePx
    +coverage, consistency, confidence
    +suggestCheck bool
    +minI..maxI, minJ..maxJ
    +inliers, cells
    +equations LatticeAccumulator
  }
  class LatticeBasis {
    +origin, u, v
    +hasV bool
    +at(i, j) Point2D
    +nearestCell(p) LatticeIndex
  }
  class LatticeAccumulator {
    +add(cell, p) O1
    +remove(cell, p) O1
    +solve() optional~LatticeBasis~
    +residualSumOfSquares(basis)
  }
  class LatticePredictor {
    +holes(max)
    +cellsInRange(margin, max)
    +allInBounds(max)
    +orderNearToFar(cells, anchor)$
  }
  class NonMaxSuppression {
    +keep(boxes) indices
  }
  class OverlapIndex {
    +add(box)
    +overlapsAny(box) bool
  }
  class GenerateObjectsFromSample {
    +run(request, cancel, progress)
  }
  LatticeEstimator --> IVectorClustering : 注入的策略
  IVectorClustering <|.. GridVoteClustering
  IVectorClustering <|.. GreedyClustering
  LatticeEstimator ..> LatticeFit : 回傳
  LatticeFit o-- LatticeModel
  LatticeModel *-- LatticeBasis
  LatticeModel *-- LatticeAccumulator
  LatticePredictor --> LatticeModel : 讀
  NonMaxSuppression ..> OverlapIndex : 用
  GenerateObjectsFromSample ..> LatticeEstimator
  GenerateObjectsFromSample ..> LatticePredictor
  GenerateObjectsFromSample ..> NonMaxSuppression
  GenerateObjectsFromSample ..> LatticeAccumulator : 驗證後 O(1) 重新擬合
```

模型帶著自己的正規方程（`equations`）：use case 在視窗驗證之後把找到的元件加進去重新擬合，每個 O(1)，不必回頭看所有的點。內部的 `BasisFinder`、接受規則（`lattice_acceptance`）、連通（`cell_components`）沒有畫：它們只在估計器裡用。

## 3. 退化情況與「沒有晶格」的決策樹

```mermaid
flowchart TD
  P["點"] --> F{"有限值的點 ≥ 2？"}
  F -- "否" --> R1["TooFewPoints"]
  F -- "是" --> E{"點的範圍 > 0？"}
  E -- "否（全部重疊）" --> R2["NoDirection"]
  E -- "是" --> D{"有不為 0 的鄰居向量？"}
  D -- "否" --> R2
  D -- "是" --> C{"至少 3 個內點，<br/>或每個點都被解釋？"}
  C -- "否" --> R3["NoConsistentLattice"]
  C -- "是" --> H{"缺格 ＜ 內點 − 3（一維）<br/>／ − 4（二維）？"}
  H -- "否" --> R4["TooManyHoles"]
  H -- "是" --> Q{"信心 ≥ 0.15？"}
  Q -- "否" --> R5["LowConfidence"]
  Q -- "是" --> S{"信心 ≥ 0.6？"}
  S -- "是" --> OK["晶格"]
  S -- "否" --> CHK["晶格，建議檢查"]
```

例：1 個點 → TooFewPoints；3 個重疊的點 → NoDirection；2 個點 → 一組 2 點的一維晶格（每個點都被解釋，不補缺格，信心 0.28 → 建議檢查）；20–100 個隨機點 → 大多沒有晶格，偶爾有 3–4 點「不補缺格」的一維晶格（建議檢查）。use case 拿到「沒有晶格」時只回傳找到的（合併重複之後），不做視窗驗證。

## 4. use case 的流程：修改前與修改後

修改前（S03／S04）：

```mermaid
flowchart LR
  S1["種子搜尋"] --> C1["候選：樣本＋種子<br/>（重複的匹配沒有合併）"]
  C1 --> F1["fitGrid（SimpleGrid）<br/>只看最近 6 個鄰居、O(n²)"]
  F1 --> G1["gridHoles<br/>一組晶格、索引順序"]
  G1 --> W1["視窗驗證（一次批次）"]
  W1 --> O1["Verified／Predicted<br/>（Predicted 用只有種子的擬合）"]
```

修改後（S05；★ 與琥珀色是新的或改過的節點）：

```mermaid
flowchart LR
  S2["種子搜尋"] --> N2["★ NMS<br/>同一個元件只留分數最高的"]
  N2 --> C2["候選：樣本＋種子"]
  C2 --> F2["★ LatticeEstimator::fitAll<br/>可能多組；沒有晶格就不補"]
  F2 --> G2["★ LatticePredictor::holes<br/>＋ 與已有候選重疊的不補"]
  G2 --> O2["★ 由近到遠、上限內留近的"]
  O2 --> W2["視窗驗證（一次批次）"]
  W2 --> R2["★ O(1) 重新擬合<br/>驗證找到的加進正規方程"]
  R2 --> P2["★ Verified／Predicted<br/>Predicted 在修正後的位置"]
  classDef changed fill:#fff3d6,stroke:#b45309,stroke-width:2px,color:#18212b
  class N2,F2,G2,O2,R2,P2 changed
```

presentation 沒有改：Presenter 只透過 use case 的 `candidates` 與 `classify` 用到這些，順序改成由近到遠也不影響它（S04 的 `tst_presenter` 照舊通過）。

## 5. 二維成立嗎？逐列判斷

```mermaid
flowchart TD
  A["二維的候選晶格"] --> B{"全部在同一個 i？"}
  B -- "是" --> NO["不成立 → 試一維"]
  B -- "否" --> C{"主列以外 ≥ 2 個內點？"}
  C -- "否（一排＋一個離群點）" --> NO
  C -- "是" --> D["每個點歸到最近的一列<br/>（±半個列距）"]
  D --> E["每一列（≥ 3 個點）：<br/>落在格點上的比例、列線上相鄰兩點的間隔"]
  E --> F{"比例 < 0.6，<br/>或間隔大多不是整數格？"}
  F -- "是" --> SUS["可疑的列"]
  F -- "否" --> GOOD["正常的列"]
  SUS --> G{"主列以外的內點，<br/>一半以上在可疑的列？"}
  GOOD --> G
  G -- "是（間距或方向不同的幾排）" --> NO
  G -- "否" --> YES["成立"]
```

為什麼要看「間隔」：一排間距 34 的元件配上 20 的格子，個別的點有 6 成碰巧落在格點容差內（比例會過 0.6），但相鄰兩點的間隔永遠是 34 ＝ 1.7 格，不是整數格。為什麼要「歸到最近的一列」：另一排斜著穿過時，只有幾個點正好在列線上，其中幾個又碰巧在格點上；整列一起看，比例才會反映實情（說明頁的「兩排方向不同」情境在 S05 抓到這個漏洞）。
