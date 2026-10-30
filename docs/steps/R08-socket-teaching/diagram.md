> 目前主要流程已改為 [區域 CAD 註冊](region-workflow.md)，見 [ADR-036](../../adr/036-socket-region-cad.md)。下列為原版歷史。

# Socket 樣本搜尋接線

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-29 | Codex (gpt-6.1-sol) | 主流程改為 R 教樣本、F5 全圖搜尋、統一尺寸與樣本配方。 |
| 2026-10-28 | Codex (gpt-6.1-sol) | 初版亮塊定位與可編輯框。 |

## 修改前

```mermaid
flowchart LR
 A[R 圈選區域] --> B[亮塊分割]
 B --> C[各亮塊外接框]
 C --> D[手動修改、存配方]
```

## 修改後

```mermaid
flowchart TD
 A[R 教代表樣本] --> B[SolderTeachingSession::addSample]
 B --> C[F5 / searchWholeImage]
 C --> D[SearchSocketSamples]
 D --> E[ITemplateBuilder + IPatternSearcher]
 E --> F[相似度篩選 + NMS]
 F --> G[replaceMatches 第一樣本統一尺寸]
 G --> H[ImageCanvas + 類別／分數清單]
 H --> I[V 編輯 / S 補框 / Delete]
 I --> J[RecipeSaver / JsonRecipeStore v3]
 J --> K[還原樣本、門檻及框]
```

```mermaid
stateDiagram-v2
 [*] --> Loaded: 載圖
 Loaded --> Samples: R 教樣本
 Samples --> Samples: R 新增／V 修改
 Samples --> Searching: F5
 Searching --> Samples: 取消／錯誤
 Searching --> Results: 有匹配
 Results --> Searching: F5 重搜
 Results --> Results: V／S／Delete／Undo
 Results --> Saved: 存配方
 Saved --> Samples: 開配方，保留樣本及結果
```
