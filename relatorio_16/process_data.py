import pandas as pd
import sys

if len(sys.argv) < 3: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <output_path>"); exit(0)
if len(sys.argv) > 3: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <output_path>"); exit(0)

data = pd.read_csv(sys.argv[1])

data = data.groupby(['PROCESS_COUNT', 'ROW_COUNT']).aggregate(
    MEAN_TIME_MS=('TIME_MS', 'mean'),
).unstack('PROCESS_COUNT')

#data = data.reset_index()
#data = data.set_index('PROCESS_COUNT')

data.columns = data.columns.droplevel()

print(data)

data.to_json(sys.argv[2], orient='split')
exit(0)

data.to_csv(sys.argv[2])