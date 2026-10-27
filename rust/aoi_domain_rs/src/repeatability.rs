//! 重複性的樣本統計（R05.4，ADR-029 的 `summarize`）：逐行對應 C++ 的 `src/domain/legacy/repeatability_legacy.cpp`。
//!
//! 一個 Box、一個指標在幾次量測裡的值：只算有限值的個數、最小、最大、平均（Welford）、Range、樣本標準差（n − 1）與
//! P/T（6σ ÷ SPEC）。運算順序和 C++ 相同（不合併成 fma），`std::min`／`std::max` 用 `cpp.rs` 的版本
//! （`std::max(0.0, NaN)` 是 0），所以兩邊的結果一位不差（`tests/domain/tst_domain_rs_diff` 逐筆比對）。
//! `valueOf`、`defaultMetrics` 是型別的存取與預設的指標表，留在 C++（`src/domain/repeatability_types.cpp`）。

use crate::cpp::{cpp_max, cpp_min};

/// `aoi::repeatability::Statistics`。
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Statistics {
    pub count: usize,
    pub expected: usize,
    pub minimum: Option<f64>,
    pub maximum: Option<f64>,
    pub average: Option<f64>,
    pub range: Option<f64>,
    pub deviation: Option<f64>,
    pub pt: Option<f64>,
}

/// `summarize(values, expected, tolerance)`：非有限值（NaN、±∞）不算；有一個值才有平均、最小、最大、Range，
/// 兩個以上才有標準差；SPEC 是有限的正數時才有 P/T。
pub fn summarize(values: &[f64], expected: usize, tolerance: Option<f64>) -> Statistics {
    let mut out = Statistics {
        expected,
        ..Statistics::default()
    };
    let mut mean = 0.0_f64;
    let mut m2 = 0.0_f64;
    for &v in values {
        if !v.is_finite() {
            continue;
        }
        out.count += 1;
        let delta = v - mean;
        mean += delta / out.count as f64;
        m2 += delta * (v - mean);
        out.minimum = Some(out.minimum.map_or(v, |m| cpp_min(m, v)));
        out.maximum = Some(out.maximum.map_or(v, |m| cpp_max(m, v)));
    }
    if out.count == 0 {
        return out;
    }
    out.average = Some(mean);
    // minimum、maximum 在 count > 0 時一定有值。
    out.range = Some(out.maximum.unwrap_or(0.0) - out.minimum.unwrap_or(0.0));
    if out.count >= 2 {
        let deviation = cpp_max(0.0, m2 / (out.count - 1) as f64).sqrt();
        out.deviation = Some(deviation);
        if let Some(t) = tolerance
            && t.is_finite()
            && t > 0.0
        {
            out.pt = Some(6.0 * deviation / t);
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn excel_formula_and_sample_denominator() {
        // tests/infra/tst_repeatability.cpp 的 excelFormulaAndSampleDenominator（同樣的數字）。
        let values = [-48.0, -41.0, -43.0, -44.0, -46.0];
        let s = summarize(&values, 5, Some(350.0));
        assert_eq!(s.count, 5);
        assert_eq!(s.range, Some(7.0));
        assert_eq!(s.average, Some(-44.4));
        assert!((s.deviation.unwrap() - 2.701851217221259).abs() < 1e-12);
        assert!((s.pt.unwrap() - 0.04631744943807873).abs() < 1e-12);
        assert_eq!(summarize(&[0.0], 1, Some(350.0)).deviation, None);
        assert_eq!(summarize(&values, 5, Some(0.0)).pt, None);
        assert_eq!(summarize(&values, 5, None).pt, None);
    }

    #[test]
    fn non_finite_values_are_skipped() {
        let s = summarize(&[f64::NAN, 1.0, f64::INFINITY, 3.0, f64::NEG_INFINITY], 5, Some(f64::NAN));
        assert_eq!(s.count, 2);
        assert_eq!(s.expected, 5);
        assert_eq!(
            (s.minimum, s.maximum, s.average, s.range),
            (Some(1.0), Some(3.0), Some(2.0), Some(2.0))
        );
        assert_eq!(s.pt, None);
        let empty = summarize(&[f64::NAN], 3, Some(200.0));
        assert_eq!(
            empty,
            Statistics {
                expected: 3,
                ..Statistics::default()
            }
        );
    }

    #[test]
    fn an_overflowing_sum_of_squares_gives_zero_deviation_like_std_max() {
        // 第二個值的 delta 溢位成 ∞：平均變成 ∞ 再變成 NaN，m2 變成 −∞ 再變成 NaN。
        // C++ 的 std::sqrt(std::max(0.0, NaN)) 是 0（std::max 回第一個引數），Range 是 ∞。
        let s = summarize(&[-1.5e308, 1.5e308, 1.5e308], 3, Some(1.0));
        assert_eq!(s.count, 3);
        assert_eq!(s.range, Some(f64::INFINITY));
        assert!(s.average.unwrap().is_nan());
        assert_eq!(s.deviation, Some(0.0));
        assert_eq!(s.pt, Some(0.0));
    }
}
