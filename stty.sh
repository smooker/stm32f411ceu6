#!/bin/bash
#stty -F /dev/ttyACMTarg speed 9600 cs8 -cstopb -parenb
#stty -F /dev/ttyACMTarg -parenb -parodd -cmspar cs8 -hupcl -cstopb cread clocal -crtscts ignbrk brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl -ixon -ixoff -iuclc -ixany -imaxbel -iutf8 -opost -olcuc -ocrnl -onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0 -isig -icanon -iexten -echo -echoe -echok -echonl -noflsh -xcase -tostop -echoprt -echoctl -echoke -flusho -extproc

stty -F /dev/ttyACMTarg raw -parenb -parodd -cmspar cs8 -hupcl -cstopb cread -clocal crtscts ignbrk brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl ixon ixoff iuclc ixany -imaxbel iutf8 opost olcuc ocrnl onlcr onocr onlret ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0 -isig -icanon -iexten -echo -echoe -echok -echonl noflsh -xcase tostop -echoprt -echoctl echoke flusho -extproc

stty -a -F /dev/ttyACMTarg
#stty -F /dev/ttyACMTarg "0:4:d0cbd:8a38:3:1c:7f:15:4:0:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0"
stty -F /dev/ttyACMTarg raw -parenb -parodd -cmspar cs8 hupcl -cstopb cread clocal -crtscts -ignbrk -brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl -ixon -ixoff -iuclc -ixany -imaxbel -iutf8 -opost -olcuc -ocrnl onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0 -isig -icanon iexten echo echoe echok -echonl -noflsh -xcase -tostop -echoprt echoctl echoke -flusho -extproc