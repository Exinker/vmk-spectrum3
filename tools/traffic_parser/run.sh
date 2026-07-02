#!/bin/bash

tshark -l -i enp7s0 -f 'udp port 555' -T fields -e 'ip.src' -e 'ip.dst' -e 'data' 2>/dev/null | python main.py