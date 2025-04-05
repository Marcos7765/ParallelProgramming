import pandas as pd
import matplotlib.pyplot as plt
import sys

if len(sys.argv) < 4: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <csv_output_filepath>"); exit(0)
if len(sys.argv) > 4: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <csv_output_filepath>"); exit(0)

data = pd.read_csv(sys.argv[1])

data = data.groupby(['NUM_TERMS', 'DECIMAL_PLACES_PRECISION', 'DECIMAL_PLACES_VAR'], as_index=False).mean()


size = (16,9)
fig, ax = plt.subplots(figsize=size)

x = data['NUM_TERMS']
y1 = data['DECIMAL_PLACES_PRECISION']
y2 = data['TIME_MS']

ax1 = ax.twinx()

line1, = ax.plot(x, y1, c="orange", label="Decimal places", linestyle="--")
line2, = ax1.plot(x, y2, label="Mean time", linestyle=":")

ax.set_xlabel('NUM_TERMS')
ax.set_ylabel('Decimal places precision')
ax1.set_ylabel('Mean time (ms)')
ax.set_title("5 samples per NUM_TERMS, ran on a AMD Ryzen 5 7600, compiled with -Ofast")
fig.suptitle("Precision, time and complexity visualization")

linear = True
#linear = False
if linear:
    ax.set_xlim(left=data['NUM_TERMS'].min(), right=data['NUM_TERMS'].max())
else:
    ax.set_xscale('log')
    ax.set_yscale('log')

lines = [line1,line2]
ax.legend(lines, [line.get_label() for line in lines])
plt.tight_layout()
fig.savefig(sys.argv[2])
data.to_csv(sys.argv[3])