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
    print(f"Plotting data from {filename} with shape {arr.shape}")

    # Identify mixer channels (names containing 'MIX' or 'MIXER')
    mixer_indices = [i for i, h in enumerate(header) if 'MIX' in h.upper() or 'MIXER' in h.upper() or 'signal' in h.lower()]
    skip_indices  = [i for i, h in enumerate(header) if 'NSAMP' in h.upper()]
    other_indices = [i for i in range(1, arr.shape[1]) if i not in mixer_indices and i not in skip_indices]

    # Find avgmixI and avgmixQ indices
    try:
        idx_I = header.index("avgmixI")
        idx_Q = header.index("avgmixQ")
        has_avgmix = True
    except ValueError:
        has_avgmix = False

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

    # Plot in subplots: non-mixer, mixer
    fig, axs = plt.subplots(2, 1, sharex=True, figsize=(10, 8))

    # Non-mixer channels + atan2(avgmixQ, avgmixI) if present
    for i in other_indices:
        print(f"Plotting channel {i}: {header[i]}")
        lw = 1
        if 'f0' in header[i].lower() or 'phase' in header[i].lower():
            lw = 4
            
        axs[0].plot(time, arr[:, i], label=header[i], linewidth=lw)
        axs[0].scatter(time, arr[:, i], s=12)
    if has_avgmix:
        phase = np.arctan2(arr[:, idx_Q], arr[:, idx_I])
        axs[0].plot(time, phase, label="atan2(avgmixQ, avgmixI)", color='purple', linewidth=2)
    axs[0].set_title("IQ Demodulator Data (Non-Mixer Channels & Phase)")
    axs[0].legend()
    axs[0].grid(True)

    # Mixer channels
    for i in mixer_indices:
        y = arr[:, i]
        if 'SIGNAL' in header[i].upper() or 'AVG' in header[i].upper():
            axs[1].plot(time, y, label=header[i], linewidth=4)
        else:
            axs[1].scatter(time, y, s=6, label=header[i])

    axs[1].set_xlabel("time (ms)")
    axs[1].set_title("Mixer Channels")
    axs[1].legend()
    axs[1].grid(True)

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Plot IQ demodulator data from CSV file.")
    parser.add_argument("csv_file", help="CSV file to plot")
    parser.add_argument("--hist-raw", action="store_true", help="Show histogram of RAW_ columns")
    args = parser.parse_args()
    plot_csv(args.csv_file, hist_raw=args.hist_raw)
