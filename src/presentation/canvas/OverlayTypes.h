#pragma once

#include <array>

namespace aoi::ui {

// 疊在影像上的框的視覺狀態（docs/ux/ux-spec.md 第 7 節）。S03 的 domain 型別 ObjectState 會對應到這裡；
// 這是 presentation 自己的 view 型別，不是 domain 型別。
enum class OverlayState { Sample, Found, Inferred, Rejected };

// 框的種類：用線寬區分（Body ＝ 狀態寬度、Pin ＝ 狀態寬度 − 0.5、Window ＝ 1）。
enum class BoxKind { Body, Pin, Window };

// 使用者正在拖出的框：S＝藍色樣本框，R＝白色細線區域（淡填色），B＝子框（畫法和區域相同；ADR-025）。
enum class DraftMode { Sample, Region, Box, Match };  // Match：比對區（ADR-028，M 工具；畫法和區域相同）

inline constexpr int kOverlayStateCount = 4;
inline constexpr int kBoxKindCount = 3;

constexpr int stateIndex(OverlayState s) noexcept { return static_cast<int>(s); }
constexpr int kindIndex(BoxKind k) noexcept { return static_cast<int>(k); }

template <typename T>
using PerState = std::array<T, kOverlayStateCount>;

}  // namespace aoi::ui
