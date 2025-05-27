import pandas as pd
import sys

if len(sys.argv) < 3: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <output_path>"); exit(0)
if len(sys.argv) > 3: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <output_path>"); exit(0)

data = pd.read_csv(sys.argv[1])

data = data.groupby(['MODE', 'PROCESS_COUNT', 'BARSIZE']).aggregate(
    MEAN_TIME_MS=('TIME_MS', 'mean'),
)

data.to_csv(sys.argv[2])