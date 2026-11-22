/*
 * Test doubles for everything Src/dshot.c expects from the rest of the firmware.
 *
 * Findings from the host-link spike: dshot.o leaves exactly 22 symbols undefined
 * (19 globals + getInputPinState, saveEEpromSettings, DroneCAN_active). Declaring
 * them via the real headers means a type mismatch would fail to compile here.
 */
#include <stdbool.h>
#include <stdint.h>

#include "IO.h"
#include "common.h"
#include "eeprom.h"
#include "am32_test_support.h"

/* ---- globals owned by other firmware modules ---- */
int16_t actual_current;
volatile char armed;
uint16_t battery_voltage;
uint8_t buffer_padding;
volatile int16_t degrees_celsius;
uint32_t dma_buffer[64];
uint16_t dshot_frametime_high;
uint16_t dshot_frametime_low;
volatile char dshot_telemetry;
uint8_t dshotcommand;
EEprom_t eepromBuffer;
char forward;
uint8_t last_dshot_command;
volatile uint16_t newinput;
char play_tone_flag;
uint8_t running;
char send_esc_info_flag;
volatile char send_telemetry;
uint16_t signaltimeout;

/* ---- functions owned by other firmware modules ---- */
int stub_save_settings_calls;
uint8_t stub_input_pin_high;

uint8_t getInputPinState(void)
{
    return stub_input_pin_high;
}

void saveEEpromSettings(void)
{
    stub_save_settings_calls++;
}

bool DroneCAN_active(void)
{
    return false;
}
