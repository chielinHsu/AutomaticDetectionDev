#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "application/Result.h"
#include "application/ports/IExecutor.h"
#include "application/ports/IRecipeStore.h"
#include "domain/recipe.h"

// application 層：只用標準庫。
namespace aoi {

// 在背景存配方（S06，ADR-015）：存檔不佔用呼叫端（UI）執行緒，而且同一個路徑的存檔依序寫，
// 磁碟上最後留下的一定是最後交進來的那一份。
//
// 規則：
//  - save() 立刻回傳一個編號；序列化與寫檔在 executor 的 worker 上做。recipe 是呼叫端交出的複本（快照）。
//  - 同一個路徑同時只有一個存檔在寫；寫的時候又交進新的，就排在它後面。還沒開始寫的舊存檔被更新的取代：
//    舊的 done 收到 ErrorCode::Cancelled（寫下去也會立刻被覆蓋），只寫最新的那一份。
//  - 不同路徑互不影響，可以同時寫。路徑以字串比對：同一個檔案請用同一個寫法。
//  - done 在 worker 執行緒上呼叫（要回 UI 執行緒，呼叫端自己 post），每個編號恰好一次；done 丟的例外會被吞掉。
//  - store 丟出的例外變成 ErrorCode::Internal（IRecipeStore 的契約是不丟，這裡多一層保險）。
//  - 解構時等所有已交出的存檔寫完（不丟掉使用者的資料）。store 與 executor 要活得比它久。
//
// 呼叫端注意（S06 還沒有接上 UI）：存完才可以 markSaved，而且要標在「取快照時」的文件位置；
// 存檔期間文件又改了，文件仍然是未儲存。
class RecipeSaver {
public:
    struct Outcome {
        std::uint64_t ticket = 0;
        std::string path;
        Result<void> result;
    };
    using Done = std::function<void(const Outcome&)>;

    RecipeSaver(IRecipeStore& store, IExecutor& executor);
    ~RecipeSaver();
    RecipeSaver(const RecipeSaver&) = delete;
    RecipeSaver& operator=(const RecipeSaver&) = delete;

    std::uint64_t save(Recipe recipe, std::string path, Done done);
    // 等到沒有任何存檔在排隊或在寫。
    void waitIdle();
    // 還沒有結果的存檔數（排隊中＋寫入中）。
    std::size_t pending() const;

private:
    struct Request {
        std::uint64_t ticket = 0;
        Recipe recipe;
        Done done;
    };
    struct PathState {
        bool writing = false;
        std::optional<Request> next;
    };

    void drain(const std::string& path);
    static void finish(const Done& done, const Outcome& outcome) noexcept;

    IRecipeStore& store_;
    IExecutor& executor_;
    mutable std::mutex mutex_;
    std::condition_variable idle_;
    std::map<std::string, PathState> paths_;
    std::uint64_t nextTicket_ = 1;
    std::size_t pending_ = 0;
};

}  // namespace aoi
