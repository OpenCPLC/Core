// .tests/test_ary.c
// Fixed array: a shift takes the first element out, a push lands behind the last

#include "test.h"
#include "ary.c"

static void shift_and_push_keep_order(void)
{
  ary_init(values, uint8_t, 3, { 1, 2, 3 });
  TEST_Check(values._head == 0);
  uint8_t shifted = 0;
  TEST_Check(ary_shift(&values, &shifted));
  TEST_Equal(shifted, 1, "first element shifted out");
  uint8_t next = 9;
  TEST_Check(ary_push(&values, &next));
  TEST_Check(values._head < values.limit);
  uint8_t *second = ary_get(&values, 1);
  uint8_t *third = ary_get(&values, 2);
  TEST_Check(second && *second == 3);
  TEST_Check(third && *third == 9);
}

void TEST_Cases(void)
{
  TEST(shift_and_push_keep_order);
}
