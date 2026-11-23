/*
 * Characterization tests for Src/functions.c (pure helpers only).
 *
 * "Characterization" = these tests pin down what the code does TODAY, including
 * quirks, so later refactoring can be proven behaviour-preserving.
 */
#include "unity.h"
#include "functions.h"

TEST_SOURCE_FILE("stub_sitl_timer.c")

/* Defined in functions.c but not declared in functions.h. */
uint8_t update_crc8(uint8_t crc, uint8_t crc_seed);
uint8_t get_crc8(uint8_t* Buf, uint8_t BufLen);

void setUp(void) {}
void tearDown(void) {}

static void check_map(long expected, long x, long in_min, long in_max, long out_min, long out_max)
{
    TEST_ASSERT_EQUAL_INT((int)expected, (int)map(x, in_min, in_max, out_min, out_max));
}

/* ---- map() ---------------------------------------------------------------- */

void test_map_clamps_to_output_range_at_and_beyond_the_input_ends(void)
{
    check_map(0, 0, 0, 2000, 0, 100);
    check_map(100, 2000, 0, 2000, 0, 100);
    check_map(0, -5, 0, 2000, 0, 100);
    check_map(100, 2500, 0, 2000, 0, 100);
}

void test_map_linear_points_are_exact_for_a_power_of_two_friendly_range(void)
{
    check_map(25, 500, 0, 2000, 0, 100);
    check_map(50, 1000, 0, 2000, 0, 100);
    check_map(75, 1500, 0, 2000, 0, 100);
}

void test_map_rounds_down_next_to_the_ends(void)
{
    check_map(0, 1, 0, 2000, 0, 100);
    check_map(99, 1999, 0, 2000, 0, 100);
}

void test_map_is_a_bisection_approximation_of_the_linear_mapping(void)
{
    /* The input range of computeMSInput() (243..1200 -> 0..2000). Exactly linear
     * would give 119, 746 and 1373; the recursive bisection is a few units off. */
    check_map(117, 300, 243, 1200, 0, 2000);
    check_map(742, 600, 243, 1200, 0, 2000);
    check_map(1371, 900, 243, 1200, 0, 2000);
}

void test_map_is_monotonic_and_stays_inside_the_output_range(void)
{
    long previous = map(0, 0, 2000, 0, 100);
    for (long x = 1; x <= 2000; x++) {
        long current = map(x, 0, 2000, 0, 100);
        TEST_ASSERT_TRUE_MESSAGE(current >= previous, "map() must never decrease");
        TEST_ASSERT_TRUE(current >= 0 && current <= 100);
        previous = current;
    }
}

void test_map_with_a_constant_output_range_returns_that_value(void)
{
    check_map(50, 5, 0, 10, 50, 50);
}

void test_map_supports_an_inverted_output_range(void)
{
    check_map(50, 1000, 0, 2000, 100, 0);
}

void test_map_with_reversed_input_range_returns_out_max_because_the_swap_branch_is_unreachable(void)
{
    /* QUIRK: for in_min > in_max the clamp `x >= in_max` fires first, so the
     * "swap the ranges" branch further down can never be reached. */
    check_map(100, 7, 10, 0, 0, 100);
}

/* ---- getAbsDif() ---------------------------------------------------------- */

void test_getAbsDif_is_symmetric(void)
{
    TEST_ASSERT_EQUAL_UINT32(7, getAbsDif(3, 10));
    TEST_ASSERT_EQUAL_UINT32(7, getAbsDif(10, 3));
}

void test_getAbsDif_handles_negative_numbers_and_zero_difference(void)
{
    TEST_ASSERT_EQUAL_UINT32(14, getAbsDif(-7, 7));
    TEST_ASSERT_EQUAL_UINT32(0, getAbsDif(5, 5));
}

void test_getAbsDif_handles_the_largest_int_magnitudes(void)
{
    TEST_ASSERT_EQUAL_UINT32(2147483647, getAbsDif(0, 2147483647));
    TEST_ASSERT_EQUAL_UINT32(2147483647, getAbsDif(-2147483647, 0));
}

/* ---- CRC-8 (polynomial 0x07, initial value 0) ------------------------------ */

void test_get_crc8_matches_the_standard_check_vector(void)
{
    uint8_t check[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    TEST_ASSERT_EQUAL_HEX8(0xF4, get_crc8(check, sizeof check));
}

void test_get_crc8_of_an_empty_buffer_is_zero(void)
{
    uint8_t unused[1] = { 0 };
    TEST_ASSERT_EQUAL_HEX8(0x00, get_crc8(unused, 0));
}

void test_get_crc8_known_values(void)
{
    uint8_t zero[] = { 0x00 };
    uint8_t ascending[] = { 0x01, 0x02, 0x03 };
    TEST_ASSERT_EQUAL_HEX8(0x00, get_crc8(zero, 1));
    TEST_ASSERT_EQUAL_HEX8(0x48, get_crc8(ascending, 3));
}

void test_update_crc8_single_step(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x97, update_crc8(0x31, 0x00));
}
