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
# Configuration
# -----------------------------
UDP_IP = "0.0.0.0"   # listen on all interfaces
UDP_PORT = 50000
FMT = "<i"           # little-endian 32-bit int per packet (adjust as needed)
MAX_POINTS = 500    # plot window size

# -----------------------------
# Asyncio UDP receiver
# -----------------------------
class UdpFloatReceiver(asyncio.DatagramProtocol):
    def __init__(self, q: asyncio.Queue):
        self.q = q

    def datagram_received(self, data: bytes, addr):
        # Parse one float; adjust for your payload format
        try:
            value = struct.unpack(FMT, data)[0]
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
            self.q.put_nowait((ts, value))
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
        self.root.title("UDP Live Data with Spinbox")
        self.q = asyncio.Queue(maxsize=10000)
        start_event_loop_in_thread(self.q)

        # Spinbox for adjusting MAX_POINTS
        self.spin_var = tk.IntVar(value=MAX_POINTS)
        spinbox = ttk.Spinbox(root, from_=100, to=20000, increment=100, textvariable=self.spin_var, width=8)
        spinbox.pack(side=tk.TOP, padx=10, pady=5)
        spin_label = ttk.Label(root, text="Max Points")
        spin_label.pack(side=tk.TOP)

        # Matplotlib Figure
        self.fig, self.ax = plt.subplots()
        self.line, = self.ax.plot([], [], lw=1.5)
        self.ax.set_title(f"UDP live data (port {UDP_PORT})")
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("Value")

        self.times = deque(maxlen=self.spin_var.get())
        self.values = deque(maxlen=self.spin_var.get())
        self.t0 = time.time()

        self.canvas = FigureCanvasTkAgg(self.fig, master=root)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Update deque maxlen when spinbox changes
        self.spin_var.trace_add("write", self.update_max_points)

        self.ani = FuncAnimation(self.fig, self.update, init_func=self.init,
                                 interval=20, blit=True, cache_frame_data=False)

    def update_max_points(self, *args):
        max_points = self.spin_var.get()
        self.times = deque(self.times, maxlen=max_points)
        self.values = deque(self.values, maxlen=max_points)

    def init(self):
        self.ax.set_xlim(0, 10)  # initial 10s window
        self.ax.set_ylim(-1, 1)  # adjust as needed
        return (self.line,)

    def update(self, _frame):
        # Drain queue quickly
        got = 0
        while True:
            try:
                ts, val = self.q.get_nowait()
                self.times.append(ts - self.t0)
                self.values.append(val)
                got += 1
            except asyncio.QueueEmpty:
                break

        if got == 0:
            # Nothing new — skip redraw to reduce CPU
            return (self.line,)

        # Update data
        self.line.set_data(self.times, self.values)

        # Auto-scale x-window around latest 10s
        if len(self.times) > 1:
            t_last = self.times[-1]
            self.ax.set_xlim(max(0, t_last - 10), t_last)

        # Optional dynamic y-limits
        if len(self.values) > 10:
            vmin = min(self.values)
            vmax = max(self.values)
            pad = 0.05 * (vmax - vmin + 1e-6)
            self.ax.set_ylim(vmin - pad, vmax + pad)

        return (self.line,)

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
