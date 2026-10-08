# ADR-017 HALCON back end：shape model、延遲載入與授權判斷、角度步進與範圍餘量、位姿換算

- 日期：2026-10-07
- 決定者：Claude Code (Claude Opus 5.5) 提出並實作（S08）。使用者在 S08 開工時決定兩件事：（1）HALCON 的開發檔從 CV_Algorithm 專案複製到本專案；（2）runtime（RTL）與 development（SDK）兩種 dongle 要能分辨，授權錯了要提醒。其餘是這份 ADR 的提案。
- 狀態：採用（調整參數是合成板子與真實影像定的起點；HALCON 比 OpenCV 好不好是 S09 的 A/B 比較，這份 ADR 不下結論）

## 背景

S08 要做第二個 back end（`docs/plan/S08-halcon.md`）。開工時的事實（實際查過）：

- 這台機器的 HALCON 24.11.2 只有 runtime：`%HALCONROOT%\bin\x64-win64` 有 DLL、`license\` 有兩個授權檔，**沒有 `include\` 與 `lib\`**。使用者指示從 `D:\AOI_Project\SType\AlgoParamFineTune\BnSDKLibsController\CV_Algorithm\HALCON-24.11-Steady\` 複製（那個專案把 SDK 放在版控裡，版本也是 24.11.2）。
- 授權：`license.dat` 是 RTL（CodeMeter 容器 `3-7614034`，這台有插），`license--DEVELOP-.dat` 是 SDK（容器 `3-7081033`，**沒有插**）。HALCON 會挑找得到 dongle 的那一個，所以程式照樣能跑（用 runtime 授權），不會報錯。
- HALCON 的運算子說明在 `%HALCONROOT%\help\operators_en_US.ref`（文字檔），執行時也能用 `get_operator_info` 查；這次的參數與多執行緒行為都是查這兩個來源，不憑記憶。

## 決定

1. **開發檔放在 repo 的 `third_party/halcon/`**（標頭全部＋`halconcpp.lib`，CV_Algorithm 也只連這一個），`.gitattributes` 設 `-text` 保持原樣、`.gitignore` 對這一個 `.lib` 開例外。`cmake/FindHalconLocal.cmake` 建 `Halcon::HalconCpp`；`AOI_HALCON_SDK_DIR` 可以換成別的 SDK。執行時的 DLL 與授權來自 `%HALCONROOT%`（不複製 80 MB 的 `halcon.dll`）。
2. **`halconcpp.dll` 延遲載入**（`/DELAYLOAD`）：沒有安裝 HALCON 的電腦，`AOI_WITH_HALCON=ON` 的程式照樣啟動。任何 HALCON 呼叫之前，`HalconRuntime::halconRuntimeProblem()` 先載入 DLL、比對版本（主、次版本要和標頭相同）、在結構化例外的保護下一次綁定所有延遲載入的函式；有問題回原因，程式不當掉。列 back end 時只看 PATH 上有沒有 `halconcpp.dll`（微秒級），授權等真的選了 halcon 才檢查（第一次約 1.6 秒）。
3. **授權的判斷是純函式**（`judgeHalconLicense`，`HalconLicense.h`）：輸入是 `get_system` 的 `current_license_info`、`hostids`、`available_license_files`、`unlicensed_operators` 與第一個運算子的成敗；授權檔只讀 feature、ID、INFO。規則：程式庫載不起來、沒有有效授權、授權不含 shape matching → `BackendUnavailable`，原因寫出找到的授權檔與這台電腦的 ID；能用，但用的是 runtime 授權、而某個 development 授權檔的 ID 都不在這台電腦上 → **提醒**（`SdkDongleMissing`）：stderr 一行、訊息列一個吐司、back end 選單的提示。只有 runtime 授權（部署的電腦）不提醒。
4. **搜尋用 shape model**（`create_shape_model`／`find_shape_model`），參數多半 'auto'，例外兩個（S08 實測，`perf.md` 的 AS-*）：
   - **角度步進自己算**：範本轉一步，角落移動 0.5 像素（`atan(0.5 ／ 半對角線)`）。'auto' 對 48×28 的樣本給 5.2°；說明寫「AngleExtent 不是步進的整數倍時，步進會調整」，所以 ±5° 只剩 −5、0、5 三個角度，傾斜 3° 的元件有一半回報 5°（精修拉不回來）。
   - **模型的角度範圍兩端各多 2 步**：範圍剛好等於要求時，接近兩端的元件精修被範圍卡住（傾斜 −4° 回報 −5°、位置差 0.4 像素）。搜尋只要求使用者的範圍，結果的角度夾回範圍內（契約）。
   - 次像素 `least_squares`（說明建議的取捨）；`greediness` 0.9；`MaxOverlap` 0.5，之後再做 domain 的 NMS（和 OpenCV 同一套契約）。**不預先產生模型**（說明：預先產生的模型在不同大小的影像上平行搜尋「可能當掉」）。
5. **位姿換算**（`HalconPose.h`）：`find_shape_model` 的 Row、Column 是配 `affine_trans_pixel` 調過的值（說明原文），參考點真正的位置是 `(Row, Column) ＋ R(Angle)·(0.5, 0.5) − (0.5, 0.5)`；換成 ADR-010：`x ＝ Column ＋ 0.5(sin ＋ cos)`、`y ＝ Row ＋ 0.5(cos − sin)`。角度：`hom_mat2d_rotate` 的矩陣作用在 (row, column) 上，展開和 `geometry.h` 的 `rotate()` 一樣，所以只換單位、不改正負號。兩件事都用傾斜的合成板子實測並以測試鎖住（ADR-010 的「後續（S08）」）。
6. **影像**（`HalconImages.h`）：`Gray8` 緊密排列時 `gen_image1_extern` 零複製（不給清理函式，像素的生命週期由 `HalconGray` 管：`keepAlive` 先建、最後解構）；子影像要複製（外部影像要一列接一列）。`Gray16` 預設依有效位元數換成 8 bit（查表；和 OpenCV 同一個輸入，A/B 公平），`nativeGray16` 選項直接當 `uint2`（零複製，實測一樣準、更快）。彩色用和 OpenCV `cvtColor` 同一組定點係數。
7. **搜尋區域**：視窗清單是一次 `find_shape_model`，影像的 domain ＝ 所有視窗（各往外 2^(層數−1) 像素，說明建議）的聯集，結果再用視窗本身過濾。
8. **取消**：`find_shape_model(s)` 支援 `interrupt_operator(…, 'cancel')`（`get_operator_info` 實查）；`HalconCancelWatch` 在搜尋期間每毫秒看一次旗標，升起就打斷。`create_shape_model` 不支援中斷（'none'），只在前後檢查。
9. **執行緒**：建立 back end 時 `set_system('thread_num', 核心數 − 1)`（和 JobRunner、OpenCV 同一個數字，ADR-016 的理由）。`create_shape_model`、`find_shape_model(s)` 的 parallelization 都是 'no_restriction'；同一個模型可以被幾個 worker 同時用（測試驗證）。
10. **流程 B**（`HalconLibraryMatcher`）：每個候選類型一個模型，**一次 `find_shape_models`**，`MaxOverlap` 給每個模型一個值（說明：這樣只有同一個模型的結果互相抑制，等於分開呼叫）。實測和每個類型分開呼叫給出一樣的元件；N ≥ 10 時快 1.1–2.0 倍（N ＝ 1000：1.0–1.5 倍），N ＝ 1 時反而慢一點；比 OpenCV 快 2.4–3.6 倍（N ＝ 1000）。ADR-006「每個條目一個 shape model、一次找多個」的預期成立（S08 確認）。合併成元件的規則和 OpenCV 版相同（兩個 adapter 不能共用程式碼，抄一份；S09 的契約測試鎖住兩邊一致）。

## 證據

事實（已觀察，數字見 `docs/steps/S08-halcon/perf.md`）：

- 授權探測：這台機器是 runtime 授權（`3-7614034`，Steady，模組含 Matching），SDK 的授權檔在、dongle 不在 → 提醒出現（`tst_halcon_backend`、`tst_cli`）。`HALCONROOT` 指到空資料夾 → 錯誤 2036，`--backend=halcon` 結束碼 2；PATH 上沒有 HALCON → 登記成不可用，`--backend=fake` 照常啟動（`tst_cli`）。
- **真的讀不到 dongle**（S08 驗證途中，HALCON 突然讀不到一直插著的 RTL dongle，重插後恢復，原因不明）：錯誤 2300（Dongle not attached, or can't read dongle），`--backend=halcon` 結束碼 2；HALCON 的測試跳過並說原因。那時 `get_system` 查不到授權檔與主機 ID，所以探測在沒有有效授權時改讀 `%HALCONROOT%\license\*.dat`，原因寫成「`license--DEVELOP-.dat` 需要 `CM:3-7081033`（development）、`license.dat` 需要 `CM:3-7614034`（runtime）」，並說用 `hhostid.exe` 查這台電腦的 ID。
- 位姿：傾斜 ±3°、+4.5° 的板子全部找到，角度誤差 0.09–0.21°（如果正負號反了會是 6° 以上）；±30° 的板子在 |傾斜| ≥ 20° 時，修正後平均誤差 0.018 像素、沒修正 0.303 像素（`theRotatedPoseCorrectionIsNeeded`）。
- 'auto' 的角度步進 5.227°（`get_shape_model_params`）；改成自己算＋餘量之後，−5°..+5° 每 0.5° 的掃描角度誤差最大 0.52°（在 0° 附近：±0.5° 的元件有一部分回報接近 0°，每種設定都一樣，是 HALCON 本身的行為），兩端的誤差 0.04–0.06°。
- 合成板子組（8 塊 5 MP、6 673 個元件）：召回率 100%；位置誤差 p95 0.48 像素（預算 0.5，**3 次都通過但很接近**；雜訊 12＋模糊 2 的板子最大 1.36 像素；反光的板子 HALCON 比較好：0.14 對 OpenCV 0.50）——同一次 OpenCV p95 0.11。shape model 只看邊緣點，雜訊大時比 NCC 不穩。
- T2b（5 MP 種子搜尋）p95 31.5、35.4 ms，機器整體變慢的那一次 82.2 ms（預算 80）；同一次的 OpenCV 80.8–84.7 ms。
- 真實影像：LA 兩個 back end 的晶格相同；HA 上 HALCON 用預設參數只找到少數針腳（`perf.md` §4）。

假設（未驗證）：

- 這些預設值在使用者的真實影像以外的板子上也合適。
- HALCON 新版的 'auto' 角度步進與 0° 附近的行為不變（換版本要重跑 `tst_halcon_backend` 與 `perf_halcon` 的 AS-*）。

## 否決的替代方案

- **在 runtime 上用 `LoadLibrary` 手寫 HALCON/C 的宣告**（開工時的備案）：宣告寫錯是未定義行為、無法對照官方標頭；使用者提供了開發檔，不需要。
- **直接連結（不延遲載入）**：沒有 HALCON 的電腦，`AOI_WITH_HALCON=ON` 的程式一啟動就被 Windows 擋下（找不到 DLL），連 `--backend fake` 都不能用。
- **啟動時就檢查授權**（讓選單一開始就知道 halcon 能不能用）：每次啟動多 1.6 秒，即使使用者用的是別的 back end。
- **'auto' 的角度步進**：見決定 4 的實測。
- **每個類型一次 `find_shape_model`（流程 B）**：結果一樣、比較慢；保留成選項（`libraryOneCall ＝ false`）供量測。
- **HALCON 的影像來源（`read_image`）**：S06 的提醒裡有，但 `release-halcon` 已經有 OpenCV 讀 TIFF；HALCON 的來源要和 Qt、OpenCV 在 18 種檔案上給出相同的影像（`tst_image_sources_agree`），是另一塊工作。沒有做，留給使用者決定要不要。
- **`create_ncc_model` 當對照**（plan 寫「可選」）：沒有做；OpenCV 的 NCC 已經是強度相關法的對照。

## 後果

- `AOI_WITH_HALCON=ON` 的建置需要 `third_party/halcon`（已在 repo），執行需要 `%HALCONROOT%` 的 runtime 與授權；沒有時 halcon 不可用、程式照常。
- `Backend` 結構沒有改（沒有 `libraryMatcher` 欄位，流程 B 的 UI 等使用者決定），application、domain 沒有為了 HALCON 改（開放封閉）。presentation 改了 back end 選單（不可用的項目停用、提示原因）；`copy.json` 新增兩個 key。
- 換 HALCON 版本：換掉 `third_party/halcon` 整份、更新 `README.md` 與 `AI_CONTEXT.md`，重跑 `tst_halcon_*` 與 `perf_halcon`。

## 後續（2026-10-07）：建置改 qmake

決定 1 的 `cmake/FindHalconLocal.cmake`（`Halcon::HalconCpp`）隨 CMake 一起移除（[ADR-018](018-qmake-only-build.md)）。同樣的工作改由 `AiInspTool/qmake/aoi_halcon.pri`（找標頭與 `halconcpp.lib`，預設 `third_party/halcon`，`AOI_HALCON_SDK_DIR` 可以換；讀標頭與 runtime 的版本，主、次版本不同或找不到 runtime 時 qmake 印警告）與 `aoi_target.pri`（`NOMINMAX`、`halconcpp.lib delayimp.lib version.lib`、`/DELAYLOAD:halconcpp.dll`）負責。HALCON **預設編進去**（`AOI_WITH_HALCON=OFF` 才關）；`tst_halcon_*`、`tst_cli` 在 qmake 的建置上照樣全部通過。
