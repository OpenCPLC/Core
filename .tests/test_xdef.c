// .tests/test_xdef.c

// Math of `xdef.h` at the limits of its types:
// `absv` saturates the minimum of every signed type instead of overflowing,
// `clamp` and `in_range` hold their bounds,
// and `minv`, `maxv` and `clamp` evaluate each argument once.

#include "test.h"
#include "xdef.h"

static uint32_t calls;

// Argument with a side effect: counts its evaluations
static int counted(int value)
{
  calls++;
  return value;
}

static void absv_saturates_the_minimum(void)
{
  TEST_Equal(absv((signed char)SCHAR_MIN), SCHAR_MAX, "signed char");
  TEST_Equal(absv((short)SHRT_MIN), SHRT_MAX, "short");
  TEST_Equal(absv(INT_MIN), INT_MAX, "int");
  TEST_Equal(absv(LONG_MIN), LONG_MAX, "long");
  TEST_Equal(absv(LLONG_MIN), LLONG_MAX, "long long");
  TEST_Equal(absv(-5), 5, "negative");
  TEST_Equal(absv(UINT_MAX), UINT_MAX, "unsigned passes through");
  TEST_Equal(absv(-2.5f), 2.5f, "float");
}

static void clamp_and_in_range_hold_their_bounds(void)
{
  TEST_Equal(clamp(INT_MAX, INT_MIN, 0), 0, "above");
  TEST_Equal(clamp(INT_MIN, -1, 1), -1, "below");
  TEST_Equal(clamp(7, 7, 7), 7, "one-value range");
  TEST_Equal(clamp(-1.5f, 0.0f, 1.0f), 0.0f, "float");
  TEST_Check(in_range(INT_MAX, INT_MIN, INT_MAX), "bounds inclusive");
  TEST_Check(!in_range(-1, 0, INT_MAX), "below the range");
}

static void each_argument_runs_once(void)
{
  calls = 0;
  TEST_Equal(minv(counted(3), counted(1), counted(2)), 1, "least of three");
  TEST_Equal(maxv(counted(3), counted(1), counted(2), counted(9), counted(4), counted(0)), 9,
    "greatest of six");
  TEST_Equal(clamp(counted(5), counted(0), counted(9)), 5, "clamp");
  TEST_Equal(calls, 12, "one evaluation per argument");
}

static void sign_of_takes_three_values(void)
{
  TEST_Check(sign_of(INT_MIN) == -1 && sign_of(0) == 0 && sign_of(INT_MAX) == 1, "int");
  TEST_Check(sign_of(-0.5f) == -1 && sign_of(0.0f) == 0, "float");
}

void TEST_Cases(void)
{
  TEST(absv_saturates_the_minimum);
  TEST(clamp_and_in_range_hold_their_bounds);
  TEST(each_argument_runs_once);
  TEST(sign_of_takes_three_values);
}
