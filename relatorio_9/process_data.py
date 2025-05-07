import pandas as pd
import sys

if len(sys.argv) < 3: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <output_path>"); exit(0)
if len(sys.argv) > 3: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <output_path>"); exit(0)

data = pd.read_csv(sys.argv[1])

data = data.groupby(['MODE', 'NUM_THREADS']).aggregate(
    MEAN_TIME_MS=('TIME_MS', 'mean'),
    DESV_PAD_TIME_MS=('TIME_MS', 'std')
).unstack('NUM_THREADS')

data.columns = data.columns.swaplevel(0,1)
data = data.sort_index(axis=1)#.reset_index(level='')

data.to_csv(sys.argv[2], float_format="%.6g")

#can't differentiate indexes to merge, maybe adding a index list of which lines to merge?
def csv_line_to_asciidoc(_line, sep=",", merge=True):
    line = _line
    if _line.endswith("\n"): line = _line[:-1]
    skip_tuple_token = (None, None)
    new_line = []
    ipair_line = list(enumerate(line.split(sep)))
    for i, word in ipair_line:
        if i is None:
            continue
        rep_count = 0
        if merge or word == '':
            for j, next_word in ipair_line[i+1:]:
                if next_word != word:
                    if next_word != '':
                        break
                rep_count += 1
                ipair_line[j] = skip_tuple_token
        if rep_count == 0:
            new_line.append(f"|{word}\n")
        else:
            new_line.append(f"{rep_count+1}+|{word}\n")
    new_line[-1] = new_line[-1][:-1] + "  \n" 
    return "".join(new_line)
        
merge_indexes = [0]
for i,line in enumerate(open(f"{sys.argv[2]}").readlines()):
    print(csv_line_to_asciidoc(line, merge=i in merge_indexes))