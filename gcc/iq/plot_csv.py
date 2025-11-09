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
    if hist_raw:
        # for i in range(1):
        i=1
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
            edgecolor='black'  # Add black lines around bars
        )
        plt.xlabel("Raw Value")
        plt.ylabel("Count")
        plt.title("Histogram of XADC RAW Data")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.show()
        return

    for i in range(1, arr.shape[1]):
        plt.plot(time, arr[:, i]/4096, label=header[i])
        plt.scatter(time, arr[:, i]/4096, s=12)

    plt.xlabel("time (ms)")
    plt.ylabel("Voltage")
    plt.title("XADC CSV Data")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Plot XADC data from CSV file.")
    parser.add_argument("csv_file", help="CSV file to plot")
    parser.add_argument("--hist-raw", action="store_true", help="Show histogram of RAW_ columns")
    args = parser.parse_args()
    plot_csv(args.csv_file, hist_raw=args.hist_raw)