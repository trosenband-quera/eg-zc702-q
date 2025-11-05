import sys
import csv
import numpy as np
import matplotlib.pyplot as plt

def plot_csv(filename):
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
    for i in range(1, arr.shape[1]):
        plt.plot(time, arr[:, i]/4096, label=header[i])
        plt.scatter(time, arr[:, i]/4096, s=12)  # Show dots for data points

    plt.xlabel("time (ms)")
    plt.ylabel("Voltage")
    plt.title("XADC CSV Data")
    plt.legend()
    plt.grid(True)  # Show grid
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <csv_file>")
        sys.exit(1)
    plot_csv(sys.argv[1])