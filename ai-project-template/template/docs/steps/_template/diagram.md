# <ID> — 圖解

至少一張圖。選最能說明這個設計的類型：

| 想說明 | 用哪種 Mermaid 圖 |
|---|---|
| 模組怎麼切、誰依賴誰 | `flowchart` |
| 類別與介面的關係 | `classDiagram` |
| 一次操作中，誰在什麼時候呼叫誰 | `sequenceDiagram` |
| 介面或物件的狀態變化 | `stateDiagram-v2` |
| 資料與執行緒怎麼流動 | `flowchart LR` |

```mermaid
flowchart LR
  A["TODO"] --> B["TODO"]
```

每張圖下面用一兩句話寫出「這張圖要讓你看到什麼」。
