#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// domain 層：只用 C++ 標準庫。晶格模組內部用的小工具，不是公開的型別。
//
// 鍵是 64 位元整數的雜湊表（開放定址、線性探測、容量是 2 的冪）。用途：投票的格子、晶格格子的佔用。
// 為什麼不用 std::unordered_map：每次插入都要配置一個節點，1 000 點的晶格擬合裡會插入幾千次，
// 這個成本和整個擬合的預算（T3：1 000 點 ≤ 2 ms）同一個量級。
namespace aoi::detail {

// 把兩個 32 位元的有號整數（格子座標）打包成一個鍵。
constexpr std::uint64_t packKey(int a, int b) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32) | static_cast<std::uint32_t>(b);
}
constexpr int unpackFirst(std::uint64_t key) noexcept { return static_cast<int>(static_cast<std::uint32_t>(key >> 32)); }
constexpr int unpackSecond(std::uint64_t key) noexcept { return static_cast<int>(static_cast<std::uint32_t>(key)); }

template <class V>
class FlatMap {
public:
    explicit FlatMap(std::size_t expected = 16) { reset(expected); }

    void reset(std::size_t expected) {
        std::size_t capacity = 16;
        while (capacity < expected * 2) capacity *= 2;
        keys_.assign(capacity, 0);
        values_.assign(capacity, V{});
        used_.assign(capacity, 0);
        mask_ = capacity - 1;
        size_ = 0;
    }

    // 找不到就插入預設值。回傳的參考在下一次插入之前有效（插入可能重新配置）。
    V& operator[](std::uint64_t key) {
        if ((size_ + 1) * 2 > keys_.size()) grow();
        std::size_t slot = hash(key) & mask_;
        while (used_[slot] != 0) {
            if (keys_[slot] == key) return values_[slot];
            slot = (slot + 1) & mask_;
        }
        used_[slot] = 1;
        keys_[slot] = key;
        values_[slot] = V{};
        ++size_;
        return values_[slot];
    }

    const V* find(std::uint64_t key) const noexcept {
        std::size_t slot = hash(key) & mask_;
        while (used_[slot] != 0) {
            if (keys_[slot] == key) return &values_[slot];
            slot = (slot + 1) & mask_;
        }
        return nullptr;
    }

    V* find(std::uint64_t key) noexcept {
        return const_cast<V*>(static_cast<const FlatMap&>(*this).find(key));
    }

    std::size_t size() const noexcept { return size_; }

    // 走訪順序由雜湊決定（同樣的輸入永遠同樣的順序），但不是插入順序；需要穩定輸出的呼叫端自己排序。
    template <class Fn>
    void forEach(Fn&& fn) const {
        for (std::size_t s = 0; s < keys_.size(); ++s) {
            if (used_[s] != 0) fn(keys_[s], values_[s]);
        }
    }

private:
    static std::size_t hash(std::uint64_t k) noexcept {
        k ^= k >> 33;
        k *= 0xFF51AFD7ED558CCDull;
        k ^= k >> 33;
        k *= 0xC4CEB9FE1A85EC53ull;
        k ^= k >> 33;
        return static_cast<std::size_t>(k);
    }

    void grow() {
        std::vector<std::uint64_t> oldKeys = std::move(keys_);
        std::vector<V> oldValues = std::move(values_);
        std::vector<std::uint8_t> oldUsed = std::move(used_);
        reset(oldKeys.size());  // 容量加倍（reset 取 ≥ 2 × expected 的 2 的冪）
        for (std::size_t s = 0; s < oldKeys.size(); ++s) {
            if (oldUsed[s] != 0) (*this)[oldKeys[s]] = std::move(oldValues[s]);
        }
    }

    std::vector<std::uint64_t> keys_;
    std::vector<V> values_;
    std::vector<std::uint8_t> used_;
    std::size_t mask_ = 0;
    std::size_t size_ = 0;
};

}  // namespace aoi::detail
