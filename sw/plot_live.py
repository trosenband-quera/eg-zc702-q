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

import message_pb2
import socket
# -----------------------------
# Configuration
# -----------------------------
TCP_IP = '127.0.0.1'  # TODO: set to your server IP
TCP_PORT = 60000      # TODO: set to your server port
# -----------------------------
UDP_IP = "0.0.0.0"   # listen on all interfaces
UDP_PORT = 50000
MAX_POINTS = 1500    # plot window size
YMAX = 5000  # fixed y-axis max value
# -----------------------------
# Asyncio UDP receiver
# -----------------------------
class UdpReceiver(asyncio.DatagramProtocol):
    def __init__(self, q: asyncio.Queue):
        self.q = q

    def datagram_received(self, data: bytes, addr):
        # Parse Protobuf message
        msg = message_pb2.plotData()
        try:
            # print(f"Received data: {data}, length={len(data)} from {addr}")
            msg.ParseFromString(data)
            ts = time.time()
            # Non-blocking put (drop oldest if queue is full)
            if self.q.full():
                try:
                    self.q.get_nowait()
                except asyncio.QueueEmpty:
                    pass
            try:
                self.q.put_nowait((ts, msg))

            except asyncio.QueueFull:
                pass

        except Exception as e:
            print(f"Failed to parse Protobuf message: {e}")

        

async def udp_server(q: asyncio.Queue):
    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: UdpReceiver(q),
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
        self.root.title("I/Q Demodulator Live Data")
        self.q = asyncio.Queue(maxsize=10000)
        start_event_loop_in_thread(self.q)


        # Controls frame for horizontal layout
        controls_frame = ttk.Frame(root)
        controls_frame.pack(side=tk.TOP, fill=tk.X, padx=10, pady=5)
        icol = 0

        # Spinbox for adjusting MAX_POINTS
        spin_label = ttk.Label(controls_frame, text="Max Points")
        spin_label.grid(row=0, column=icol, padx=5)
        icol += 1
        
        self.spin_var = tk.IntVar(value=MAX_POINTS)
        spinbox = ttk.Spinbox(controls_frame, from_=100, to=20000, increment=100, textvariable=self.spin_var, width=8)
        spinbox.grid(row=0, column=icol, padx=5)
        icol += 1

        
        # Divider
        sep = ttk.Separator(controls_frame, orient='vertical')
        sep.grid(row=0, column=icol, sticky='ns', padx=2)
        icol += 1

        # Label for number of samples received
        samples_caption = ttk.Label(controls_frame, text="Samples received")
        samples_caption.grid(row=0, column=icol, padx=5)        
        icol += 1
        
        self.samples_received_var = tk.IntVar(value=0)
        samples_label = ttk.Label(controls_frame, textvariable=self.samples_received_var, font=("TkDefaultFont", 12, "bold"))
        samples_label.grid(row=0, column=icol, padx=5)
        icol += 1
        
        # Divider
        sep = ttk.Separator(controls_frame, orient='vertical')
        sep.grid(row=0, column=icol, sticky='ns', padx=2)
        icol += 1

        # Run/Pause button
        self.paused = False
        self.run_pause_btn = ttk.Button(controls_frame, text="PAUSE", command=self.toggle_run_pause)
        self.run_pause_btn.grid(row=0, column=icol, padx=5)
        icol += 1
        self.times = [deque(maxlen=self.spin_var.get())]
        self.values = [deque(maxlen=self.spin_var.get())]
        self.t0 = time.time()
        
        self.fig = plt.figure()
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Update deque maxlen when spinbox changes
        self.spin_var.trace_add("write", self.update_max_points)

        self.lines = []
        self.axes = []
        self.ani = FuncAnimation(self.fig, self.update,
                                 interval=20, blit=True, cache_frame_data=False)

    def toggle_run_pause(self):
        self.paused = not self.paused
        self.run_pause_btn.config(text="RUN" if self.paused else "PAUSE")
        self.send_pause_tcp_message()

    def send_pause_tcp_message(self):
        MESSAGE = b'PAUSED' if self.paused else b'RUN'   # TODO: set to your desired message
        try:
            with socket.create_connection((TCP_IP, TCP_PORT), timeout=2) as sock:
                sock.sendall(MESSAGE)
        except Exception as e:
            print(f"Failed to send TCP pause message: {e}")

    def update_max_points(self, *args):
        max_points = self.spin_var.get()
        self.times = [deque(t, maxlen=max_points) for t in self.times]
        self.values = [deque(v, maxlen=max_points) for v in self.values]

    def setnumplots(self, n, titles=None, minmax=None):
        # Matplotlib Figure: n subplots
        if n>4:
            nrows = 4
            ncols = n // 4 + (1 if n % 4 != 0 else 0)
        else:
            nrows = n
            ncols = 1

        width = ncols*500
        self.root.geometry(f"{width}x{nrows*400}+{4000-width}+0")
        self.fig.clf()
        self.axes = [None]*n
        self.lines = [None]*n
        # Create subplots
        ival = 0
        for c in range(ncols):
            for r in range(nrows):
                iplot = r*ncols + c
                if ival >= n:
                    break

                
                ax = self.fig.add_subplot(nrows, ncols, iplot + 1)
                self.axes[ival] = ax

                line, = ax.plot([], [], lw=1.5)
                title = f"Signal {ival}" if titles is None else titles[ival]
                ax.set_title(title)
                

                self.lines[ival] = line

                if ival >= n or r+1 == nrows:
                    ax.set_xlabel("Time (s)")
                else:
                    ax.set_xticklabels([])

                ax.set_xlim(0, 10)
                if minmax is not None:
                    ax.set_ylim(minmax[ival], minmax[ival+len(minmax)//2])
                else:
                    ax.set_ylim(0, YMAX)

                ax.grid(True)
                ax.figure.canvas.draw_idle()
                print(f"Created plot {iplot}, rows={nrows}, cols={ncols}, title={title}")
                ival += 1

        return tuple(self.lines)

    def update(self, _frame):
        got_values = 0
        ts = None
        while True:
            try:
                ts, msg = self.q.get_nowait()
                if msg.ch_info:
                    self.update_channel_info(msg.ch_info)
                if msg.values:
                    self.update_values(list(msg.values), ts)
            except asyncio.QueueEmpty:
                break

        for i in range(len(self.lines)):
            self.lines[i].set_data(self.times[i], self.values[i])
            if ts is not None and self.times[i]:
                t_last = ts - self.t0
                self.axes[i].set_xlim(max(0, t_last - 10), t_last)

        return tuple(self.lines)
       
    def update_channel_info(self, channel_info):
        nval = len(channel_info)
        if len(self.times) != nval:
            titles = [ci.name for ci in channel_info]
            minmax = [ci.min for ci in channel_info] + [ci.max for ci in channel_info]
            print(f"Updating channel info: {titles}, minmax={minmax}")
            self.setnumplots(nval, titles=titles, minmax=minmax)
            self.times = [deque(maxlen=self.spin_var.get()) for _ in range(nval)]
            self.values = [deque(maxlen=self.spin_var.get()) for _ in range(nval)]

    def update_values(self, vals, ts):
        if len(self.times) != len(vals):
            nval = len(vals)
            self.setnumplots(nval)
            self.times = [deque(maxlen=self.spin_var.get()) for _ in range(nval)]
            self.values = [deque(maxlen=self.spin_var.get()) for _ in range(nval)]

        for i in range(len(vals)):
            self.times[i].append(ts - self.t0)
            self.values[i].append(vals[i])

        self.samples_received_var.set(self.samples_received_var.get() + 1)

def main():
    root = tk.Tk()
    root.geometry("1000x1000+2000+0")

    # Optional: Make sure the window is not resizable
    root.resizable(False, False)
    app = LivePlotApp(root)
    # Exit the process when the window is closed
    root.protocol("WM_DELETE_WINDOW", root.quit)
    root.mainloop()
    # Ensure clean exit after mainloop ends
    sys.exit(0)

if __name__ == "__main__":
    main()
