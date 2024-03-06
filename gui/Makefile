ifndef $(OS)
	OS:=$(shell uname -s)
endif

ifdef MINGW
override CC:=x86_64-w64-mingw32-gcc
override CCFLAGS+= -mwindows
OS:=Windows_NT
endif

ifeq ($(OS),Windows_NT)
pubby-pad.exe: main.c
	$(CC) $(CCFLAGS) main.c -static-libgcc hidapi/windows/hid.c -o $@ -I hidapi/hidapi/ -lpdcurses
endif

ifeq ($(OS),Linux)
pubby-pad: main.c
	$(CC) $(CCFLAGS) main.c hidapi/linux/hid.c -o $@ -I hidapi/hidapi/ -lncurses -ludev
endif

ifeq ($(OS),Darwin)
pubby-pad: main.c
	$(CC) $(CCFLAGS) main.c hidapi/mac/hid.c -o $@ -I hidapi/hidapi/ -lncurses
endif

windows.zip: pubby-pad.exe
	rm $@
	rm -rf windows
	mkdir windows
	cp pubby-pad.exe windows/
	cp libpdcurses.dll windows/
	zip -r $@ windows/

linux.zip: pubby-pad 69-pubby-hid.rules
	rm $@
	rm -rf linux
	mkdir linux
	cp pubby-pad linux/
	cp 69-pubby-hid.rules linux/
	zip -r $@ linux/
