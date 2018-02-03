all: build install
	
install:
	cp beep /usr/local/bin/beep

build:
	cc beep.c -lsoundio -o beep
