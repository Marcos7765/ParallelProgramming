import pandas as pd
import matplotlib.pyplot as plt
import sys

if len(sys.argv) < 4: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <csv_output_filepath>")
if len(sys.argv) > 4: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <csv_output_filepath>")

data = pd.read_csv(sys.argv[1])

data = data.groupby(['MODE', 'MAT_COL', 'MAT_ROW']).mean().reset_index().drop('MAT_ROW', axis=1)

size = (16,9)
mat_col_cutoff = 10**8
fig, ax = plt.subplots(figsize=size)

cutoff = data[data['MAT_COL'] < mat_col_cutoff]
#cutoff['MAT_COL'] = cutoff['MAT_COL'].apply(lambda x : int(x**2))

for label in pd.unique(data['MODE']):
    filtered = cutoff[cutoff['MODE']==label]
    x = filtered['MAT_COL']
    y = filtered['TIME_MS']
    #ax.scatter(x, y, label=label)
    ax.plot(x, y, label=label, marker='o')

ax.set_xlabel('MAT_SIDE')
ax.set_ylabel('mean time (ms)')
ax.set_title("10 samples per MAT_SIDE, ran on a AMD Ryzen 5 7600, compiled with -O2")
fig.suptitle("Time comparison between algorithms")

linear = True
#linear = False
if linear:
    ax.set_xlim(left=cutoff['MAT_COL'].min(), right=cutoff['MAT_COL'].max())
    ax.set_ylim(bottom=data['TIME_MS'].min())
else:
    ax.set_xscale('log')
    ax.set_yscale('log')
ax.legend()
plt.tight_layout()
fig.savefig(sys.argv[2])
data.rename(columns={"MAT_COL":"MAT_SIDE"}).to_csv(sys.argv[3])