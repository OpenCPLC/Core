// .tests/test_xmath.c
// Integer means: `iqr_mean_u32` leaves samples beyond 1.5 × IQR out,
// keeps a plain mean under four samples and scales before it divides.
// Interpolation holds the end values past the table, rising or falling.

#include <math.h>

#include "test.h"
#include "xmath.c"

static void iqr_mean_drops_outliers(void)
{
  uint32_t data[] = { 100, 101, 99, 100, 102, 98, 100, 5000 };
  TEST_Equal(iqr_mean_u32(data, 8, 1), 100, "spike of 5000 left out of the mean");
}

// The bound lies 1.5 × IQR past the quartiles: a sample on it counts, one past it does not
static void iqr_mean_bound_is_one_and_a_half_iqr(void)
{
  uint32_t on[] = { 100, 100, 100, 100, 104, 104, 104, 110 };
  TEST_Equal(iqr_mean_u32(on, 8, 1), 103, "sample on the bound in, 822 / 8 rounded");
  uint32_t past[] = { 100, 100, 100, 100, 104, 104, 104, 111 };
  TEST_Equal(iqr_mean_u32(past, 8, 1), 102, "sample past the bound out, 712 / 7 rounded");
}

static void iqr_mean_keeps_flat_core(void)
{
  uint32_t data[] = { 7, 7, 7, 7, 100 };
  TEST_Equal(iqr_mean_u32(data, 5, 1), 7, "zero spread keeps the equal samples alone");
}

static void iqr_mean_under_four_is_plain(void)
{
  uint32_t data[] = { 1, 2, 2 };
  TEST_Equal(iqr_mean_u32(data, 3, 1), 2, "plain mean of 5 / 3, rounded");
}

static void iqr_mean_scales_before_division(void)
{
  uint32_t data[] = { 1, 2 };
  TEST_Equal(iqr_mean_u32(data, 2, 65536), 98304, "mean 1.5 in Q16");
}

static void iqr_mean_of_nothing(void)
{
  TEST_Equal(iqr_mean_u32(NULL, 0, 1), 0, "no samples, zero");
}

static void interp_holds_the_ends(void)
{
  const float xs[] = { 1.0f, 2.0f, 4.0f };
  const float ys[] = { 10.0f, 20.0f, 40.0f };
  TEST_Near(interp_f32(3.0f, xs, ys, 3), 30.0f, 1e-4, "between points");
  TEST_Near(interp_f32(0.5f, xs, ys, 3), 10.0f, 1e-4, "below the table, first value");
  TEST_Near(interp_f32(9.0f, xs, ys, 3), 40.0f, 1e-4, "above the table, last value");
  TEST_Near(interp_f32(2.0f, xs, ys, 3), 20.0f, 1e-4, "on a point");
  TEST_Near(interp_f32(2.0f, xs, ys, 1), 10.0f, 1e-4, "one point, its value");
  TEST_Check(isnan(interp_f32(NAN, xs, ys, 3)), "no input, no value");
  TEST_Check(isnan(interp_f32(1.0f, xs, ys, 0)), "no table, no value");
}

static void interp_takes_a_falling_table(void)
{
  const float xs[] = { 4.0f, 2.0f, 1.0f };
  const float ys[] = { 40.0f, 20.0f, 10.0f };
  TEST_Near(interp_f32(3.0f, xs, ys, 3), 30.0f, 1e-4, "between points");
  TEST_Near(interp_f32(5.0f, xs, ys, 3), 40.0f, 1e-4, "past the first point");
  TEST_Near(interp_f32(0.0f, xs, ys, 3), 10.0f, 1e-4, "past the last point");
}

void TEST_Cases(void)
{
  TEST(iqr_mean_drops_outliers);
  TEST(iqr_mean_bound_is_one_and_a_half_iqr);
  TEST(iqr_mean_keeps_flat_core);
  TEST(iqr_mean_under_four_is_plain);
  TEST(iqr_mean_scales_before_division);
  TEST(iqr_mean_of_nothing);
  TEST(interp_holds_the_ends);
  TEST(interp_takes_a_falling_table);
}
