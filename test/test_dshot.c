/*
 * Characterization tests for Src/dshot.c, linked UNCHANGED on the host.
 *
 * The firmware's globals and hardware calls are replaced by test doubles from
 * test/support/stub_firmware_state.c. A DShot frame is fed in the way the DMA
 * capture delivers it: 32 edge timestamps, long pulse = 1, short pulse = 0.
 */
#include <string.h>

#include "unity.h"
#include "dshot.h"
#include "common.h"
#include "eeprom.h"
#include "am32_test_support.h"

TEST_SOURCE_FILE("stub_firmware_state.c")

/* Globals defined in dshot.c that are not exposed by dshot.h. */
extern char EDT_ARMED;
extern char EDT_ARM_ENABLE;
extern char send_EDT_init;
extern char send_EDT_deinit;
extern uint16_t dshot_goodcounts;
extern uint16_t dshot_badcounts;
extern uint8_t command_count;
extern uint8_t high_pin_count;
extern uint8_t programming_mode;
extern uint16_t position;
extern uint8_t new_byte;
extern uint8_t dshot_extended_telemetry;
extern volatile uint32_t gcrnumber;
extern int dshot_full_number;
extern uint32_t gcr[37];

/* Declared locally inside dshot.c, owned by other modules (see stub_firmware_state.c). */
extern volatile char send_telemetry;
extern char send_esc_info_flag;

#define FRAMES_FOR_A_COMMAND 6

void setUp(void)
{
    memset(&eepromBuffer, 0, sizeof eepromBuffer);
    memset(dma_buffer, 0, sizeof dma_buffer);
    newinput = 0;
    dshotcommand = 0;
    command_count = 0;
    last_dshot_command = 0;
    armed = 1;
    running = 0;
    forward = 0;
    EDT_ARMED = 1;       /* main.c sets this for AUTO and DSHOT input types */
    EDT_ARM_ENABLE = 0;
    dshot_goodcounts = 0;
    dshot_badcounts = 0;
    dshot_telemetry = 0;
    dshot_extended_telemetry = 0;
    send_EDT_init = 0;
    send_EDT_deinit = 0;
    high_pin_count = 0;
    programming_mode = 0;
    play_tone_flag = 0;
    send_esc_info_flag = 0;
    send_telemetry = 0;
    signaltimeout = 0;
    buffer_padding = 0;
    dshot_frametime_low = 900;
    dshot_frametime_high = 1100;
    stub_save_settings_calls = 0;
    stub_input_pin_high = 0;
}

void tearDown(void) {}

/* Build the capture timestamps for one 16-bit frame: 11 bit value, telemetry
 * request bit, 4 bit CRC (xor of the three 4-bit groups). */
static void load_frame(unsigned value, unsigned telemetry_request, int corrupt_crc)
{
    unsigned data12 = (value << 1) | telemetry_request;
    unsigned crc = ((data12 >> 8) ^ (data12 >> 4) ^ data12) & 0xF;
    if (corrupt_crc) {
        crc ^= 0x1;
    }
    unsigned frame = (data12 << 4) | crc;
    for (int i = 0; i < 16; i++) {
        unsigned bit = (frame >> (15 - i)) & 1;
        dma_buffer[2 * i] = 64 * i;
        dma_buffer[2 * i + 1] = 64 * i + (bit ? 48 : 16);
    }
}

static void send_frames(unsigned value, int count)
{
    load_frame(value, 0, 0);
    for (int i = 0; i < count; i++) {
        computeDshotDMA();
    }
}

/* ---- decoding ------------------------------------------------------------- */

void test_a_valid_throttle_frame_sets_the_new_input(void)
{
    send_frames(1000, 1);
    TEST_ASSERT_EQUAL_UINT16(1000, newinput);
    TEST_ASSERT_EQUAL_UINT16(1, dshot_goodcounts);
    TEST_ASSERT_EQUAL_UINT16(0, dshot_badcounts);
}

void test_a_frame_with_a_bad_crc_is_counted_and_ignored(void)
{
    load_frame(1000, 0, 1);
    computeDshotDMA();
    TEST_ASSERT_EQUAL_UINT16(0, newinput);
    TEST_ASSERT_EQUAL_UINT16(1, dshot_badcounts);
    TEST_ASSERT_EQUAL_UINT16(0, dshot_goodcounts);
}

void test_a_frame_outside_the_expected_period_is_dropped_silently(void)
{
    dshot_frametime_low = 2000;
    dshot_frametime_high = 3000;
    send_frames(1000, 1);
    TEST_ASSERT_EQUAL_UINT16(0, newinput);
    TEST_ASSERT_EQUAL_UINT16(0, dshot_goodcounts);
    TEST_ASSERT_EQUAL_UINT16(0, dshot_badcounts);
}

void test_the_telemetry_request_bit_sets_send_telemetry(void)
{
    load_frame(1000, 1, 0);
    computeDshotDMA();
    TEST_ASSERT_EQUAL_INT(1, send_telemetry);
}

void test_values_from_48_upwards_are_throttle_and_47_is_a_command(void)
{
    send_frames(48, 1);
    TEST_ASSERT_EQUAL_UINT16(48, newinput);

    setUp();
    send_frames(47, 1);
    TEST_ASSERT_EQUAL_UINT16(0, newinput);
    TEST_ASSERT_EQUAL_UINT8(47, dshotcommand);
}

void test_throttle_is_ignored_while_the_edt_arm_state_is_off(void)
{
    EDT_ARMED = 0;     /* what main.c does for the EDTARM_IN input type */
    send_frames(1000, 1);
    TEST_ASSERT_EQUAL_UINT16(0, newinput);
}

void test_a_zero_value_clears_the_throttle_input(void)
{
    newinput = 500;
    send_frames(0, 1);
    TEST_ASSERT_EQUAL_UINT16(0, newinput);
}

/* ---- commands ------------------------------------------------------------- */

void test_beacon_command_1_fires_on_the_first_frame(void)
{
    send_frames(1, 1);
    TEST_ASSERT_EQUAL_INT(1, play_tone_flag);
}

void test_other_commands_need_six_identical_frames(void)
{
    send_frames(8, FRAMES_FOR_A_COMMAND - 1);   /* command 8: reverse direction */
    TEST_ASSERT_EQUAL_UINT8(0, eepromBuffer.dir_reversed);

    computeDshotDMA();
    TEST_ASSERT_EQUAL_UINT8(1, eepromBuffer.dir_reversed);
    TEST_ASSERT_EQUAL_INT(0, forward);
}

void test_commands_are_ignored_while_the_motor_is_running(void)
{
    running = 1;
    send_frames(8, FRAMES_FOR_A_COMMAND);
    TEST_ASSERT_EQUAL_UINT8(0, eepromBuffer.dir_reversed);
}

void test_commands_are_ignored_while_disarmed(void)
{
    armed = 0;
    send_frames(8, FRAMES_FOR_A_COMMAND);
    TEST_ASSERT_EQUAL_UINT8(0, eepromBuffer.dir_reversed);
}

void test_the_save_settings_command_calls_the_eeprom_writer_once(void)
{
    send_frames(12, FRAMES_FOR_A_COMMAND);
    TEST_ASSERT_EQUAL_INT(1, stub_save_settings_calls);
    TEST_ASSERT_EQUAL_INT(1, play_tone_flag);
}

void test_programming_mode_writes_one_eeprom_byte(void)
{
    send_frames(36, FRAMES_FOR_A_COMMAND);      /* enter programming mode */
    TEST_ASSERT_EQUAL_UINT8(1, programming_mode);

    send_frames(5, 1);                           /* eeprom position */
    TEST_ASSERT_EQUAL_UINT8(2, programming_mode);
    TEST_ASSERT_EQUAL_UINT16(5, position);

    send_frames(99, 1);                          /* new value */
    TEST_ASSERT_EQUAL_UINT8(3, programming_mode);

    send_frames(37, 1);                          /* commit */
    TEST_ASSERT_EQUAL_UINT8(0, programming_mode);
    TEST_ASSERT_EQUAL_UINT8(99, eepromBuffer.buffer[5]);
}

/* ---- signal polarity detection ------------------------------------------------ */

void test_an_inverted_signal_is_detected_after_more_than_100_high_idle_checks(void)
{
    armed = 0;
    stub_input_pin_high = 1;
    send_frames(0, 100);
    TEST_ASSERT_EQUAL_INT(0, dshot_telemetry);

    computeDshotDMA();
    TEST_ASSERT_EQUAL_INT(1, dshot_telemetry);
}

/* ---- telemetry frame encoding --------------------------------------------------- */

static void check_encoding(uint16_t com_time, int expected_full, uint32_t expected_gcr)
{
    running = 1;
    make_dshot_package(com_time);
    TEST_ASSERT_EQUAL_HEX16((uint16_t)expected_full, (uint16_t)dshot_full_number);
    TEST_ASSERT_EQUAL_HEX32(expected_gcr, (uint32_t)gcrnumber);
}

void test_make_dshot_package_encodes_the_commutation_time(void)
{
    check_encoding(1000, 0x3F47, 0x9BFB7);   /* needs the shift/exponent form */
    check_encoding(291, 0x123F, 0xDCA6F);    /* fits without shifting */
    check_encoding(300, 0x12C0, 0xDCBD9);
}

void test_make_dshot_package_reports_the_maximum_time_while_the_motor_is_stopped(void)
{
    running = 0;
    make_dshot_package(1000);
    TEST_ASSERT_EQUAL_HEX16(0xFFF0, (uint16_t)dshot_full_number);
}
