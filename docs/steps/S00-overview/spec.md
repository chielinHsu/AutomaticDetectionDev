# S00 — 總覽：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要在每段之後。
> 檢查：`node tools/ste-lint.mjs`（只檢查句長與被動語態，不是完整 STE 驗證）。

<!--STE-->
## Purpose
The program helps a user teach a part to an AOI machine.
The user draws one box around one sample part.
The program finds the other parts that look the same.
Then it makes one inspection rectangle for each part.
<!--/STE-->

**繁中**：使用者只框一個樣本元件，程式找出所有相似元件，並為每一個產生檢測框。

<!--STE-->
## Reason
A board can have hundreds of pins.
It takes a long time to draw each box by hand. The boxes are not uniform.
Parts in an array have the same size, the same angle and the same pitch.
The program uses this rule to do the work.
<!--/STE-->

**繁中**：一塊板子可能有幾百支腳位，手工畫框很慢又不一致。陣列中的元件尺寸、角度、間距（pitch）都相同，程式利用這個規律代勞。

<!--STE-->
## Effect
The user draws one box. The program makes the other boxes.
The target is less than 300 ms for a 5 MP image with 200 parts.
Each box has a state: Found, Inferred or Rejected.
The user sees at once which boxes need a check.
<!--/STE-->

**繁中**：使用者畫一個框，其餘由程式產生。目標是 5 MP 影像、200 個元件在 300 ms 內完成（目前是**目標值，尚未量測**）。每個框有狀態（找到 / 推算補上 / 被拒絕），使用者一眼就知道哪些要檢查。

<!--STE-->
## Architecture
The program has five layers.
The Presentation layer shows the screen.
The Application layer holds the use cases and the ports.
The Domain layer holds the geometry rules.
The Infrastructure layer connects OpenCV, HALCON and files.
The Composition root creates all objects and connects them.
An inner layer must not include a header from an outer layer.
<!--/STE-->

**繁中**：五層。內層不可 include 外層的標頭檔。完整依賴表見 `docs/architecture.md`，圖見 [diagram.md](diagram.md)。

<!--STE-->
## Two back ends
The program has two back ends: OpenCV and HALCON.
Both back ends implement the same ports.
The user can switch between them.
A contract test checks that both back ends give the same kind of result.
<!--/STE-->

**繁中**：OpenCV 與 HALCON 實作同一組 port（介面），可切換；契約測試（S09）確保兩邊的座標、角度、分數定義一致。

<!--STE-->
## Speed
We make the program fast in four ways.
First, the program searches small windows. It does not search the full image again and again.
Second, the program passes image data by view. It does not copy the data.
Third, the search runs on a worker thread. The screen stays smooth.
Fourth, the program draws only the visible boxes, in one batch.
We prove the speed with tests. See the performance test plan.
<!--/STE-->

**繁中**：四個提速手段——只搜尋小視窗、影像零複製、搜尋放在 worker 執行緒、只批次繪製看得見的框。**這些是設計假設，要用 S10 的測試證明**；視窗搜尋的優勢取決於「元件占影像的比例」，板子越密優勢越小，所以必須量測而不是相信。

## 待決策（需要使用者或後續步驟確認）

| # | 問題 | 預設建議 | 在哪一步定案 |
|---|---|---|---|
| 1 | UI 工具集：Qt Widgets + QGraphicsView，還是 QML | Widgets + QGraphicsView（大量圖形項、縮放平移成熟、易測試） | S01（ADR-001） |
| 2 | HALCON 授權在其他電腦 / CI 上可能不可用 | 建置選項 `AOI_WITH_HALCON`；沒有授權時 UI 停用該 back end | S03、S08 |
| 3 | OpenCV 是強度相關（NCC），HALCON shape model 是邊緣導向，結果會有差異 | 契約測試設容差；A/B 比較畫面把差異可視化 | S09 |
| 4 | OpenCV 的旋轉搜尋成本高 | 預設小角度範圍，用角度掃描；範圍越大越慢 | S07 |
| 5 | 沒有真實板子影像可驗收 | 先用合成影像；之後請使用者提供 1–2 張去識別化的實拍圖 | S07 |
| 6 | 兩個函式庫的像素座標與角度正負號慣例可能不同 | 在 domain 規定一種慣例，adapter 負責轉換，S09 用測試鎖住 | S06、S09 |
| 7 | MSVC 2022 工具鏈是否可用 | S02 第一件事驗證 | S02 |
