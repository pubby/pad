ifndef $(OS)
	OS:=$(shell uname -s)
endif

ifeq ($(OS),Windows_NT)
pubby-pad.exe: main.c
	$(CC) main.c hidapi/pc/hid.c -o $@ -I hidapi/hidapi/ -lncurses
endif

ifeq ($(OS),Linux)
pubby-pad: main.c
	$(CC) main.c hidapi/linux/hid.c -o $@ -I hidapi/hidapi/ -lncurses -ludev
endif

ifeq ($(OS),Darwin)
pubby-pad: main.c
	$(CC) main.c hidapi/mac/hid.c -o $@ -I hidapi/hidapi/ -lncurses
endif
