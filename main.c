#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/adc.h"
#include "ws2812.pio.h"

#include "tusb.h"
#include "bsp/board.h"

#include "usb_descriptors.h"

const int NUM_BUTTONS = 4;
const int SENSOR_PADDING = 2;
const int PIN_TX = 16;

const int FIRST_PIN = 26;

typedef uint8_t force_t;
typedef uint8_t buttons_t;

static force_t sensors[4] = { 1, 2, 3, 4 };
static force_t thresholds[4] = { 5, 6, 7, 8 };
static uint8_t prev_buttons = 0; 

static PIO pio;

static inline void put_pixel(uint32_t pixel_grb) {
  pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r) << 8) |
         ((uint32_t)(g) << 16) |
         (uint32_t)(b);
}

void hid_task(void);
void tud_task(void);

int main(void)
{
    stdio_init_all();
    read_thresholds();

    uint32_t const initial_millis = board_millis();

    // Enabling PWM reduces ADC noise.
    int const PWM_PIN = 23;
    gpio_init(PWM_PIN);
    gpio_set_dir(PWM_PIN, GPIO_OUT);
    gpio_put(PWM_PIN, 1);

    pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    char str[12];

    ws2812_program_init(pio, sm, offset, PIN_TX, 800000, true);

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    adc_init();
    adc_set_round_robin(0xF);


    for(unsigned i = 0; i < 4; ++i)
        adc_gpio_init(FIRST_PIN + i);

    while(true)
    {
        if(board_button_read())
            put_pixel(urgb_u32(0x10, 0x10, 0x10));
        else
            put_pixel(urgb_u32(0x0, 0x0, 0x0));

        tud_task();
        hid_task();
    }
}

#define FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_ADDR ((uint8_t*)(XIP_BASE + FLASH_OFFSET))

int find_flash_offset(void)
{
    for(int i = 0; i < FLASH_SECTOR_SIZE; i += sizeof(thresholds))
    {
        for(unsigned j = 0; j < sizeof(thresholds); ++j)
            if(FLASH_ADDR[i + j] != 0xFF)
                goto next;
        return i;
    next:;
    }

    return FLASH_SECTOR_SIZE;
}

void read_thresholds(void)
{
    int const offset = find_flash_offset();
    if(offset > 0)
        memcpy(thresholds, FLASH_ADDR + offset - sizeof(thresholds), sizeof(thresholds));
    else
        memset(thresholds, 127, sizeof(thresholds));
}

void save_thresholds(void)
{
    unsigned const offset = find_flash_offset() % FLASH_SECTOR_SIZE;
    unsigned const page_offset = offset & ~(FLASH_PAGE_SIZE-1);
    uint8_t page[FLASH_PAGE_SIZE];

    memset(page , 0xFF, sizeof(page));
    memcpy(page + (offset % FLASH_PAGE_SIZE), thresholds, sizeof(thresholds));

    uint32_t const ints = save_and_disable_interrupts();
    if(offset == 0)
        flash_range_erase(FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_OFFSET + page_offset, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void poll_sensors(void)
{
    adc_select_input(0);

    force_t new_reading[4];
    for(int i = 0; i < NUM_BUTTONS; ++i)
        new_reading[i] = ~(adc_read() >> 4);

    for(int i = 0; i < NUM_BUTTONS; ++i)
        sensors[i] = ((sensors[i] * 3) + new_reading[i]) / 4;
}

buttons_t read_buttons(void)
{
    buttons_t buttons = prev_buttons;

    for(int i = 0; i < NUM_BUTTONS; ++i)
    {
        buttons_t const button = 1 << i;

        if(sensors[i] < thresholds[i] - SENSOR_PADDING)
            buttons &= ~button;
        else if(sensors[i] >= thresholds[i] + SENSOR_PADDING)
            buttons |= button;
    }

    return buttons;
}

// Every ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
    if(!tud_hid_ready())
        return;

    static absolute_time_t prev_time = 0;
    if(!prev_time)
        prev_time = get_absolute_time();
    absolute_time_t const time = get_absolute_time();
    int64_t const time_diff = absolute_time_diff_us(prev_time, time);
    if(time_diff >= 250)
    {
        poll_sensors();
        prev_time = time;
    }

    static uint32_t prev_millis = 0;
    uint32_t const millis = board_millis();

    if(prev_millis == millis) 
        return;
    prev_millis = millis;

    uint8_t const buttons = read_buttons();

    if(prev_buttons == buttons)
        return;
    prev_buttons = buttons;

    tud_hid_report(REPORT_ID_BUTTONS, &buttons, sizeof(buttons));
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    if(report_type != HID_REPORT_TYPE_FEATURE)
        return 0;

    if(report_id == REPORT_ID_SENSORS && reqlen >= sizeof(sensors))
    {
        memcpy(buffer, &sensors, sizeof(sensors));
        return sizeof(sensors);
    }
    else if(report_id == REPORT_ID_THRESHOLDS && reqlen >= sizeof(thresholds))
    {
        memcpy(buffer, &thresholds, sizeof(thresholds));
        return sizeof(thresholds);
    }

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    if(report_type != HID_REPORT_TYPE_FEATURE)
        return;

    if(report_id == REPORT_ID_THRESHOLDS)
    {
        if(bufsize >= sizeof(thresholds))
        {
            int const cmp = memcmp(&thresholds, buffer, sizeof(thresholds));
            memcpy(&thresholds, buffer, sizeof(thresholds));
            if(cmp != 0)
                save_thresholds();
        }
    }
}
