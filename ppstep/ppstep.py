
import logging
from quetzal.instruments.remote import RemoteInstrument
import numpy as np
import matplotlib.pyplot as plt
import time
import sys
import json
import pprint

logging.basicConfig(level=logging.INFO)

ip = "172.16.100.96"
ip = "127.0.0.1"
ip = "10.0.0.111"
loops = -1
if len(sys.argv) > 1:
	cmd_file = sys.argv[1]
else:
	print("Usage: python ppstep.py <cmd file> [<steps>]")
	print("Example: python ppstep.py large.json 1")
	exit()

steps = 0
if len(sys.argv) > 2:
	steps = int(sys.argv[2])

with open(cmd_file, 'r') as f:
    cmd_list = json.load(f)

ppoly = RemoteInstrument(ip, 8710)
ctl = RemoteInstrument(ip, 8712)
PLAY_IMMEDIATELY = True

for command in cmd_list:
	cmd = [command]
	ppoly.clear_commands()
	ppoly.set_instructions(instructions=cmd_list)
	ppoly.play_immediate() if PLAY_IMMEDIATELY else None

