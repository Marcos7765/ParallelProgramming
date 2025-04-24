import pandas as pd
import matplotlib.pyplot as plt
import sys
import json

if len(sys.argv) < 4: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <img_output_path> <output_path>"); exit(0)
if len(sys.argv) > 4: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <img_output_path> <output_path>"); exit(0)

data = pd.read_csv(sys.argv[1])

out_mode = input("which mode? [TIMING | COST] ")

#timing
data = data.groupby(['MODE', 'NUM_THREADS', 'ENTRY_COUNT'], as_index=False).aggregate(
    MEAN_ALT_COST=('ALT_COST','mean'),
    MEAN_TIME_MS=('TIME_MS', 'mean')
)

print(data)

size = (16,9)
fig, ax = plt.subplots(figsize=size)

counter = 0
linestyles = ["-.", "-", ":"]
markers = ["x", "o", "*"]

modes = {mode:{} for mode in pd.unique(data["MODE"])}
for mode in modes.keys():
    filtered = data[data['MODE'] == mode]
    modes[mode]["x"] = [x for x in filtered['NUM_THREADS']]
    match out_mode:
        case "TIMING":
            modes[mode]["y"] = [y for y in filtered['MEAN_TIME_MS']]
            modes[mode]["extra"] = [x for x in filtered['MEAN_ALT_COST']]
        case "COST":
            modes[mode]["extra"] = [y for y in filtered['MEAN_TIME_MS']]
            modes[mode]["y"] = [x for x in filtered['MEAN_ALT_COST']]
        case _:
            print("Invalid mode!")
            exit(1)

    if mode == "SERIAL":
        modes[mode]["x"] = [x for x in range(1, 13)]
        modes[mode]["y"] *= 12
        modes[mode]["extra"] *= 12

    ax.plot(modes[mode]["x"], modes[mode]["y"], label=mode, linestyle=linestyles[counter], marker=markers[counter])
    counter += 1

ax.legend()
ax.set_xlabel("NUM_THREADS")


match out_mode:
    case "TIMING":
        fig.suptitle("Pseudo-timing comparison — 50 entries")
        ax.set_title("5 samples per NUM_THREADS, ran on a AMD Ryzen 5 7600, compiled with -O2, artificial 1s delay on files")
        ax.set_ylabel("Mean time (ms)")
    case "COST":
        fig.suptitle("Arbitrary cost comparison — 255 entries")
        ax.set_title("30 samples per NUM_THREADS, ran on a AMD Ryzen 5 7600, compiled with -O2, files cost 1, directories cost 0")
        ax.set_ylabel("Mean cost")


plt.tight_layout()
fig.savefig(sys.argv[2])

with open(sys.argv[3], "w") as file:
    json.dump(modes, file)