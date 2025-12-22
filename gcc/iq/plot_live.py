import asyncio
import threading
from collections import deque
import struct
import time
import tkinter as tk
from tkinter import ttk
import sys

import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation

# -----------------------------
UDP_IP = "0.0.0.0"   # listen on all interfaces
UDP_PORT = 50000
NUM_FLOATS = 4
FMT = f"<{NUM_FLOATS}f"  # little-endian 4x 32-bit float per packet
MAX_POINTS = 1500    # plot window size
YMAX = 5000  # fixed y-axis max value
# -----------------------------
# Asyncio UDP receiver
# -----------------------------
class UdpFloatReceiver(asyncio.DatagramProtocol):
    def __init__(self, q: asyncio.Queue):
        self.q = q

    def datagram_received(self, data: bytes, addr):
        # Parse four floats per packet
        try:
            values = struct.unpack(FMT, data)
        except struct.error:
            return  # ignore malformed packet
        ts = time.time()
        # Non-blocking put (drop oldest if queue is full)
        if self.q.full():
            try:
                self.q.get_nowait()
            except asyncio.QueueEmpty:
                pass
        try:
            self.q.put_nowait((ts, values))
        except asyncio.QueueFull:
            pass

async def udp_server(q: asyncio.Queue):
    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: UdpFloatReceiver(q),
        local_addr=(UDP_IP, UDP_PORT),
        reuse_port=True
    )
    try:
        await asyncio.Future()  # run forever
    finally:
        transport.close()

def start_event_loop_in_thread(q: asyncio.Queue):
    def runner():
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        loop.run_until_complete(udp_server(q))
    t = threading.Thread(target=runner, daemon=True)
    t.start()
    return t

# -----------------------------
# Tkinter GUI with Matplotlib
# -----------------------------

class LivePlotApp:
    def __init__(self, root):
        self.root = root
        self.root.title("UDP Live Data (4 floats, 4 plots)")
        self.q = asyncio.Queue(maxsize=10000)
        start_event_loop_in_thread(self.q)

        # Spinbox for adjusting MAX_POINTS
        self.spin_var = tk.IntVar(value=MAX_POINTS)
        spinbox = ttk.Spinbox(root, from_=100, to=20000, increment=100, textvariable=self.spin_var, width=8)
        spinbox.pack(side=tk.TOP, padx=10, pady=5)
        spin_label = ttk.Label(root, text="Max Points")
        spin_label.pack(side=tk.TOP)

        # Matplotlib Figure: 2x2 subplots
        self.fig, self.axes = plt.subplots(2, 2, figsize=(10, 10))
        self.lines = []
    
        for i, ax in enumerate(self.axes.flat):
            line, = ax.plot([], [], lw=1.5)
            ax.set_title(f"Signal {i}")
            ax.set_xlabel("Time (s)")
            self.lines.append(line)

        self.times = [deque(maxlen=self.spin_var.get()) for _ in range(NUM_FLOATS)]
        self.values = [deque(maxlen=self.spin_var.get()) for _ in range(NUM_FLOATS)]
        self.t0 = time.time()

        self.canvas = FigureCanvasTkAgg(self.fig, master=root)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Update deque maxlen when spinbox changes
        self.spin_var.trace_add("write", self.update_max_points)

        self.ani = FuncAnimation(self.fig, self.update, init_func=self.init,
                                 interval=20, blit=True, cache_frame_data=False)

    def update_max_points(self, *args):
        max_points = self.spin_var.get()
        self.times = [deque(t, maxlen=max_points) for t in self.times]
        self.values = [deque(v, maxlen=max_points) for v in self.values]

    def init(self):
        for ax in self.axes.flat:
            ax.set_xlim(0, 10)
            ax.set_ylim(0, YMAX)
        return tuple(self.lines)

    def update(self, _frame):
        got = 0
        while True:
            try:
                ts, vals = self.q.get_nowait()
                for i in range(NUM_FLOATS):
                    self.times[i].append(ts - self.t0)
                    self.values[i].append(vals[i])
                got += 1
                #print(f"Received: {vals} at {ts}")
            except asyncio.QueueEmpty:
                break

        if got == 0:
            return tuple(self.lines)

        for i, (line, ax) in enumerate(zip(self.lines, self.axes.flat)):
            line.set_data(self.times[i], self.values[i])
            if len(self.times[i]) > 1:
                t_last = self.times[i][-1]
                ax.set_xlim(max(0, t_last - 10), t_last)
            if len(self.values[i]) > 10:
                ax.set_ylim(0, YMAX)
        return tuple(self.lines)

def main():
    root = tk.Tk()
    app = LivePlotApp(root)
    # Exit the process when the window is closed
    root.protocol("WM_DELETE_WINDOW", root.quit)
    root.mainloop()
    # Ensure clean exit after mainloop ends
    sys.exit(0)

if __name__ == "__main__":
    main()
