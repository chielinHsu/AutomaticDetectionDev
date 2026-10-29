# R09 — 圖解

## 修改前：建完樣本之後訊息列只有提示

```mermaid
flowchart LR
  S["S 工具放開<br/>AutoRoiPresenter::onRegionDrawn"] --> R["InspectionSession::endRegion<br/>→ SampleAdded（Idle）"]
  R --> P["publishAll → publishChrome<br/>→ publishMessage"]
  P --> H["Idle：hideCard<br/>提示列：ui.sample.hintIdle<br/>「F5 搜尋 · S 再建一個 …」"]
  H -. 使用者不知道 HALCON 整框會把鋸齒當端子 .-> F5["F5 搜尋"]
```

這張圖要讓你看到：`publishMessage` 在 Idle 一律收起卡片，沒有任何地方知道「這個 back end 用整框比較差」。

## 修改後：Idle 時多一個規則，back end 的特性跟著 engine 走

```mermaid
flowchart LR
  subgraph infra["infrastructure/backend_halcon"]
    F["makeHalconBackend<br/>backend.traits.suggestsMatchMask = true"]
  end
  subgraph app["application"]
    B["Backend::traits<br/>（SearchEngine 帶著）"]
    M["MatchAdvice::pending(session, traits)<br/>旗標 ∧ Idle ∧ 類型的 match 是預設 ∧ 沒按過〔關閉〕"]
  end
  subgraph ui["presentation"]
    BP["BackendPresenter<br/>useSearchEngine(useCase, labels, traits)"]
    AP["AutoRoiPresenter::publishMessage<br/>Idle：advised_ = pending(...)"]
    C["matchAdviceCard<br/>〔用 M 點端子〕〔關閉〕"]
    MB["MessageBar 卡片<br/>（進度列 > 卡片 > 提醒卡片 > 提示）"]
  end
  F --> B --> BP --> AP
  AP --> M --> AP
  AP -->|非空| C --> MB
  AP -->|空| HIDE["hideCard、提示回來"]
  C -->|PickMatchFeature| T["select(樣本) + toolRequested(Match)"]
  C -->|CloseAdvice / Esc| D["MatchAdvice::dismiss(類型)"]
  T --> MT["M 工具點端子 → setMatchSettings<br/>→ publishAll → pending 為空 → 卡片收起"]
```

這張圖要讓你看到：和修改前不同的節點是 `makeHalconBackend`（填旗標）、`Backend::traits`、`useSearchEngine` 的第三個參數、`publishMessage` 的 Idle 分支（新增 `MatchAdvice`），以及卡片的兩個新動作。搜尋的路徑（F5 → Searching → Preview）一個節點都沒改。

## 時序：建完樣本到設好遮罩

```mermaid
sequenceDiagram
  participant U as 使用者
  participant P as AutoRoiPresenter
  participant A as MatchAdvice（application）
  participant V as MessageBar
  U->>P: S 工具放開（樣本）
  P->>A: pending(session, traits{suggestsMatchMask})
  A-->>P: [樣本 t1]
  P->>V: showCard（HALCON：建議用遮罩當比對區）
  U->>P: 〔用 M 點端子〕
  P->>P: select(t1 的樣本)、toolRequested(Match)
  U->>P: M 工具點一下端子
  P->>P: pickMatchFeature → publishAll
  P->>A: pending(...)
  A-->>P: []（t1 已有比對區）
  P->>V: hideCard，提示回來
```
