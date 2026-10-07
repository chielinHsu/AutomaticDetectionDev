# ADR-010 座標與角度慣例（影像像素、y 向下、角度為度、螢幕上逆時針為正）

- 日期：2026-10-05
- 決定者：Claude Code (Claude Sonnet 5.5) 提出並實作（S03）。ADR-006 把「角度的正負號」列為未決 #5 並建議「S03 在座標慣例中固定一種」，這份 ADR 就是那個固定。使用者沒有針對這份 ADR 另外表示意見。
- 狀態：採用（OpenCV 端 S07 實測；**HALCON 端 S08 實測並以測試鎖住**，見文末「後續（S08）」；S09 的契約測試對兩個 back end 跑同一組）

## 背景

總體計畫風險 6：「兩函式庫的座標與角度慣例不同。對策：domain 規定唯一慣例，adapter 轉換，測試鎖住。」S03 要定義這個唯一慣例，否則 S05（晶格）、S07（OpenCV）、S08（HALCON）、S12（CAD 匯出）各自猜。

已知的差異（**來自作者對各函式庫文件的了解；這個 session 沒有查閱文件原文，也沒有用程式驗證**，所以表中任何一格都可能錯，S07、S08 要實測）：

| 項目 | Qt（S02 畫布） | OpenCV | HALCON |
|---|---|---|---|
| 像素座標 | 場景座標：像素 (i, j) 佔據 [i, i+1)，中心在 (i+0.5, j+0.5) | 像素中心在整數座標 | `(Row, Column)`，y 在前；像素中心在整數座標 |
| y 軸 | 向下 | 向下 | Row 向下 |
| 角度單位 | — | 度（例如 `getRotationMatrix2D`） | 弧度 |
| 角度正方向 | — | 影像上逆時針為正（`getRotationMatrix2D` 文件：原點在左上角時，正值＝逆時針） | 數學正向（逆時針），並以自己的軸順序回報 |

## 決定

寫在 `src/domain/geometry.h`（一處），其他文件引用它：

1. **原點**在影像「左上角像素的左上角」，像素 (i, j) 佔據 [i, i+1) × [j, j+1)，中心在 (i + 0.5, j + 0.5)。和 S02 的畫布一致，所以畫布不必換算。
2. **x 向右、y 向下。**
3. **角度單位是度。** UI、CAD 清單（S12）與搜尋參數都用度，domain 內不必再換算。
4. **正方向是「在螢幕上看起來逆時針」**，正規化到 (−180, 180]。因為 y 向下，把向量 (x, y) 轉 θ 度：x' = x·cosθ + y·sinθ，y' = −x·sinθ + y·cosθ。例：(1, 0) 轉 +90° 到 (0, −1)（畫面正上方）。
5. **Rect 用左上角＋寬高；Pose2D 與 RectObj 用中心。**
6. 樣本庫存 µm，影像帶校正值 `ImageView::umPerPixel`（0 ＝ 未校正；ADR-006 §6）。`ComponentType` 與 `BoxGroup` 的長度欄位名稱一律以 `Um` 結尾；`ComponentInstance` 與 use case 的結果是影像像素。

選逆時針為正的理由：（a）CAD 與一般數學習慣一致，S12 匯出時不必反轉；（b）OpenCV 的 `getRotationMatrix2D` 在影像座標下就是這個方向，OpenCV adapter 不必改號；（c）使用者在畫面上看到的旋轉方向與數字符號一致。

## 證據

事實（已觀察）：

- `tests/core/tst_domain.cpp` 用數字鎖住這個慣例：`(1, 0)` 轉 `+90°` 得 `(0, −1)`、`(0, 1)` 轉 `+90°` 得 `(1, 0)`、轉 `θ` 再轉 `−θ` 回到原點、長度不變；角度正規化的邊界值（±180、540、−725…）。
- `FakeGrid` 以同一個 `rotate()` 排出旋轉的網格，`tst_generate_objects` 的旋轉網格測試（3°）通過。

假設（未驗證）：

- OpenCV 與 HALCON 的行為只來自上表（作者對文件的了解），**沒有查閱原文，也沒有在這台機器上用程式驗證**。OpenCV 的 `matchTemplate` 回傳的是模板左上角的整數座標，adapter 要自己加上半個模板大小的換算，S07 驗證；HALCON 的 `find_shape_model` 的 `Angle` 正負號與 `Row`／`Column` 的順序，S08 驗證，S09 的契約測試用「已知旋轉角的合成影像」對每個 back end 跑同一組斷言，這才是這個慣例的真正鎖。

## 否決的替代方案

- **弧度**：HALCON 用弧度，但 UI、CAD 與參數都用度，內部再轉一次只會多一層換算與出錯的機會。換算放在 HALCON adapter。
- **順時針為正**：和 CAD、OpenCV 的 `getRotationMatrix2D` 相反，兩邊都要反號。
- **像素中心在整數座標（OpenCV 的慣例）**：畫布（Qt）會多一個 0.5 的換算，而畫布是整天在用的路徑（I1 預算很緊）。

## 後果

- 每個 adapter 要自己轉換（座標偏移 0.5、HALCON 的軸順序與弧度），並用契約測試（S09）證明。
- `Match::pose.angleDeg` 的契約：搜尋器回報的角度在 `SearchParams` 的範圍內且已正規化。
- 之後若要改任何一條，要新增一份 ADR 取代這一份，並檢查 S05–S12 全部用到角度的地方。

## 後續（S08，2026-10-07）：HALCON 端實測

決定沒有改；上面「假設（未驗證）」裡 HALCON 的部分，S08 查了說明原文並用程式驗證（[ADR-017](017-halcon-shape-model-backend.md)、`src/infrastructure/backend_halcon/HalconPose.h`）：

- **軸順序與單位**：`find_shape_model` 回 `(Row, Column, Angle)`，Row 向下，角度是弧度。像素中心在整數（`affine_trans_pixel` 的說明：「標準座標系的原點在左上角像素的中心」）。
- **角度的正方向和本 ADR 相同**：`hom_mat2d_rotate` 的旋轉矩陣 `[[cos, −sin], [sin, cos]]` 作用在 (row, column) 上，展開後和 `rotate()` 一模一樣，所以 adapter 只換單位、不改正負號。實測：傾斜 ±3°、+4.5° 的合成板子，角度誤差 0.09–0.21°（正負號反了會差 6° 以上）。
- **位置不是單純的 ±0.5**：說明寫 Row、Column 是「為了配 `vector_angle_to_rigid` 與 `affine_trans_pixel` 調過的值」。參考點真正的位置是 `(Row, Column) ＋ R(Angle)·(0.5, 0.5) − (0.5, 0.5)`，換成本 ADR：`x ＝ Column ＋ 0.5(sin ＋ cos)`、`y ＝ Row ＋ 0.5(cos − sin)`；角度 0 時就是 ＋0.5。實測（±30°，|傾斜| ≥ 20°）：修正後平均誤差 0.018 像素，只加 0.5 的是 0.303 像素。
- 鎖住的測試：`tests/backend/tst_halcon_backend.cpp` 的 `poseConversionMatchesTheDocumentedFormula`、`theAngleSignFollowsTheConvention`、`everyAngleInTheRangeIsRecovered`、`theRotatedPoseCorrectionIsNeeded`。
