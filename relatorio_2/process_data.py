import pandas as pd
import matplotlib.pyplot as plt
import sys

if len(sys.argv) < 4: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <csv_output_filepath>")
if len(sys.argv) > 4: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <csv_output_filepath>")

data = pd.read_csv(sys.argv[1])

data = data.groupby(['MODE', 'VEC_SIZE']).mean().reset_index()

size = (16,9)
param_cutoff = 10**9
fig, ax = plt.subplots(figsize=size)

cutoff = data[data['VEC_SIZE'] < param_cutoff]

for label in pd.unique(data['MODE']):
    filtered = cutoff[cutoff['MODE']==label]
    x = filtered['VEC_SIZE']
    y = filtered['TIME_MS']
    ax.plot(x, y, label=label, marker='o')

ax.set_xlabel('VEC_SIZE')
ax.set_ylabel('mean time (ms)')
ax.set_title("5 samples per VEC_SIZE, ran on a AMD Ryzen 5 7600, compiled with -O0")
fig.suptitle("Time comparison between algorithms")

linear = True
#linear = False
if linear:
    ax.set_xlim(left=cutoff['VEC_SIZE'].min(), right=cutoff['VEC_SIZE'].max())
    ax.set_ylim(bottom=data['TIME_MS'].min())
else:
    ax.set_xscale('log')
    ax.set_yscale('log')
ax.legend()
plt.tight_layout()
fig.savefig(sys.argv[2])
data.to_csv(sys.argv[3])