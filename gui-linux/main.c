// STD
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Linux
#include <libudev.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

// Unix
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Curses (put last)
#include <ncurses.h>

enum
{
    LEFT,
    DOWN,
    UP,
    RIGHT
};

enum
{
  REPORT_ID_BUTTONS = 1,
  REPORT_ID_SENSORS,
  REPORT_ID_THRESHOLDS,
  REPORT_ID_COUNT
};

enum
{
    CP_ERROR = 1,
    CP_BAR_PRE,
    CP_BAR_POST,
};

char const* const arrow_names[4] = 
{ 
    "Button 0: ",
    "Button 1: ",
    "Button 2: ",
    "Button 3: ",
};

uint8_t sensors[4] = {};
uint8_t thresholds[4] = {};
int ui_line = 0;

static char io_buf[64];

static struct udev* udev;
struct udev_enumerate* enumerate;
struct udev_list_entry* entry;
static int fd = -1;

void read_sensors(int fd)
{
    io_buf[0] = REPORT_ID_SENSORS; // Report number
    int res = ioctl(fd, HIDIOCGFEATURE(sizeof(io_buf)), io_buf);
    if(res >= 1)
        memcpy(sensors, io_buf+1, res-1);
}

void read_thresholds(int fd)
{
    io_buf[0] = REPORT_ID_THRESHOLDS; // Report number
    int res = ioctl(fd, HIDIOCGFEATURE(sizeof(io_buf)), io_buf);
    if(res >= 1)
        memcpy(thresholds, io_buf+1, res-1);
}

void write_thresholds(int fd)
{
    io_buf[0] = REPORT_ID_THRESHOLDS; // Report number
    memcpy(io_buf+1, thresholds, sizeof(thresholds));
    ioctl(fd, HIDIOCSFEATURE(sizeof(thresholds) + 1), io_buf);
}

char const* next_entry(void)
{
    struct udev_device* dev = NULL;
    bool retried = false;

    while(entry)
    {
        const char* path = udev_list_entry_get_name(entry);
        entry = udev_list_entry_get_next(entry);
        if(!entry && !retried)
        {
            entry = udev_enumerate_get_list_entry(enumerate);
            retried = true;
        }

        dev = udev_device_new_from_syspath(udev, path);
        if(!dev)
            continue;

        struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        if(!parent)
            continue;

        char const* vid = udev_device_get_sysattr_value(parent,"idVendor");
        char const* pid = udev_device_get_sysattr_value(parent,"idProduct");
        char const* man = udev_device_get_sysattr_value(parent,"manufacturer");
        char const* devnode;

        if(vid && pid && man
           && strcmp(vid, "16c0") == 0
           && strcmp(pid, "27d9") == 0
           && strcmp(man, "http://pubby.games") == 0
           && (devnode = udev_device_get_devnode(dev)))
        {
            write_thresholds(fd);
            close(fd);
            fd = open(devnode, O_RDWR | O_NONBLOCK);
            read_thresholds(fd);

            devnode = strdup(devnode); // TODO
            udev_device_unref(dev);
            return devnode;
        }

        udev_device_unref(dev);
    }

    return NULL;
}

int main(void)
{
    udev = udev_new();
    if(!udev) 
    {
        fprintf(stderr, "udev_new() failed\n");
        return 1;
    }

    enumerate = udev_enumerate_new(udev);

    udev_enumerate_add_match_subsystem(enumerate, "hidraw");
    udev_enumerate_add_match_is_initialized(enumerate);
    //udev_enumerate_add_match_sysattr(enumerate, "idVendor",  "16c0");
    //udev_enumerate_add_match_sysattr(enumerate, "idProduct", "27d9");
    udev_enumerate_scan_devices(enumerate);

    entry = udev_enumerate_get_list_entry(enumerate);
    char const* devnode = next_entry();
    read_thresholds(fd);
    read_sensors(fd);

    initscr();
    raw(); // Disable line buffering
    noecho();
    keypad(stdscr, TRUE); // Enable more keypresses

    halfdelay(1); // Poll input every 1/10 a second

    // Colors
    start_color();
    init_pair(CP_ERROR, COLOR_RED, COLOR_BLACK);
    init_pair(CP_BAR_PRE, COLOR_MAGENTA, COLOR_MAGENTA);
    init_pair(CP_BAR_POST, COLOR_CYAN, COLOR_BLACK);

    clear();

    while(true)
    {
        read_sensors(fd);

        int line = 0;
        mvprintw(line++, 0, "Pad Sensor Thresholds: %s", devnode ? devnode : "NULL");
        mvprintw(line++, 0, "[Tab]: Toggle device  [q]: Quit");
        line++;

        for(int i = 0; i < 4; ++i)
        {
            if(i == ui_line)
                attron(A_REVERSE);
            mvprintw(line++, 2, "%s %3i", arrow_names[i], thresholds[i]);
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
        if(fd < 0)
        {
            attron(COLOR_PAIR(CP_ERROR));
            printw("Error: Unable to access USB device. Are you running as root?");
            attroff(COLOR_PAIR(CP_ERROR));
        }
        clrtoeol();

        refresh();

        switch(getch())
        {
        case KEY_EXIT:
        case 'q':
        case 'Q':
            write_thresholds(fd);
            goto exit;
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
            devnode = next_entry();
            break;
        }
    }

exit:

    endwin();
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    return 0;
}
