//! C++ 標準庫與 MSVC 的行為（R05.2 從 `nms.rs` 搬出來共用）。
//!
//! Rust 的 `f64::min`／`max`／`clamp` 遇到 NaN 的結果和 `std::min`／`std::max`／`std::clamp` 不同，
//! `as i32` 是飽和轉換、MSVC 的 `static_cast<int>` 不是；C++ 的 `std::sort` 只要一個「小於」。
//! 這些小函式讓 Rust 的實作和 C++（`aoi::legacy`）一步一步相同，差異測試才能要求結果一位不差。

use std::cmp::Ordering;

/// `std::min(a, b)`：`(b < a) ? b : a`。
pub fn cpp_min(a: f64, b: f64) -> f64 {
    if b < a { b } else { a }
}

/// `std::max(a, b)`：`(a < b) ? b : a`。
pub fn cpp_max(a: f64, b: f64) -> f64 {
    if a < b { b } else { a }
}

/// `std::min({a, b, c})`（`min_element`：比自己小才換）。
pub fn cpp_min3(a: f64, b: f64, c: f64) -> f64 {
    let mut m = a;
    if b < m {
        m = b;
    }
    if c < m {
        m = c;
    }
    m
}

/// `std::clamp(v, lo, hi)`：`v < lo ? lo : (hi < v ? hi : v)`；NaN 原樣回傳。
pub fn cpp_clamp(v: f64, lo: f64, hi: f64) -> f64 {
    if v < lo {
        lo
    } else if hi < v {
        hi
    } else {
        v
    }
}

/// MSVC 的 `static_cast<int>(double)`（cvttsd2si）：NaN 或超出 int 範圍時得到 `INT_MIN`。
/// Rust 的 `as i32` 是飽和轉換，所以超出範圍時另外處理，結果才和 C++ 相同。
pub fn msvc_i32(v: f64) -> i32 {
    if v.is_nan() || v >= 2_147_483_648.0 || v < -2_147_483_648.0 {
        i32::MIN
    } else {
        v as i32
    }
}

/// MSVC（UCRT）的 `std::lround`（R05.3）：四捨五入、.5 遠離 0（和 `f64::round` 相同），結果超出 `long`（Windows 是 32 bit）
/// 或是 NaN 時得到 **0**（實測：NaN、±∞、±1e20、2147483647.5、−2147483648.5 都是 0；和 `static_cast<int>` 的 `INT_MIN` 不同）。
pub fn msvc_lround(v: f64) -> i32 {
    let r = v.round();
    if r.is_nan() || r >= 2_147_483_648.0 || r < -2_147_483_648.0 {
        0
    } else {
        r as i32
    }
}

/// C++ 的比較函式（「小於」）換成 Rust 的 `Ordering`：`less(a, b)` 是 Less、`less(b, a)` 是 Greater、都不是就 Equal。
/// 用在排序：C++ 的比較函式只要嚴格弱序，排序的鍵都不同時，任何排序演算法的結果都相同。
pub fn by_less<T>(less: impl Fn(&T, &T) -> bool) -> impl Fn(&T, &T) -> Ordering {
    move |a, b| {
        if less(a, b) {
            Ordering::Less
        } else if less(b, a) {
            Ordering::Greater
        } else {
            Ordering::Equal
        }
    }
}

/// `std::pair<double, U>` 的 `operator<`：先比 first（`<`），相等（含 −0 與 +0）再比 second。
pub fn pair_less<U: PartialOrd>(a: &(f64, U), b: &(f64, U)) -> bool {
    if a.0 < b.0 {
        return true;
    }
    if b.0 < a.0 {
        return false;
    }
    a.1 < b.1
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nan_and_conversion_follow_cpp() {
        assert!(cpp_min3(f64::NAN, 1.0, 2.0).is_nan());
        assert!(cpp_clamp(f64::NAN, 0.0, 1.0).is_nan());
        assert_eq!(cpp_max(0.0, f64::NAN), 0.0);
        assert!(cpp_max(f64::NAN, 0.0).is_nan());
        assert_eq!(msvc_i32(3.0e9), i32::MIN);
        assert_eq!(msvc_i32(f64::NAN), i32::MIN);
        assert_eq!(msvc_i32(-7.9), -7);
    }

    #[test]
    fn lround_follows_msvc() {
        // R05.3 用 MSVC 19.44（/O2）實際印出來的值。
        assert_eq!(msvc_lround(2.5), 3);
        assert_eq!(msvc_lround(-2.5), -3);
        assert_eq!(msvc_lround(0.499_999_999_999_999_94), 0);
        assert_eq!(msvc_lround(2_147_483_647.4), i32::MAX);
        assert_eq!(msvc_lround(-2_147_483_648.4), i32::MIN);
        for v in [
            f64::NAN,
            f64::INFINITY,
            f64::NEG_INFINITY,
            1e20,
            -1e20,
            2_147_483_647.5,
            -2_147_483_648.5,
            -2_147_483_649.0,
        ] {
            assert_eq!(msvc_lround(v), 0, "{v}");
        }
    }

    #[test]
    fn pair_less_treats_signed_zeros_as_equal() {
        assert!(pair_less(&(-0.0, 1u32), &(0.0, 2u32)));
        assert!(!pair_less(&(0.0, 2u32), &(-0.0, 1u32)));
    }
}
