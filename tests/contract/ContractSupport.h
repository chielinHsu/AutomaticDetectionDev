#pragma once

#include <QString>
#include <QtTest>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ContractTolerances.h"
#include "app/CompositionRoot.h"
#include "application/backend/BackendRegistry.h"
#include "application/backend/SearchEngine.h"
#include "QtTestPrinters.h"

// 契約測試（S09）共用：以工廠參數化（Liskov）。
//
//  - 參與的 back end ＝ 組裝根註冊的每一個（app::makeBackendRegistry：fake、opencv、halcon，產品用的就是這一份）。
//    每個測試函式的資料列是「一個 back end 一列」（列名 ＝ id），所以 log 裡每一列都看得到結果。
//  - 每個 back end 在這個行程只建立一次（makeSearchEngine；HALCON 第一次約 2.8 秒：另一個行程的授權檢查＋初始化）。
//  - 沒編進程式、沒有 runtime、沒有授權的 back end：那一列 QSKIP 並寫出原因（**SKIPPED，不是 PASSED**）。
//    scripts/dev/test.ps1 最後會列出「有跳過項目的測試」。建立失敗但不是「不可用」的錯誤 ＝ 失敗（QFAIL）。
namespace aoi::contract {

// 資料列：All ＝ 每個註冊的 back end；Vision ＝ 會讀像素的（準確度與「影像上真的沒有東西」的測試）。
enum class Rows { All, Vision };

inline const BackendRegistry& registry() {
    static const BackendRegistry r = app::makeBackendRegistry();
    return r;
}

inline std::vector<std::string> backendIds(Rows rows) {
    std::vector<std::string> ids;
    for (const BackendInfo& info : registry().list()) {
        if (rows == Rows::Vision && !profileOf(info.id).readsPixels) continue;
        ids.push_back(info.id);
    }
    return ids;
}

// _data() 用：一個 back end 一列；extra 可以再加欄位與列（例如每個 back end × 每個角度）。
inline void addBackendRows(Rows rows) {
    QTest::addColumn<QString>("backend");
    for (const std::string& id : backendIds(rows)) QTest::newRow(id.c_str()) << QString::fromStdString(id);
}

struct ContractEngine {
    SearchEnginePtr engine;
    std::string skipReason;  // 非空：這一列跳過（沒編進程式、沒有授權…）
    std::string failure;     // 非空：建立失敗，而且不是「不可用」
};

inline const ContractEngine& engineFor(const std::string& id) {
    static std::map<std::string, ContractEngine> cache;  // 只在測試的主執行緒用
    const auto found = cache.find(id);
    if (found != cache.end()) return found->second;
    ContractEngine entry;
    bool registered = false;
    for (const BackendInfo& info : registry().list()) {
        if (info.id != id) continue;
        registered = true;
        if (!info.available) entry.skipReason = id + " is not usable in this build: " + info.unavailable.message;
    }
    if (!registered) entry.failure = id + " is not registered by app::makeBackendRegistry";
    if (entry.skipReason.empty() && entry.failure.empty()) {
        Result<SearchEnginePtr> made = makeSearchEngine(registry(), id);
        if (made) entry.engine = made.value();
        else if (made.error().code == ErrorCode::BackendUnavailable) entry.skipReason = id + " is not usable here: " + made.error().message;
        else entry.failure = id + " could not be created: " + made.error().message;
    }
    return cache.emplace(id, std::move(entry)).first->second;
}

}  // namespace aoi::contract

// 測試函式的開頭：取這一列的 back end（變數 name 是 const SearchEngine&）。不能用時 QSKIP 並說原因。
// 是巨集而不是函式：QSKIP、QFAIL 只會從「呼叫它的那個函式」返回。
#define CONTRACT_BACKEND(name)                                                                                          \
    QFETCH(QString, backend);                                                                                           \
    const ::aoi::contract::ContractEngine& name##Entry_ = ::aoi::contract::engineFor(backend.toStdString());           \
    if (!name##Entry_.skipReason.empty()) QSKIP(qPrintable(QString::fromStdString(name##Entry_.skipReason)));          \
    if (!name##Entry_.failure.empty()) QFAIL(qPrintable(QString::fromStdString(name##Entry_.failure)));                \
    const ::aoi::SearchEngine& name = *name##Entry_.engine;                                                             \
    const ::aoi::contract::ContractProfile name##Profile = ::aoi::contract::profileOf(name.backend.id)

// 契約的錯誤碼：結果一定要是錯誤，而且是這一個錯誤碼。結果是成功時乾淨地失敗（「expected an error」），
// 不是在成功的 Result 上呼叫 error()（那會丟例外，整個測試執行檔中止，後面的列都沒有跑：S09 的變異測試實際遇到）。
#define QCOMPARE_ERROR(resultExpr, expectedCode)                                                                       \
    do {                                                                                                               \
        const auto result_ = (resultExpr);                                                                             \
        if (result_.ok()) QFAIL("expected an error (" #expectedCode "), got a success");                               \
        QCOMPARE_CODE(result_.error().code, expectedCode);                                                             \
    } while (false)
