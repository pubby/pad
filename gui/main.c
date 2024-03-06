// Copyright 2024, Patrick Bene

// STD
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// libusb
#include <hidapi.h>

// Curses (put last)
#ifdef _WIN32
#define PDC_WIDE
#include <pdcurses.h>
#else
#include <ncurses.h>
#endif

enum
{
  REPORT_ID_BUTTONS = 1,
  REPORT_ID_FEATURES,
  REPORT_ID_COUNT
};

enum
{
    CP_ERROR = 1,
    CP_BAR_PRE,
    CP_BAR_POST,
};

struct hid_device_info* enumeration = NULL;
struct hid_device_info* entry = NULL;
hid_device* device = NULL;

uint8_t sensors[4] = {};
uint8_t thresholds[4] = {};
int ui_line = 0;

static char io_buf[64] = {};
static char device_name[64] = {};

void read_sensors(void)
{
    if(!device)
        return;
    io_buf[0] = REPORT_ID_FEATURES; // Report number
    if(hid_get_feature_report(device, io_buf, sizeof(io_buf)))
        memcpy(sensors, io_buf+5, sizeof(sensors));
}

void read_thresholds(void)
{
    if(!device)
        return;
    io_buf[0] = REPORT_ID_FEATURES; // Report number
    if(hid_get_feature_report(device, io_buf, sizeof(io_buf)))
        memcpy(thresholds, io_buf+1, sizeof(thresholds));
}

void write_thresholds(void)
{
    if(!device)
        return;
    io_buf[0] = REPORT_ID_FEATURES; // Report number
    memcpy(io_buf+1, thresholds, sizeof(thresholds));
    hid_send_feature_report(device, io_buf, sizeof(thresholds) + 1);
}

void enumerate(void)
{
    if(enumeration)
        hid_free_enumeration(enumeration);
    enumeration = entry = hid_enumerate(0x16C0, 0x27D9);
}

void find_next_entry(void)
{
    bool retried = false;
    while(entry)
    {
        if(!(entry = entry->next))
        {
            if(retried)
                return;
            enumerate();
            retried = true;
        }
        else if(entry->usage == 0xA0 && wcscmp(entry->manufacturer_string, L"http://pubby.games") == 0)
        {
            if(device)
                hid_close(device);
            device = hid_open_path(entry->path);
            strncpy(device_name, entry->path, sizeof(device_name));
            device_name[sizeof(device_name)-1] = '\0';
            return;
        }
    }
}

void poll_mode(bool on)
{
    if(on)
    {
        raw();        // Disable line buffering
        noecho();     // Don't display key presses
        halfdelay(1); // Poll input every 1/10 a second
        curs_set(false);
    }
    else
    {
        noraw();
        echo();
        nocbreak();
        curs_set(true);
    }
}

int main(void)
{

    // Init ncurses:
    initscr();
    keypad(stdscr, true); // Enable more keypresses

    // Colors
    start_color();
    init_pair(CP_ERROR, COLOR_RED, COLOR_BLACK);
    init_pair(CP_BAR_PRE, COLOR_MAGENTA, COLOR_MAGENTA);
    init_pair(CP_BAR_POST, COLOR_CYAN, COLOR_BLACK);

    clear();

    // Init HID:
    if(hid_init() != 0)
    {
        fwprintf(stderr, L"HID Error: %ls\n", hid_error(NULL));
        return EXIT_FAILURE;
    }

    enumerate();
    find_next_entry();
    read_thresholds();
    read_sensors();

    while(true)
    {
        poll_mode(true);
        read_sensors();

        int line = 0;
        mvprintw(line++, 0, "Pad Sensor Thresholds: %s", device_name);
        mvprintw(line++, 0, "[Tab]: Toggle device  [Enter]: Set Value  [c]: Calibrate");
        mvprintw(line++, 0, "[s]: Save Profile     [l]: Load Profile   [q]: Quit");
        line++;

        for(int i = 0; i < 4; ++i)
        {
            if(i == ui_line)
                attron(A_REVERSE);
            mvprintw(line++, 2, "Button %i: %3i", i, thresholds[i]);
            attroff(A_REVERSE);

            mvprintw(line++, 6, "%3i ", sensors[i]);
            attron(COLOR_PAIR(CP_BAR_PRE));
            for(int j = 0; j < (sensors[i] >> 2); ++j)
                addch('#');
            attroff(COLOR_PAIR(CP_BAR_PRE));
            attron(COLOR_PAIR(CP_BAR_POST));
            for(int j = sensors[i] >> 2; j < 64; ++j)
                addch('-');
            attroff(COLOR_PAIR(CP_BAR_POST));

            move(line, 10);
            clrtoeol();
            mvaddch(line++, 10 + (thresholds[i] >> 2), '^');
        }

        move(line++, 0);
        if(!device)
        {
            attron(COLOR_PAIR(CP_ERROR));
            printw("Error: Unable to access USB device.\n");
            attroff(COLOR_PAIR(CP_ERROR));
        }
        clrtoeol();

        refresh();

        switch(getch())
        {
        case KEY_EXIT:
        case 'q':
        case 'Q':
            write_thresholds();
            goto exit;

        case 'c':
            thresholds[ui_line] = sensors[ui_line];
            break;

        case 'C':
            for(int i = 0; i < 4; ++i)
                thresholds[i] = sensors[i];
            break;

        case KEY_DOWN:
            ++ui_line;
            ui_line &= 3;
            break;

        case KEY_UP:
            --ui_line;
            ui_line &= 3;
            break;

        case KEY_LEFT:
            thresholds[ui_line] -= 1;
            break;

        case KEY_RIGHT:
            thresholds[ui_line] += 1;
            break;

        case KEY_SLEFT:
            thresholds[ui_line] -= 8;
            break;

        case KEY_SRIGHT:
            thresholds[ui_line] += 8;
            break;

        case '\t':
        case KEY_STAB:
            write_thresholds();
            find_next_entry();
            read_thresholds();
            break;

        case KEY_ENTER:
        case '\n':
        case '\r':
            poll_mode(false);
            printw("New value: ");
            getnstr(io_buf, sizeof(io_buf));
            io_buf[sizeof(io_buf)-1] = '\0';
            if(isdigit(io_buf[0]))
            {
                long value = atoi(io_buf);
                if(value < 0)
                    value = 0;
                else if(value > 255)
                    value = 255;
                thresholds[ui_line] = value;
            }
            break;

        case 's':
        case 'S':
            poll_mode(false);
            printw("Save profile: ");
            getnstr(io_buf, sizeof(io_buf)-4);
            io_buf[sizeof(io_buf)-5] = '\0';
            strcat(io_buf, ".fsr");

            if(io_buf[0])
            {
                FILE* fp = fopen(io_buf, "wb");
                if(fp)
                {
                    fwrite(thresholds, sizeof(thresholds), 1, fp);
                    fclose(fp);
                }
            }
            break;

        case 'l':
        case 'L':
            poll_mode(false);
            printw("Load profile: ");
            getnstr(io_buf, sizeof(io_buf)-4);
            io_buf[sizeof(io_buf)-5] = '\0';
            strcat(io_buf, ".fsr");

            if(io_buf[0])
            {
                FILE* fp = fopen(io_buf, "rb");
                if(fp)
                {
                    fread(thresholds, sizeof(thresholds), 1, fp);
                    fclose(fp);
                }
            }
            break;
        }
    }

exit:
    poll_mode(false);
#ifndef _WIN32
    endwin(); // Not sure why this doesn't link on PDCURSES
#endif
    hid_close(device);
    hid_free_enumeration(enumeration);
    hid_exit();
    return 0;
}

