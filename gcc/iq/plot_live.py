
import asyncio
import threading
from collections import deque
import struct
import time

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# -----------------------------
# Configuration
# -----------------------------
UDP_IP = "0.0.0.0"   # listen on all interfaces
UDP_PORT = 50000
FMT = "<f"           # little-endian 32-bit float per packet (adjust as needed)
MAX_POINTS = 5000    # plot window size

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
# Matplotlib real-time plot
# -----------------------------
def main():
    # Thread-safe bridge: use asyncio.Queue but we only pull from it in main thread
    q = asyncio.Queue(maxsize=10000)
    start_event_loop_in_thread(q)

    # Prepare plot
    plt.style.use('seaborn-v0_8-darkgrid')
    fig, ax = plt.subplots()
    line, = ax.plot([], [], lw=1.5)
    ax.set_title(f"UDP live data (port {UDP_PORT})")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Value")

    # Ring buffers
    times = deque(maxlen=MAX_POINTS)
    values = deque(maxlen=MAX_POINTS)
    t0 = time.time()

    # Enable blitting for speed
    use_blit = True

    def init():
        ax.set_xlim(0, 10)  # initial 10s window
        ax.set_ylim(-1, 1)  # adjust as needed
        return (line,)

    def update(_frame):
        # Drain queue quickly
        got = 0
        while True:
            try:
                ts, val = q.get_nowait()
                times.append(ts - t0)
                values.append(val)
                got += 1
            except asyncio.QueueEmpty:
                break

        if got == 0:
            # Nothing new — skip redraw to reduce CPU
            return (line,)

        # Update data
        line.set_data(times, values)

        # Auto-scale x-window around latest 10s
        if len(times) > 1:
            t_last = times[-1]
            ax.set_xlim(max(0, t_last - 10), t_last)

        # Optional dynamic y-limits
        if len(values) > 10:
            vmin = min(values)
            vmax = max(values)
            pad = 0.05 * (vmax - vmin + 1e-6)
            ax.set_ylim(vmin - pad, vmax + pad)

        return (line,)

    ani = FuncAnimation(fig, update, init_func=init, interval=20,
                        blit=use_blit, cache_frame_data=False)

    plt.show()

if __name__ == "__main__":
    main()
