#!/usr/bin/bash

gcc chall.c -o chall -lm -ldl -std=gnu11 -Wpedantic -O2 -ffast-math -DNCURSES_WIDECHAR=1 -DNDEBUG=1 -l:libncursesw.a -l:libtinfo.a
strip chall
