beep
====

A cross-platform (Windows 7+, MacOS 10.10+, Linux 3.7+) implementation
of [beep](http://www.johnath.com/beep/) based on
[libsoundio](https://github.com/andrewrk/libsoundio).

Check out https://github.com/ShaneMcC/beeps for cool files!

Requirements
============

-   [libsoundio](https://github.com/andrewrk/libsoundio)

Install
=======

    make

TODO
====

-   file input (`-s` and `-c` options)
-   better smoothing of the sound at the beginning and end of the notes
    (continue the sound until it is under a threshold then set it 0
    instead of doing an exponential decay)
