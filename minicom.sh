#!/bin/bash
# target always goes to /dev/ttyACMTarg by udev: Bus 003 Device 016: ID 0483:5740 STMicroelectronics Virtual COM Port 
minicom -D /dev/ttyACMTarg --noinit