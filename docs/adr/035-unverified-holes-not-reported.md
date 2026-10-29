# ADR-035 影像上驗證不到的推算格預設不回報

- 日期：2026-10-29
- 決定者：使用者（決定不畫）／Claude Code (Claude Fable 5.1)（做法與量測）
- 狀態：採用（補充 ADR-013、ADR-026；S05 的「Inferred」輸出改成選項）

## 背景

S05 起 use case 的補漏分兩種：晶格預測的缺格在小視窗裡再搜尋一次，找到的是 `Verified`（有分數）；
什麼都沒找到的（分數 < `candidateFloor` 0.30）回報成 `Predicted`（沒有分數，畫面上的「推算」），
用意是「影像上真的缺件時 CAD 仍然完整」。

R07.2 把晶格的索引區域改成三個條件的交集之後（ADR-013 的「後續」），在使用者的兩張真實影像
（AdvancePin 的 `0_0_HA.tif`：2026-10-07 兩條橫的長槽、2026-10-14 LeftTopRegion 右下角一個長槽）
用 `tests/backend/perf_multi_sample_real` 量測，**每一個沒有影像支持的推算格都是錯的**：
長槽角落 4–13 格（HALCON）、7–11 格（OpenCV）。它們來自陣列角落「兩個方向都貼著內點」的規則
把槽內的格子當成陣列的一部分；驗證時影像裡是一片黑，就留在「推算」。換範本（整框、矩形比對區、
自動擷取的遮罩、外擴）都去不掉，因為不是搜尋器的事。使用者 2026-10-29 看了畫面決定：這些格子不畫。

另一個考量：教學用的影像本來就是好板子，真的缺件很少見，而且可以手動補；
把沒有證據的框畫出來，使用者得一個一個刪。

## 決定

- `GenerateOptions::reportUnverifiedHoles`（預設 `false`）：視窗驗證什麼都沒找到的缺格不回報成候選；
  數量記在 `GridSummary::unverified`（`holes` 照舊是預測的缺格數）。打開時行為和之前完全相同。
- 其餘不變：驗證找到的仍是 `Verified`（低分的在畫面上是「拒絕」，門檻拖到 0.30 看得到）；
  晶格的擬合、重新擬合、晶格位姿（ADR-026）、`classify` 的 `inferred` 分類都不動——只是預設下沒有 `Predicted`。
- 測試：核心（`tests/core/CoreTestSupport.h` 的 `makeRequest`）與契約（`tst_contract_end_to_end`）的請求
  打開選項，繼續驗證補漏的機制；`tst_generate_objects::unverifiedHolesAreCountedButNotReportedByDefault`
  與 `tst_presenter`（走產品的接線）驗證預設。
- 畫面不加選項：參數面板、物件面板的「推算」計數與「推算」狀態保留（打開選項時仍會用到），
  要不要在參數面板加〔也畫影像上沒有的格子〕等使用者要求再做。

## 否決的替代方案

- 把角落的條件收緊（兩側證據的距離 3 格 → 1 格、或角落不算）：只去得掉槽內部的，去不掉槽緣夾在
  鋸齒之間的；而且 1 029 種情境裡缺角的格子本來就靠這個條件補。
- 只在畫面上隱藏（`classify` 或 Presenter 不畫 `Predicted`）：接受時還是會存進文件，和「不畫」的意思不符；
  規則放在 use case 一處，A/B 比較、契約與量測工具看到的都一樣。
- 改搜尋器讓鋸齒不被找到：鋸齒的分數剛好壓在 0.70（整框）或已經被遮罩壓到 0.48，槽內的推算格與它無關。

## 後果

- 預設下「推算」的數量永遠是 0；真的缺件（影像上沒有）不會補框。要補時打開 `reportUnverifiedHoles`
  （目前只有程式介面）。
- 真實影像的 after（HALCON、整框、LeftTopRegion）：推算 13 → 0，找到與驗證的數量不變；
  OpenCV 推算 7 → 0。量測：`perf_multi_sample_real` 的 `predicted` 與 `holes (unverified N)`。
- S05 的 `spec.md`、說明頁描述的「Inferred」輸出仍然正確，只是預設關掉；本 ADR 是那份設計的補充。
