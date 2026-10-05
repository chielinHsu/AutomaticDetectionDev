#pragma once

#include "domain/component.h"
#include "domain/search_params.h"

// domain 層：只用 C++ 標準庫。
namespace aoi {

// 一份配方：一塊板子（元件實例＋它用到的類型複本）加上上次使用的搜尋參數。
// IRecipeStore（S06）負責存取；schemaVersion 讓檔案格式可以演進（S06 定義遷移）。
struct Recipe {
    int schemaVersion = 1;
    Board board;
    SearchParams params;
};

}  // namespace aoi
