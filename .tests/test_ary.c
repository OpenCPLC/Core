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

ary_new_static(kept, uint16_t, 2);
ary_ring_static(latest, uint8_t, 2);
ary_init_static(seeded, uint8_t, 4, { 7, 8 });
ary_const_static(primes, uint8_t, { 2, 3, 5 });

// The `_static` twins declare what the plain macros do, private to this file
static void static_twins_hold_what_plain_ones_hold(void)
{
  uint16_t value = 300;
  TEST_Check(ary_push(&kept, &value) && ary_push(&kept, &value) && !ary_push(&kept, &value));
  uint8_t byte = 1;
  for(uint8_t i = 0; i < 3; i++, byte++) TEST_Check(ary_push(&latest, &byte));
  uint8_t *oldest = ary_get(&latest, 0);
  TEST_Check(oldest && *oldest == 2, "ring dropped the oldest");
  TEST_Equal(seeded.count, 2, "initial values counted");
  uint8_t *last = ary_get(&primes, 2);
  TEST_Check(primes.count == 3 && last && *last == 5);
}

void TEST_Cases(void)
{
  TEST(shift_and_push_keep_order);
  TEST(static_twins_hold_what_plain_ones_hold);
}
