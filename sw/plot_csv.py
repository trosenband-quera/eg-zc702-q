import sys
import csv
import numpy as np
import matplotlib.pyplot as plt

def plot_csv(filename, hist_raw=False):
    with open(filename, newline='') as csvfile:
        reader = csv.reader(csvfile)
        header = next(reader)
        data = list(reader)

    if len(header) < 2:
        print("CSV file must have at least two columns (time and one channel).")
        return

    # Convert to numpy array for efficient slicing
    arr = np.array(data, dtype=float)
    time = arr[:, 0] / 1e3  # Convert us to ms for x-axis
    dt = np.mean(np.diff(arr[:, 0]))  # Average time step in us
    plotPSD = dt <= 10  # Plot PSD if time step is 10us
    print(f"Plotting data from {filename} with shape {arr.shape}")
    print(f"Average time step: {dt:.2f} us, Plot PSD: {plotPSD}")
    # Identify mixer channels (names containing 'MIX' or 'MIXER')
    mixer_indices = [i for i, h in enumerate(header) if 'MIX' in h.upper() or 'MIXER' in h.upper() or 'f' in h.lower()[0:1]]
    skip_indices  = [i for i, h in enumerate(header) if 'NSAMP' in h.upper()]
    signal_indices = [i for i, h in enumerate(header) if 'SIGNAL' in h.upper()] 
    other_indices = [i for i in range(1, arr.shape[1]) if i not in mixer_indices and i not in skip_indices and i not in signal_indices]

    # Find avgmixI and avgmixQ indices
    try:
        idx_I = header.index("avgmixI")
        idx_Q = header.index("avgmixQ")
        has_avgmix = True
    except ValueError:
        has_avgmix = False

    # Find phase0 index
    idx_phase = []
    for i, h in enumerate(header):
        if "PHASE" in h.upper():
            idx_phase.append(i)
    has_phase0 = len(idx_phase) > 0

    has_signal = len(signal_indices) > 0

    if hist_raw:
        i = 1
        data_i = arr[:, i]
        min_val = int(np.min(data_i))
        max_val = int(np.max(data_i))
        bins = np.arange(min_val, max_val + 2) - 0.5  # Bin edges so each bin is 1 wide
        std = np.std(data_i)
        plt.hist(
            data_i,
            bins=bins,
            alpha=0.6,
            label=f"{header[i]} (std={std:.2f})",
            edgecolor='black'
        )
        plt.xlabel("Raw Value")
        plt.ylabel("Count")
        plt.title("Histogram of XADC RAW Data")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.show()
        return

    # Plot in subplots: non-mixer, mixer, signal
    nplot = 3

    if plotPSD:
        fig, axs = plt.subplots(2, 2, sharex=False, figsize=(10, 8))
        gs = axs[1, 1].get_gridspec()
        # remove the underlying Axes
        for ax in axs[0:, -1]:
            ax.remove()
        axbig = fig.add_subplot(gs[0:, -1])
        ax0 = axs[0, 0]
        ax1 = axs[1, 0]
    else:
        fig, axs = plt.subplots(nplot, 1, sharex=False, figsize=(10, 4*nplot))
        ax0 = axs[0]
        ax1 = axs[1] if nplot > 1 else None
        ax2 = axs[2] if nplot > 2 else None

    ax = ax0
    # Non-mixer channels + atan2(avgmixQ, avgmixI) if present
    for i in other_indices:
        print(f"Plotting channel {i}: {header[i]}")
        lw = 1
        offset = 0
        lbl = header[i]
        scatter = False
        
        if 'phase' in header[i].lower():
            lw = 2
            offset = np.median(arr[:, i])
            lbl += f" [rad]  (offset {offset:.2f})"
            scatter = True

        ax.plot(time, arr[:, i] - offset, label=lbl, linewidth=lw)
        if scatter:
            ax.scatter(time, arr[:, i] - offset, s=12)

    if has_avgmix:
        phase = np.arctan2(arr[:, idx_Q], arr[:, idx_I])
        ax.plot(time, phase, label="atan2(avgmixQ, avgmixI)", color='black', linewidth=2, linestyle='--')
        ax.scatter(time, phase, s=6)

    ax.set_title(f"IQ Demodulator Data (Phase, etc) -- {time.size} samples")
    ax.legend(loc='lower right')
    ax.grid(True)
    ax.set_xlabel("time (ms)")
    iax = 1

    if mixer_indices:
        # Second subplot for mixer channels
        ax = ax1

        # Mixer channels
        for i in mixer_indices:
            offset = 0
            lbl = header[i]

            if 'fLO_' in header[i] or 'fDDS_' in header[i]:
                offset = np.median(arr[:, i])
                lbl += f" [Hz] (offset {offset:.2f})"
                
            y = arr[:, i]-offset

            ax.plot(time, y, label=lbl, linewidth=1)
            ax.scatter(time, y, s=6)

        ax.set_xlabel("time (ms)")
        ax.set_title("Other Channels")
        ax.legend()
        ax.grid(True)
        # ax.set_ylim(-10, 10)
        iax += 1

    if has_signal:
        # Third subplot
        for idx in signal_indices:
            
            offset = 0
            lbl = header[idx]
 
            y = arr[:, idx]-offset

            ax2.plot(time, y, label=lbl, linewidth=1)
            ax2.scatter(time, y, s=6)

            ax2.set_xlabel("time (ms)")
            ax2.set_title("Signal Magnitudes")
            ax2.legend()
            ax2.grid(True)
            iax += 1

        # Interpolate phase0 onto a 1us grid and plot phase noise power spectrum
    if plotPSD:
        ax = axbig
        plotphase = False
        lbl = 'Phase Noise'
        time_us = arr[:, 0]  # time in us
        t_min = int(np.min(time_us))
        t_max = int(np.max(time_us))
        t_grid = np.arange(t_min, t_max+1, 1)  # 1us grid
        for idx in idx_phase:
            phase_interp = np.interp(t_grid, time_us, arr[:, idx])
            n = len(phase_interp)
            n_chunks = 10
            chunk_size = n // n_chunks
            bw = 1e6 / (n/n_chunks)  # Hz
            bwmax = 100e3;
            print(f"Computing PSD for {header[idx]} with {n_chunks} chunks of size {chunk_size}, BW={bw:.2f} Hz")   
            psd_list = []
            for i in range(n_chunks):
                start = i * chunk_size
                end = (i + 1) * chunk_size
                chunk = phase_interp[start:end]
                if len(chunk) < 2:
                    continue
                chunk_detrended = chunk - np.mean(chunk)
                fs = 1e6  # 1 MHz sample rate (1us spacing)
                freq = np.fft.rfftfreq(len(chunk_detrended), d=1/fs)
                psd = np.abs(np.fft.rfft(chunk_detrended))**2 / len(chunk_detrended)
                psd_list.append(psd)
            # Average PSDs
            if plotphase:
                avg_psd = np.mean(psd_list, axis=0)/bw  # Normalize to 1/Hz
            else:
                avg_psd = freq*freq*np.mean(psd_list, axis=0)/bw  # Normalize to 1/Hz
                lbl = 'Frequency Noise'
            # Plot averaged phase noise power spectrum
            ax.semilogx(freq[1:], 10*np.log10(avg_psd[1:]), label=f'{header[idx]}')

        ax.set_xlim(bw, bwmax)
        ax.set_xlabel('Frequency (Hz)')
        ax.set_ylabel('Power Spectral Density (dB/Hz)')
        ax.set_title(f'{lbl} PSD, {bw:.2f} Hz/point')
        ax.grid(True)
        ax.legend()
        iax += 1

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Plot IQ demodulator data from CSV file.")
    parser.add_argument("csv_file", help="CSV file to plot")
    parser.add_argument("--hist-raw", action="store_true", help="Show histogram of RAW_ columns")
    args = parser.parse_args()
    plot_csv(args.csv_file, hist_raw=args.hist_raw)
