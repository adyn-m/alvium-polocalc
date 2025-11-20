from datetime import datetime
import matplotlib.pyplot as plt
import numpy as np
import re
import os

#file_path = "/home/sst/data/alvium_test/2025-09-26_125218/alvium_log.txt"
#file_path = "/home/sst/data/alvium_test/2025-09-26_125335/alvium_log.txt"
base_dir = "/home/sst/data/alvium_test"
subfolders = [f.path for f in os.scandir(base_dir) if f.is_dir()]
input_folder = max(subfolders, key=os.path.getmtime)
file_path = os.path.join(input_folder, "alvium_log.txt")
print(f"Automatically loading latest log file: {file_path}")

timestamps = []

counter = 0

with open(file_path, "r") as f:
    for line in f:
        match = re.search(r"\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})", line)
        if match:
            ts_str = match.group(1)
            ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S.%f")
            timestamps.append(ts)

deltas = [(t2 - t1).total_seconds() for t1, t2 in zip(timestamps, timestamps[1:])]
print(f"Period: {deltas}")
mean_deltas = np.mean(deltas)
std_deltas = np.std(deltas)

average_fps = 1 / mean_deltas

textstr = f"Mean = {mean_deltas:.3f} s\nStd = {std_deltas:.3f} s\nMean Frame Rate = {average_fps:.2f} Hz"

plt.hist(deltas, bins=50, edgecolor='black')
plt.xlabel('Time between frames [s]')
plt.ylabel('Count')
plt.title('Histogram of Frame Rates')
plt.text(0.80, 0.90, textstr, transform=plt.gca().transAxes, fontsize=10, verticalalignment='top', horizontalalignment='right', bbox=dict(facecolor='white', alpha=0.7, edgecolor='black'))
plt.show()
