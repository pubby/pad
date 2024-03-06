ifndef $(OS)
	OS:=$(shell uname -s)
endif

ifdef MINGW
override CC:=x86_64-w64-mingw32-gcc
OS:=Windows_NT
endif

ifeq ($(OS),Windows_NT)
pubby-pad.exe: main.c
	$(CC) main.c -static-libgcc hidapi/windows/hid.c -o $@ -I hidapi/hidapi/ -lpdcurses
endif

ifeq ($(OS),Linux)
pubby-pad: main.c
	$(CC) main.c hidapi/linux/hid.c -o $@ -I hidapi/hidapi/ -lncurses -ludev
endif

ifeq ($(OS),Darwin)
pubby-pad: main.c
	$(CC) main.c hidapi/mac/hid.c -o $@ -I hidapi/hidapi/ -lncurses
endif
