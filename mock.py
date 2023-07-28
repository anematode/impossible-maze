#!/usr/bin/python3

from pykeyboard import PyKeyboard
import time

k = PyKeyboard()
p = open("path.txt",'r').read()

time.sleep(4)
for c in p:
    time.sleep(0.4)
    k.press_key(c)
    time.sleep(0.08)
    k.release_key(c)
    
