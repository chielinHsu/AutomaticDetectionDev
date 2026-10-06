# S06 — 影像緩衝、轉接器、配方與樣本庫儲存：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S06-image-storage.md`](../../plan/S06-image-storage.md)、S03 的 port 契約、S04 的提醒（暫代的 `ImageLoader`／`ImageViewAdapter`）、ADR-006 §4–§6。
> 本檔講「為什麼這樣做」；決策在 [ADR-014](../../adr/014-image-ownership-and-copies.md)（影像）與 [ADR-015](../../adr/015-recipe-and-library-storage.md)（儲存）；圖在 [`diagram.md`](diagram.md)；實測數字在 [`perf.md`](perf.md)。

<!--STE-->
## Purpose
This step sets the rules for image memory.
It builds the converters between Qt, OpenCV and the shared image view.
It builds the image sources, the recipe store and the sample library store.
It also builds a generator of test boards with known answers.
<!--/STE-->

**繁中**：定下影像記憶體的規則（誰擁有像素、誰可以讀、什麼時候算一次複製）；做出 Qt、OpenCV 與共用的 `ImageView` 之間的轉換；做出影像來源、配方儲存與樣本庫儲存；再加一個帶標準答案的合成板子產生器。

<!--STE-->
## Reason
A copy of a 5 MP image costs time and memory. A shared view costs almost nothing.
A count of copies turns "zero copy" into a fact that a test can check.
A recipe file must survive a crash during a save and a damaged byte.
A test board with known answers lets us measure speed and accuracy together.
<!--/STE-->

**繁中**：5 MP 影像複製一次要時間也要記憶體，共用的 view 幾乎不花成本；把複製「數出來」，零複製才是測試可以斷言的事實，而不是註解裡的願望。配方檔要經得起存到一半當機、經得起壞掉的 byte。有標準答案的合成板子，之後（S07 起）才能同時量速度與準確度。

<!--STE-->
## Effect
Qt decodes the common formats into the view with zero copies.
The canvas and the search read the same pixels.
The OpenCV source reads TIFF and 16-bit files. It makes one copy for each color file.
A recipe with 5000 parts saves in about 0.1 s and loads in about 0.03 s.
The background save takes less than 1 ms on the caller thread.
<!--/STE-->

**繁中**：PNG、JPEG、BMP（彩色、灰階、16 bit 灰階）由 Qt 解碼後直接成為 view，**0 次複製**；畫布顯示的與搜尋讀的是同一塊像素（測試用指標相等與計數斷言）。OpenCV 的來源補上 TIFF 與 16 bit（這台機器的 Qt 沒有 TIFF 外掛），讀彩色檔要複製一次（已計數）。5 000 個元件的配方存檔約 0.1 秒（3 次中有 1 次 0.48 秒，原因不明）、讀檔約 0.03 秒；背景存檔佔呼叫端（UI）不到 1 ms。數字都是這台機器、Release 的實測（`perf.md`）。

<!--STE-->
## How the image memory works
A view points to pixels and holds a shared owner.
Nobody changes the pixels after the first view leaves the producer.
Thus any thread can read them without a lock.
A copy of a view copies two pointers and a reference count.
A sub-view shares the owner of the full image.
A Qt image or an OpenCV matrix can wrap the same pixels. Then the owner stays alive until the wrapper goes away.
<!--/STE-->

**繁中**：`ImageView` 指到像素，並持有共用的擁有者（`owner`）。規則（`image_view.h`，ADR-014）：（1）像素交出去之後沒有人再改，所以任何執行緒都可以不加鎖地讀；（2）複製 view 只複製指標與參照計數；（3）跨執行緒、跨 port、活得比這次呼叫久的 view 一定要有 owner；（4）子影像與整張影像共用 owner；（5）QImage 或 `cv::Mat` 包住同一塊像素時，owner 活到包裝消失，而且包裝不可以寫；（6）做不到零複製的轉換要配新緩衝並計數。要產生像素的人用 `ImageBuffer`：列緊密排列、起點對齊 64 byte、交出 view 之前才可以寫。

<!--STE-->
## Three formats for every source
Every image source gives one of three formats: Gray8, Gray16 or Bgrx32.
Thus the same file gives the same view from Qt and from OpenCV.
Qt decodes color files into Bgrx32. So Qt needs no copy.
OpenCV decodes color files into three bytes for each pixel. So OpenCV needs one copy.
The program asks Qt first and OpenCV second.
<!--/STE-->

**繁中**：`IImageSource` 的契約是「同一個檔案，所有來源給出相同語意的 view」。具體寫法：只交出三種正規格式——8 bit 灰階 `Gray8`、16 bit 灰階 `Gray16`、其他一律 `Bgrx32`（16 bit 彩色以 `v / 257` 四捨五入換成 8 bit）。選 `Bgrx32` 是因為 Qt 解出來就是它（零複製），也是 QPainter 畫得最快的格式。OpenCV 解出 B、G、R 三個 byte，補成四個要複製一次；所以組裝時 **Qt 在前、OpenCV 在後**（計畫原本寫「Qt 當後備」，改的理由是複製次數，兩邊解碼時間相近，見 `perf.md`）。兩個來源對 18 種檔案的比對：16 種完全相同（像素逐 byte 相同），1 種是已知例外，1 種沒有驗證（下面「已知的限制」）。

<!--STE-->
## Twelve-bit images
Many cameras give 12-bit values in a 16-bit file. The values stop at 4095.
Without a bit depth, the screen shows a dark image and the contrast check fails.
So each view of 16-bit gray records the number of bits that it uses.
Both sources find this number from the largest value.
The canvas converts these images to 8 bits for display. That conversion counts as one copy.
<!--/STE-->

**繁中**：使用者的影像也有 12 bit。12 bit 相機的影像常存成 16 bit 的檔案、值只到 4095；沒有位元深度，畫面幾乎全黑（只有 6% 的亮度），use case 的對比檢查也會低估 16 倍、把正常的樣本判成「對比太低」。所以 `ImageView` 多了 `significantBits`：兩個影像來源用同一個函式看最大值推測（≤ 4095 → 12，也涵蓋 8、10、14 bit），這是推測，配方或相機設定之後可以覆寫。畫布把位元數不到 16 的 `Gray16` 轉成 8 bit 顯示（一次複製，已計數）；亮度一律用「值 ÷（2^位元數 − 1）」換算。實測：OpenCV 讀「12 bit 緊密打包」的 TIFF 會給 16 bit、值左移 4 位（放滿範圍），當成 16 bit 處理就是對的。

<!--STE-->
## What counts as a copy
A copy writes the same image into a new buffer. It changes only the container, the layout or the format.
Each copy calls one counter function.
A test reads the counter before and after an operation.
A decode, a filter and a save do not count. They make new content or bytes.
<!--/STE-->

**繁中**：一次「複製」＝為了換容器、記憶體排列或像素格式，把同一張影像寫到新緩衝（例如索引色轉灰階、BGR 轉 BGRx、16 bit 轉 8 bit、彩色轉灰階給函式庫用）。每個做複製的地方呼叫一次 `countImageCopy`（`domain/image_copy.h`，任何層都能呼叫）；測試取前後差。解碼（之前沒有像素）、演算法的新內容（模糊、金字塔）、存檔的序列化不算。

<!--STE-->
## How the recipe store keeps the file safe
The store writes JSON with a format tag and a schema version.
It writes every field. Thus a load gives back the same recipe.
It reads strictly. A bad field gives an error with the path to that field.
It writes to a temporary file first. Then it replaces the old file in one step.
A failed save leaves the old file as it was.
An older file goes through one migration for each version.
<!--/STE-->

**繁中**：配方是 UTF-8 的 JSON（縮排），頂層有 `format` 與 `schemaVersion`。每個欄位都寫，`load(save(r))` 逐欄位、浮點數逐位元相同（唯一的例外是 −0.0 讀回 +0.0：JSON 不分正負零）。讀的時候嚴格：缺欄位、型別錯、列舉不認得、id 重複、參照不存在都回 `FormatInvalid`，訊息帶 JSON 路徑；寫的時候存不下的內容（非有限的數字、不是 UTF-8 的字串）回 `InvalidArgument`、不寫檔。寫檔用 `QSaveFile`：先寫暫存檔、成功才換名，失敗時舊檔原封不動。舊版本逐版遷移；比程式新的檔案不讀。v1 的範例檔（手寫）永遠要讀得進來。

<!--STE-->
## How the background save works
The caller gives a copy of the recipe and gets a ticket at once.
A worker thread writes the file.
Two saves of one file never run at the same time.
A newer save replaces an older save that did not start.
Thus the last save that the caller gave is the last file on the disk.
<!--/STE-->

**繁中**：「非同步寫檔」由 application 的 `RecipeSaver` 做：呼叫端交出配方的複本（快照），立刻拿到編號；寫檔在 `IExecutor` 的 worker 上。同一個檔案同時只寫一份，寫的時候又交進來的排在後面；還沒開始寫的舊存檔被更新的取代（收到 `Cancelled`）。所以磁碟上最後留下的，一定是最後交出的那一份。解構時等所有存檔寫完。之後接 UI 的人要注意：存完才 `markSaved`，而且要標在取快照時的文件位置。

<!--STE-->
## How the library layers work
The library has layers, as in the TRI software: project, user and system.
A search goes from the top layer to the bottom layer.
In each layer, the part number comes first. Then the aliases come.
A save goes to the top writable layer with that part number.
If no layer has it, the save goes back to the source layer of the type.
Other types go to the user layer. The system layer is read-only.
<!--/STE-->

**繁中**：使用者決定照 TRI 的做法分層（2026-10-06）：**專案 → 使用者 → 系統**（系統層唯讀，專案層與系統層可以沒有）。每一層是一個 JSON 檔（`JsonLibraryStore`），組合在 `LayeredLibraryStore`。查找一層一層往下，同一層先比料號再比別名（去掉空白、不分大小寫）；上層遮住下層同料號的條目。存檔：① 已經有這個料號的最高可寫層；② 沒有的話，`libId` 指到的那一層（TRI：從專案層載入的存回專案層）；③ 其他存到使用者層。條目的 id 由那一層指派（`proj-`、`user-`、`sys-` 加流水號），永不重複使用，所以板子複本的 `libId` 也記得來自哪一層。壞掉的庫檔不會被覆蓋。

<!--STE-->
## The test boards
The generator draws an array of parts with known positions.
It can add a tilt, a position error, missing parts, glare, blur and noise.
The same settings and seed give the same image and the same answers.
It writes the image as PNG and the answers as JSON.
<!--/STE-->

**繁中**：`tests/support/synthetic_board` 畫一個元件陣列（主體＋兩端電極，邊緣反鋸齒），可以加傾斜、位置誤差、缺件、反光、模糊與雜訊，輸出影像與標準答案（每個元件的位姿、是否缺件、是否反光）。同樣的參數與 seed 給出逐位元相同的影像與答案；不傾斜時連跨平台也相同（測試鎖住像素雜湊）。可以存成 PNG＋JSON。

<!--STE-->
## What this step does not do
This step does not add a save command or an open command to the screen.
It does not build the HALCON converters. The HALCON files for C++ are not on this computer.
It does not decode images in the background. A 25 MP PNG stops the screen for about 0.5 s.
<!--/STE-->

**繁中**：（1）畫面上沒有〔儲存〕與〔開啟配方〕（使用者決定這一步不接；這是必要功能，記在 `docs/current-status.md` 的待辦）；開啟配方還需要 v2 的 schema（v1 沒有影像路徑）。（2）HALCON 的 `HImage` ⇄ `ImageView` 與 `HalconImageSource` 沒有做：這台機器只有 HALCON runtime，沒有 `HalconCpp.h` 與 `.lib`，寫了也無法編譯與驗證（S08 補裝開發元件後做）。（3）開圖仍在 UI 執行緒同步解碼：25 MP 的 PNG 約 0.5 秒畫面停住。（4）分層的樣本庫還沒有組裝進程式（沒有 UI 用它）。

## 元件與檔案

| 檔案 | 內容 |
|---|---|
| `src/domain/image_view.h` | 所有權規則 1–6；新增 `Bgr24`、`Gray16`、`toString(PixelFormat)`、`rowBytes`、`packed`、`subView` |
| `src/domain/image_buffer.h/.cpp` | `ImageBuffer`：自己擁有像素、緊密排列、對齊 64 byte、清成 0；不合法的大小得到無效的緩衝 |
| `src/domain/image_copy.h/.cpp` | `countImageCopy`、`imageCopyCount`（atomic） |
| `src/domain/image_depth.h/.cpp` | 12 bit：`detectSignificantBits`（看最大值推測）、`effectiveBits`、`normalizedMax` |
| `src/application/ports/IImageSource.h`、`IRecipeStore.h`、`ILibraryStore.h` | 契約補完（正規格式、canRead 只看檔名、可重入、−0、原子寫入、分層） |
| `src/application/image/ImageSourceChain` | 幾個來源依優先順序：只問認得檔名的、`FormatInvalid` 換下一個、讀不到檔案立刻回報 |
| `src/application/document/RecipeSaver` | 背景存檔：同一個檔案依序、被取代的回 `Cancelled`、解構等寫完 |
| `src/infrastructure/image_qt/QtImages`、`QtImageSource` | `fromQImage`、`wrapAsQImage`、`toCanonicalQImage`；`QImageReader` 的來源（看內容決定格式、不套用 EXIF 旋轉） |
| `src/infrastructure/backend_opencv/OpenCvImages`、`OpenCvImageSource` | `fromMat`、`borrowAsMat`；`imdecode` 的來源（自己讀 UTF-8 路徑）。`AOI_WITH_OPENCV=ON` 才建置 |
| `src/infrastructure/storage/JsonCodec`、`JsonFile` | domain ⇄ JSON；有版本的文件檔（讀、原子寫入、遷移） |
| `src/infrastructure/storage/JsonRecipeStore`、`JsonLibraryStore`、`LayeredLibraryStore` | 配方；一層樣本庫；TRI 的分層 |
| `src/presentation/shell/MainWindow`、`presenters/AutoRoiPresenter`、`canvas/ImageItem`、`canvas/ImageViewMetaType.h` | 開圖改走 `IImageSource`；訊號帶 `ImageView`（零複製到 Presenter）；畫布顯示新的兩種格式；`ImageLoader`、`ImageViewAdapter` 刪除 |
| `src/app/CompositionRoot`、`main.cpp` | `makeImageSource()`：Qt 在前、OpenCV 在後 |
| `src/application/usecases/GenerateObjectsFromSample.cpp` | 對比檢查讀 `Bgr24`、`Gray16`（16 bit 換到 0..255，門檻的單位不變） |
| `tools/check-deps.mjs` | `qt-gui` 類別（白名單）、`infrastructure/image_qt` 的規則；fixture 由 13 個變 15 個 |
| `tests/support/` | `synthetic_board`（標準庫）、`synthetic_board_io`（PNG＋JSON） |
| `tests/infra/` | 7 個測試執行檔（其中 2 個要 OpenCV）、`perf_storage`、`fixtures/recipe-v1.json`、`library-v1.json` |

## 與計畫不同的地方

| 計畫 | 實際 | 理由 |
|---|---|---|
| 產出位置 `src/infrastructure/{image,storage}/` | Qt 的轉換與來源在 `infrastructure/image_qt`；OpenCV 的在 `backend_opencv`；HALCON 的會在 `backend_halcon` | `check-deps`（S03）規定視覺函式庫只能出現在各自的 back end；QImage 屬於 Qt Gui，原本的規則沒有允許任何 adapter 用 Qt Gui，所以新增一層並登記規則（開工時向使用者說明過） |
| Qt 是後備（fallback） | Qt 在前，OpenCV 在後 | 常見格式 Qt 零複製、OpenCV 要複製一次；解碼時間相近（`perf.md`） |
| `HImage` ⇄ `ImageView`、`HalconImageSource` | 沒有做 | 本機沒有 HALCON 的 C++ 開發檔；寫了無法編譯也無法驗證。留給 S08 |
| `IRecipeStore` 的「非同步寫檔」 | port 維持同步；`RecipeSaver` 負責背景與順序 | 同步的 port 容易測；同一個檔案的順序要有一個地方保證 |
| 樣本庫「建議 Project＋User 兩層」 | TRI 的三層（專案、使用者、唯讀的系統；專案、系統可以沒有） | ADR-006 與 ux-spec 的建議衝突，使用者決定照 TRI |

## 已知的限制

1. **索引色檔案的格式例外**：色表是灰階、但不是 0..255 依序（例如 16 階灰）的 **BMP**，OpenCV 給 `Gray8`、Qt 給 `Bgrx32`（像素值相同）。兩個解碼器對「灰階色表」的定義不同。產品的順序是 Qt 在前，所以開 BMP 時用的是 Qt 的結果（`tst_image_sources_agree` 以 `QEXPECT_FAIL` 記住）。
2. **灰階＋alpha 的 PNG** 兩邊是否一致**沒有驗證**（OpenCV 寫不出這種檔案；Qt 會解成 `ARGB32`，OpenCV 的處理照 Qt 的語意寫成彩色）。
3. **`Gray16` 的顯示成本沒有量**：畫布用 `QImage::Format_Grayscale16`（零複製），Qt 每次繪製要轉換。
4. **開圖在 UI 執行緒**：25 MP 的 PNG 解碼約 0.5 秒、TIFF 約 0.6 秒（OpenCV），畫面停住；5 MP 的 PNG 約 0.1 秒。
5. **校正值不從檔案讀**（一律 0）：配方或使用者要給；沒有校正值時，框仍以 1 µm ＝ 1 像素展開（S04 的暫定）。
6. **樣本庫一層一個檔案**：存一筆要整份重寫（1 000 筆約 0.12–0.14 秒）；第一次讀檔之後用記憶體的內容，別的程式同時改同一個檔案不會被發現。
7. **大小寫折疊是 Qt 的簡單折疊**：`Ä` ＝ `ä`，但 `ß` ≠ `ss`。
8. **Windows 上同時存同一個配方檔**：有的呼叫回 `IoFailed`（換名撞在一起），檔案仍完整；要依序存用 `RecipeSaver`。
9. **HALCON 的部分全部未驗證**（見上）。
10. **有效位元數是推測的**：看最大值，很暗的 16 bit 影像會被當成 12 或 14 bit（顯示比較亮、對比的換算比較寬鬆）；配方或相機設定可以覆寫的介面還沒有做。12 bit 影像實際怎麼存（16 bit 容器還是緊密打包）要用使用者的真實檔案確認。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | 轉換（`QtImages`、`OpenCvImages`）、載入（`*ImageSource`）、組合（`ImageSourceChain`）、編解碼（`JsonCodec`）、檔案（`JsonFile`）、配方、一層庫、分層、背景存檔各自一個類別 | 每個都有自己的測試執行檔；超過 300 行的有兩個：`JsonCodec.cpp` 529 行（讀寫兩個方向、每個型別一個函式；拆成讀、寫兩個檔案會讓同一個欄位的兩半分開，改格式時容易漏一邊）、測試用的 `tests/support/synthetic_board.cpp` 317 行（產生、繪製、標準答案） |
| O | 新的影像來源 ＝ 實作 `IImageSource` ＋ 在 `makeImageSource` 加一行；新的庫層 ＝ `LibraryLayer` 多一筆 | OpenCV 來源加進來時，`application/`、`domain/` 不用改（`ImageSourceChain` 用假的來源測） |
| L | Qt 與 OpenCV 兩個來源對同一個檔案給出相同的 view | `tst_image_sources_agree`：18 種檔案中 16 種相同，1 種已知例外（XFAIL），1 種未驗證 |
| I | port 不變（`IImageSource` 2 個方法、`IRecipeStore` 2 個、`ILibraryStore` 4 個）；分層的額外查詢（`layerOf`）只在具體類別上 | 編譯 |
| D | `RecipeSaver` 只認識 `IRecipeStore` 與 `IExecutor`；主視窗只認識 `IImageSource`；具體的來源只在 `app/` 組裝 | `check-deps` 0 違規；`tst_recipe_saver` 用假的 store |

## 如何驗證

- [x] 建置通過（Release、Debug；OpenCV 關與開各一次）
- [x] 測試通過（`tests/domain/tst_image_view`、`tests/core/tst_image_chain`、`tst_recipe_saver`、`tst_generate_objects` 的格式測試、`tests/infra/` 全部、UI 測試改用 `ImageView` 之後全過）
- [x] `node tools/check-deps.mjs`（含 `--self-test` 的 15 個 fixture）
- [x] `node tools/ste-lint.mjs --strict` 無 error
- [x] `perf.md` 的指標都已量測（S06 沒有預算；U4 重量並與預算比對）
