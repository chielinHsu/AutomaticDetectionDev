# S09 — 契約測試與 A/B 比較：圖解

> 圖的原始碼是 Mermaid（GitHub、Qt Creator 的 Markdown 預覽、VS Code 都能畫）。文字說明在 [`spec.md`](spec.md)，數字在 [`perf.md`](perf.md)，接受的差異在 [ADR-004](../../adr/004-backend-divergence.md)。

## 1. 契約測試的參數化（Liskov）

同一組測試函式，資料列是「組裝根註冊的每一個 back end」。沒編進程式或沒有授權的那一列 **SKIPPED 並寫出原因**，不是 PASSED。容差只在 `ContractTolerances.h`。

```mermaid
flowchart LR
    REG["app::makeBackendRegistry()<br/>fake · opencv · halcon<br/>（產品用的同一份）"] --> LIST["registry().list()<br/>註冊順序、含不可用的"]
    LIST --> ROWS["addBackendRows(Rows::All｜Vision)<br/>一個 back end 一列（列名 ＝ id）"]
    ROWS --> FN["測試函式<br/>CONTRACT_BACKEND(be)"]
    FN --> CACHE{"engineFor(id)<br/>每個 back end 只建一次"}
    CACHE -- "登記成不可用<br/>（AOI_WITH_*=OFF）" --> SKIP1["QSKIP：not usable in this build: …"]
    CACHE -- "makeSearchEngine 回 BackendUnavailable<br/>（HALCON 沒有授權）" --> SKIP2["QSKIP：not usable here: error 2300 …"]
    CACHE -- "其他錯誤" --> FAIL["QFAIL"]
    CACHE -- "建好了" --> RUN["SearchEngine：templateBuilder、searcher、generate"]
    RUN --> TOL["ContractTolerances.h<br/>profileOf(id)：readsPixels、hasLibraryMatcher、容差"]
    TOL --> CHECK["斷言：契約（全部）＋準確度（會讀像素的）"]
```

三個測試執行檔共用這個結構（`tests/contract/`）：

| 執行檔 | port | 資料列 |
|---|---|---|
| `tst_contract_searcher` | `ITemplateBuilder`、`IPatternSearcher` | 契約：fake、opencv、halcon；慣例與準確度：opencv、halcon（× 角度、× 範圍） |
| `tst_contract_end_to_end` | use case ＋ 兩個 port | opencv、halcon × 8 種合成板子；有效位元數的覆寫 |
| `tst_contract_library_matcher` | `ILibraryMatcher` | opencv、halcon（× 樣本庫大小 N）；兩邊一致（一個函式，兩個都能用才跑） |

## 2. 修改前後的測試結構

修改前：三份契約的草稿各寫一遍，兩份端到端幾乎一樣（`diff` 只差 back end 的名字與 HALCON 的角度上限）。修改後：共同的部分只有一份；各 back end 的測試只留它自己才有的事。

```mermaid
flowchart TB
    subgraph Before["修改前（S03、S07、S08）"]
        F1["tests/core/tst_fake_backend<br/>契約（fake）＋ 假世界"]
        O1["tests/backend/tst_opencv_backend<br/>契約草稿 ＋ 金字塔、NCC"]
        H1["tests/backend/tst_halcon_backend<br/>契約草稿 ＋ 授權、位姿公式"]
        O2["tst_opencv_end_to_end"]
        H2["tst_halcon_end_to_end（同一份，改名字）"]
        O3["tst_opencv_library_matcher"]
        H3["tst_halcon_library_matcher（同一份＋一次 vs 每類型）"]
    end
    subgraph After["修改後（S09）"]
        C1["tests/contract/tst_contract_searcher<br/>每個 back end 一列"]
        C2["tests/contract/tst_contract_end_to_end"]
        C3["tests/contract/tst_contract_library_matcher<br/>＋ 兩邊一致"]
        F2["tst_fake_backend：只留假世界"]:::changed
        O4["tst_opencv_backend：只留金字塔、NCC、複製、位元數、工廠"]:::changed
        H4["tst_halcon_backend：只留授權、位姿、中斷、nativeGray16、工廠"]:::changed
        H5["tst_halcon_library_matcher：只留一次 vs 每類型"]:::changed
    end
    O2 -. 併入 .-> C2
    H2 -. 併入 .-> C2
    O3 -. 併入 .-> C3
    classDef changed fill:#fff3cd,stroke:#b45309;
```

## 3. A/B 比較的資料流

比較是另一個視窗：不改板子、不進復原堆疊、不經過主視窗的互動狀態機。兩個 back end **依序**跑（每一邊都用得到所有執行緒，耗時才能比），在比較自己的 `JobRunner` 上（和主視窗共用執行緒池，但不會互相作廢）。

```mermaid
sequenceDiagram
    actor U as 使用者
    participant MW as MainWindow（ICompareEntryView）
    participant AR as AutoRoiPresenter（ICompareSource）
    participant CP as ComparePresenter
    participant JR as JobRunner（比較專用）
    participant W as worker 執行緒
    participant CW as CompareWindow（ICompareView）

    AR-->>CP: stateChanged → refreshEntry()
    CP->>AR: compareRequest()（影像、樣本框、搜尋參數、門檻）
    CP->>MW: setCompareAvailable(有樣本 且 兩個能用的 back end)
    U->>MW: 〔A/B 比較…〕
    MW->>CP: openCompare()
    CP->>CP: defaultPair()：A ＝ 目前的、B ＝ 最完整的另一個
    CP->>CW: setChoices、showRequest（顯示視窗、畫影像）
    CP->>JR: submit(work, done)
    JR->>W: work(cancel)
    W->>W: 沒建過的 back end：create(id)（HALCON 第一次約 2.8 秒）
    W->>W: runSideBySide：A.generate → B.generate（同一個請求）
    W-->>CP: UiPoster：進度（A 在 [0, 0.5)、B 在 [0.5, 1]）
    W-->>CP: UiPoster：onFinished(世代, 兩份提案)
    CP->>CP: compareProposals(A, 門檻 A, B, 門檻 B)（application，純函式）
    CP->>CW: showResult（標題、圖例、摘要表、差異清單、疊圖）
    CW->>CW: CompareOverlayItem：● 兩邊都有、■ 只有 A、▲ 只有 B
    U->>CW: 改 B 的門檻
    CW->>CP: setThresholds(a, b)
    CP->>CP: compareProposals（不重新搜尋）
    CP->>CW: showResult
```

## 4. 配對與分類（`application/compare/BackendComparison`）

```mermaid
flowchart TB
    A["A 的所有候選<br/>Sample、Seed、Verified、Predicted"] --> P
    B["B 的所有候選"] --> P
    P["容差內、由近到遠一對一配對<br/>容差 ＝ max(3 px, 元件短邊 × 0.25)<br/>（均勻格子，3×3 鄰格）"] --> S{"兩邊各自套自己的門檻<br/>找到 ＝ Sample 或 分數 ≥ 門檻"}
    S -- "兩邊都找到" --> BOTH["Both ●"]
    S -- "只有 A 找到" --> GA{"B 在這裡是？"}
    S -- "只有 B 找到" --> GB{"A 在這裡是？"}
    S -- "都沒找到" --> DROP["不列"]
    GA -- "候選、分數低於門檻" --> BT["OnlyA ■：BelowThreshold<br/>（可能只是門檻：兩邊的分數不是同一把尺）"]
    GA -- "只有晶格推算" --> IO["OnlyA ■：InferredOnly"]
    GA -- "沒有候選" --> NF["OnlyA ■：NotFound<br/>（搜尋本身沒有找到）"]
    GB --> BT2["OnlyB ▲：同上三種原因"]
```

## 5. 類別（presentation/compare）

```mermaid
classDiagram
    class ICompareSource {
        <<interface>>
        +compareRequest() optional~CompareRequest~
    }
    class ICompareEntryView {
        <<interface>>
        +setCompareAvailable(enabled, tip)
    }
    class ICompareView {
        <<interface>>
        +setChoices(choices, a, b)
        +showRequest(image, thresholdA, thresholdB)
        +showResult(CompareResultView)
        +showBusy(busy, text)
    }
    class ICompareController {
        <<interface>>
        +openCompare()
        +runCompare(idA, idB)
        +setThresholds(a, b)
        +cancelCompare()
    }
    class ComparePresenter
    class CompareWindow
    class CompareOverlayItem
    class AutoRoiPresenter
    class MainWindow
    ComparePresenter ..|> ICompareController
    CompareWindow ..|> ICompareView
    AutoRoiPresenter ..|> ICompareSource
    MainWindow ..|> ICompareEntryView
    ComparePresenter --> ICompareView
    ComparePresenter --> ICompareSource
    ComparePresenter --> ICompareEntryView
    ComparePresenter --> BackendComparison : compareProposals、runSideBySide
    CompareWindow *-- CompareOverlayItem
    MainWindow *-- CompareWindow
```
