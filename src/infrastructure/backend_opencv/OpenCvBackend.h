#pragma once

#include "application/backend/BackendRegistry.h"
#include "infrastructure/backend_opencv/OpenCvSearchOptions.h"

// infrastructure/backend_opencv：OpenCV back end 的工廠與執行緒政策（S07）。組裝根只認識這兩個函式。
namespace aoi::infra {

// 一組配套的 OpenCV adapter（範本建立器＋搜尋器，用同一份 options）。組裝根以 id "opencv" 註冊。
Backend makeOpenCvBackend(const OpenCvSearchOptions& options = {});

// 執行緒政策（S07，ADR-016）：OpenCV 的平行迴圈（cv::parallel_for_）最多用 threads 個執行緒。
// 組裝根傳入「核心數 − 1」（保留一個給 UI 執行緒），和 JobRunner 的 worker 數相同：latest-wins 時只有一個工作在跑，
// 被取代的舊工作在下一個檢查點就停，所以兩個池子不會同時把每個核心都佔滿。cv::setNumThreads 是整個程式共用的設定。
// threads ≤ 0 時不改（OpenCV 的預設是全部的核心）。回傳設定之後 OpenCV 回報的執行緒數。
int setOpenCvThreads(int threads);

}  // namespace aoi::infra
