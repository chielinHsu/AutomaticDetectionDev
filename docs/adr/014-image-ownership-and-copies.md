# ADR-014 影像的所有權、正規格式與複製計數

- 日期：2026-10-06
- 決定者：Claude Code (Claude Opus 5.5) 提出並實作（S06）。`infrastructure/image_qt` 這個新 adapter 的做法在 S06 開工時向使用者說明過（使用者沒有反對）；其餘是本步驟依量測與測試做的決定，使用者沒有另外表示意見。
- 狀態：採用

## 背景

`ImageView`（`src/domain/image_view.h`）從 S02 起就是跨過 port 的影像型別：資料指標、寬高、stride、像素格式、一個 `shared_ptr<const void>` 的擁有者，S03 加了校正值。S02 留下「所有權規則、格式轉換與複製計數在 S06 定案」。S06 的計畫要求：Qt、OpenCV、HALCON 三方零複製轉換；必須複製的格式要明確標示並計數；三種 `IImageSource`（OpenCV、HALCON、Qt 後備）對同一個檔案回傳相同語意的 view。

本機的事實（S06 實測）：

- Qt 6.8.0 的影像外掛只有 bmp、cur、gif、ico、jfif、jpeg、jpg、pbm、pgm、png、ppm、svg、svgz、xbm、xpm——**沒有 TIFF**（`plugins/imageformats` 沒有 qtiff）。S02–S05 的開檔對話框列了 `*.tif`，但根本讀不了。S06 中途改用的 Qt 6.12.0 也沒有 TIFF（`plugins/imageformats` 只有 qgif、qico、qjpeg、qsvg；BMP、PNG 是 Qt Gui 內建的，S06 的測試在 6.12.0 讀過）。
- OpenCV 4.9.0（vc16 預建套件，`opencv_world490`）可以用 VS 2022（MSVC 19.44）連結與執行：S06 第一次真的編進程式。它有 libtiff、libpng、libjpeg。
- HALCON 只有 runtime（沒有 `HalconCpp.h`、沒有 `.lib`），**不能編譯**。
- 解碼結果的排列：Qt 把彩色的 PNG、JPEG、BMP 解成 `RGB32`／`ARGB32`（記憶體順序 B、G、R、x）；OpenCV 的 `imdecode` 解成 B、G、R 三個 byte（`8UC3`）。
- `cv::imread` 在 Windows 用系統的 ANSI 字碼頁開檔，含中文的路徑讀不到。

## 決定

1. **所有權規則**（寫在 `image_view.h`，編號在程式與測試裡引用）：
   1. 像素交出去之後不再改：誰拿到 view 都可以在任何執行緒讀，不加鎖。要改就配新的緩衝（`ImageBuffer`），寫完再交出。
   2. 複製 `ImageView` 只複製指標與 owner 的參照計數。
   3. 跨執行緒、跨 port、活得比這次呼叫久的 view 一定要有 owner；owner 是空的只能在同一次呼叫裡借用。
   4. 子影像（`subView`）與原影像共用 owner；矩形要整個在影像內，否則回傳無效的 view（不裁切，避免座標悄悄偏移）。
   5. 函式庫的影像物件包住 view 的像素時（QImage 的清理函式、`cv::Mat` 的外部資料），要讓 owner 活著，而且不可以寫。
   6. 做不到零複製的轉換，配新緩衝並呼叫 `countImageCopy`。
2. **新增兩種像素格式**（加在列舉的最後，舊的三種數值不變）：`Bgr24`（OpenCV 的彩色排列，`cv::Mat 8UC3` 可以零複製包住）、`Gray16`（16 bit 灰階，TIFF 與 16 bit PNG）。
3. **影像來源只交出三種「正規格式」**：8 bit 灰階 → `Gray8`；16 bit 灰階 → `Gray16`；其他（彩色、有 alpha、索引色、16 bit 彩色）→ `Bgrx32`（16 bit 彩色四捨五入 `v / 257` 換成 8 bit）。這是 `IImageSource` 契約「同一個檔案、相同語意（像素格式）」的具體寫法。選 `Bgrx32` 當彩色的正規格式，因為 Qt 解出來就是它（零複製），也是 QPainter 畫得最快的格式（S02 的量測都是它）。
4. **「一次複製」的定義與計數**（`domain/image_copy.h`）：為了換容器、記憶體排列或像素格式，把同一張影像的像素寫到新緩衝。解碼、演算法的新內容（模糊、金字塔）、存檔的序列化不算。計數是全程式共用的 atomic（任何層都要能呼叫，所以放 domain）；測試取前後差斷言零複製的路徑是 0。
5. **adapter 的位置**：一個函式庫一個資料夾。
   - Qt：新的 adapter `infrastructure/image_qt`（Qt Core ＋ Qt Gui，不可以用 Widgets 與視覺函式庫）：`fromQImage`／`wrapAsQImage`／`toCanonicalQImage`、`QtImageSource`。`tools/check-deps.mjs` 加了 `qt-gui` 這一類（白名單）與這一層的規則，兩個新 fixture 證明它擋得住 Widgets 與 OpenCV、擋得住 storage 用 Qt Gui。
   - OpenCV：放在 `infrastructure/backend_opencv`（`fromMat`／`borrowAsMat`、`OpenCvImageSource`），`AOI_WITH_OPENCV=ON` 才建置。
   - HALCON：S08 放在 `infrastructure/backend_halcon`。
   - 畫面的 ImageView → QImage 仍在 presentation 的 `ImageItem::wrap`（presentation 不可以 include infrastructure）；兩邊的格式對照表由 `tst_image_qt` 比對一致。
   - 計畫寫的是 `src/infrastructure/{image,storage}/`；沒有照字面做一個共用的 `image` 資料夾，因為 `check-deps`（S03）規定視覺函式庫只能出現在各自的 back end 資料夾。
6. **組裝的順序：Qt 在前，OpenCV 在後**（`app/CompositionRoot::makeImageSource`，`application/image/ImageSourceChain`）。計畫寫的是「Qt fallback」（Qt 當後備）。改成 Qt 在前的理由：常見格式（PNG、JPEG、BMP）Qt 零複製，OpenCV 讀彩色檔要多複製一次（5 MP 的 BGR → BGRx 約 3 ms、多 15 MB 的暫時記憶體），解碼時間兩邊差不多（`perf.md`）；Qt 讀不了的 TIFF 由 OpenCV 讀。只問 `canRead` 為 true 的來源；`FormatInvalid` 換下一個，讀不到檔案的錯誤立刻回報；沒有來源認得檔名時，交給最後一個試一次（看內容）。
7. **校正值不從檔案讀**：影像來源一律給 `umPerPixel = 0`。PNG、BMP、JPEG、TIFF 的 DPI 常是 72、96 的預設值，分不出是真的還是預設；校正值由配方（`Board::umPerPixel`）或使用者給。
8. **不套用 EXIF 的旋轉**（Qt `setAutoTransform(false)`、OpenCV `IMREAD_UNCHANGED`）：座標要對到感測器的像素。
9. **`ImageBuffer` 的列緊密排列**（stride ＝ 寬 × 每像素 byte，起點對齊 64 byte）：QImage 與 `cv::Mat` 接受任何 stride，HALCON 的外部記憶體要緊密排列（**未驗證**，S08），緊密排列讓三個都有機會零複製。
10. **不能零複製的地方（明確標示並計數）**：Qt 的索引色、單色、`RGB888`、16 bit 彩色、預先乘 alpha 的格式（轉一次）；OpenCV 的 8 bit 彩色（BGR → BGRx，一次）、16 bit 彩色（轉 8 bit，再補 x：兩次）、灰階＋alpha（兩次）。包住外部記憶體的 `cv::Mat`（自己不擁有像素）不可以變成 view（會懸空），`fromMat` 拒絕它。
11. **OpenCV 的來源自己用 UTF-8 路徑讀檔，再交給 `imdecode`**，不用 `imread`。
12. **12 bit 的影像（使用者在 S06 指出影像也有 12 bit）**：`ImageView` 新增 `significantBits`（`Gray16` 的值在 0..2^n − 1、低位對齊；0 ＝ 格式的完整位元數）。12 bit 相機的影像常存成 16 bit 的檔案、值只到 4095；沒有這個欄位，畫面幾乎全黑（4095 ／ 65535 ≈ 6% 的亮度），use case 的對比檢查會低估 16 倍、把正常的樣本判成「對比太低」。
    - 兩個影像來源用同一個函式 `detectSignificantBits`（`domain/image_depth.h`）看最大值推測：≤ 255 → 8、≤ 1023 → 10、≤ 4095 → 12、≤ 16383 → 14，否則 16。這是**推測**：很暗的 16 bit 影像會被當成位元數較少（只會讓顯示比較亮、對比的換算比較寬鬆）；之後配方或相機設定知道真正的位元數時應該覆寫。
    - 畫布：有效位元數不到 16 的 `Gray16` 轉成 8 bit 顯示（右移 位元數 − 8），一次複製並計數；16 bit 的照樣零複製。
    - use case 的對比、之後的搜尋器：亮度一律用 `值 ÷ normalizedMax(effectiveBits)` 換算。
    - 實測的事實：OpenCV 4.9.0 讀 BitsPerSample ＝ 12 的**緊密打包** TIFF，給 16 bit、值左移 4 位（`0x123` → `0x1230`，高位對齊），有效位元數推測為 16，亮度的換算仍然正確。Qt 讀不了 TIFF。
    - 參考影像存檔時也存 `significantBits`；合成板子可以產生 12 bit 的 `Gray16`。

## 證據

- 零複製：`tst_image_qt`（QImage 的六種格式、ImageView 的五種格式，指標相等、計數 0；畫布的 `ImageItem::wrap` 也一樣）、`tst_opencv_image`（`cv::Mat` 的四種、ImageView 的五種）、`tst_shell`（開圖之後，Presenter 拿到的 view、畫布畫的 QImage 指到同一塊像素，計數 0）。
- 要複製的轉換恰好一次並計數：`tst_image_qt`、`tst_opencv_image` 的資料表（例：Qt 的 8 bit 灰階 BMP 是索引色檔，1 次；OpenCV 的 16 bit 彩色 TIFF，2 次）。
- 兩個來源一致：`tst_image_sources_agree` 列了 18 種檔案。16 種的格式、大小、校正值、有效位元數與像素完全相同（灰階、16 bit 灰階、12 bit 灰階（16 bit 容器）、彩色、alpha、彩色索引色、16 階灰索引色、256 階灰索引色、16 bit 彩色的 PNG；彩色、灰階 BMP；彩色、灰階、帶 EXIF 方向標記的 JPEG——JPEG 的差也是 0；PGM、PPM；16 bit 彩色兩邊的 `v / 257` 一致）；1 種是下面的已知例外（XFAIL）；1 種（灰階＋alpha 的 PNG）OpenCV 寫不出來，略過（SKIP），**沒有驗證**。
- 中文路徑：兩個來源都測了（`影像 測試/板子 一號.png`、`.tif`）。
- 12 bit：`tst_image_view`（推測的門檻，含最後一個像素、子影像）、`tst_image_qt`（12／10／16 bit 的 PNG；畫布把 4095 顯示成 255、計數 1 次）、`tst_opencv_image`（12 bit 的 TIFF；緊密打包的 12 bit TIFF 左移 4 位）、`tst_image_sources_agree`（兩個來源都推測出 12）、`tst_generate_objects`（12 bit 的紋理不會被判成對比太低；當成 16 bit 時會）、`tst_synthetic_board`（12 bit 的合成板子存成 PNG 再讀回，推測出 12、像素相同）。
- 不套用 EXIF 旋轉：`tst_image_qt` 在 JPEG 裡插入「方向 ＝ 6」，先證明 Qt 開啟自動旋轉時寬高會對調，再確認來源讀出原本的寬高；`tst_image_sources_agree` 兩邊一致。
- 量測（`docs/steps/S06-image-storage/perf.md`）：Qt 讀 5 MP 的 BMP、PNG、JPEG、灰階 PNG、16 bit PNG 都是 0 次複製；OpenCV 讀彩色是 1 次；兩邊解碼時間相近。

## 否決的替代方案

- **每次載入都複製到自己的正規緩衝**：簡單、所有權單純，但每次開圖多一次 5 MP／25 MP 的複製，違反這一步的目標。
- **彩色的正規格式用 `Bgr24` 或 `Rgb24`**：OpenCV 可以零複製，但 Qt 每次都要轉，而且 QPainter 畫 24 bit 格式每次繪製都要轉換（S02 的 I1 量的是 `RGB32`）。
- **各來源回傳自己的原生格式，由使用端處理**：違反 `IImageSource` 的契約（同一個檔案、相同的像素格式），S07／S08 的搜尋器得處理所有組合。
- **從檔案讀 DPI 當校正值**：預設值與真實值分不出來，錯的校正值會讓 µm 的樣本庫框錯大小。
- **計數器用注入的物件**：每個轉換函式都要多一個參數，零複製的包裝（例如 `wrapAsQImage`）也要帶著它；全程式共用的 atomic 足夠，測試取差即可。

## 後果

- **已知的例外**（`tst_image_sources_agree` 用 `QEXPECT_FAIL` 記住，行為改了會變成 XPASS）：色表是灰階、但不是 0..255 依序（例如 16 階灰）的 **BMP**，OpenCV 給 `Gray8`、Qt 給 `Bgrx32`；像素值相同，只有格式不同。原因是兩個解碼器對「灰階色表」的定義不同（Qt：索引 ＝ 灰階值；OpenCV 的 BMP：每個顏色都是灰；OpenCV 的 PNG：一律展開成彩色），用格式層級的規則無法讓兩邊在所有索引色檔案上都一致。產品的順序是 Qt 在前，所以實際開 BMP 時用的是 Qt 的結果。
- 畫布顯示完整 16 bit 的 `Gray16` 用 `QImage::Format_Grayscale16`（零複製），每次繪製時 Qt 要轉換；**這個繪製成本沒有量**（S06 沒有 16 bit 的 UI 量測）。12 bit 的影像開圖時轉一次 8 bit。
- 有效位元數是推測的（看最大值）；配方或相機設定可以覆寫的介面還沒有做。
- 開圖仍在 UI 執行緒同步解碼（S02 起就是）：25 MP 的 PNG 解碼約 0.5 秒（Qt 6.12.0，3 次的中位數 0.45–0.56 秒），畫面會停住。要不要改成背景解碼，是之後的決定（不在 S06 的範圍）。
- S07 的搜尋器拿到的是三種正規格式之一，自己決定要不要轉灰階（轉的話算一次複製，要計數）；不用處理 `Rgb24`、`Bgr24`（它們只出現在通用的轉換函式）。
- S08 要寫 `HImage` ⇄ `ImageView`：預期 `Gray8`、`Gray16` 的緊密排列可以零複製，彩色要從交錯排列轉成 HALCON 的分平面（一次複製）——**這些都是查文件之前的預期，未驗證**。
