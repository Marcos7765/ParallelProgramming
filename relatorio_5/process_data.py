import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

if len(sys.argv) < 4: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <img_output_suffix> <output_suffix> [-basic]"); exit(0)
if len(sys.argv) > 5: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <img_output_suffix> <output_suffix> [-basic]"); exit(0)

data = pd.read_csv(sys.argv[1])

if len(sys.argv) == 5:
    if sys.argv[4] == "-basic":
        data = data[(data['MAX_NUMBER']==10) | (data['MAX_NUMBER']==50)]
        data = data[data['NUM_THREADS'] <= 6]
        data = data.groupby(['MODE', 'MAX_NUMBER', 'NUM_THREADS']).aggregate(
            {
                "PRIME_COUNT": ['mean', 'var'],
                "TIME_MS": 'mean'
            }
        ).unstack('NUM_THREADS')

        data.columns = data.columns.swaplevel(0,2)
        data.columns = data.columns.swaplevel(1,2)

        data = data.sort_index(axis=1)#.reset_index(level='')

        data.to_csv(sys.argv[3], float_format="%.6g")

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
                
        merge_indexes = [0, 1]
        for i,line in enumerate(open("sys.argv[3]").readlines()):
            print(csv_line_to_asciidoc(line, merge=i in merge_indexes))
        exit(0)
    else:
        print(f"Invalid parameter: {sys.argv[4]}")
        exit(1)



data = data.drop(columns=['PRIME_COUNT']).groupby(['MODE', 'MAX_NUMBER', 'NUM_THREADS'], as_index=False).mean()
size = (16,9)
fig, ax = plt.subplots(figsize=size)

thread_vals = pd.unique(data['NUM_THREADS'])

def make_plot(data):

    comp_plots_data = {num_threads:{} for num_threads in thread_vals}

    for num_threads in thread_vals:
        filtered = data[data['NUM_THREADS']==num_threads]
        #comp_plots_data[num_threads]["div_scale"] = filtered['TIME_MS'].max()
        for mode in pd.unique(filtered['MODE']):
            filtered_2 = filtered[filtered['MODE']==mode]
            if len(filtered_2) == 0: print(f"Empty for {num_threads} and {mode}!"); continue
    
            comp_plots_data[num_threads][mode] = {"x": [x for x in filtered_2['MAX_NUMBER']],
                "y": [x for x in filtered_2['TIME_MS']]}

    counter = 0
    for num_threads in comp_plots_data:
        fig, ax = plt.subplots(figsize=size)
        minval_x = float("inf")
        maxval_x = -float("inf")
        for mode in comp_plots_data[num_threads]:
            ax.plot(comp_plots_data[num_threads][mode]["x"], comp_plots_data[num_threads][mode]["y"],
                    label=f"{mode}", linestyle=":" if counter%2 == 0 else "-.")
            minval_x = min(minval_x, min(comp_plots_data[num_threads][mode]['x']))
            maxval_x = max(maxval_x, max(comp_plots_data[num_threads][mode]['x']))
            counter +=1
        ax.set_xlabel('N')
        ax.set_ylabel('Mean time (ms)')
        ax.set_title("5 samples per N, ran on a AMD Ryzen 5 7600, compiled with -Ofast")
        ax.legend()
        ax.set_xlim(left=minval_x, right=maxval_x)
        fig.suptitle(f"Timings for {num_threads} threads")
        plt.tight_layout()
        fig.savefig(f"{num_threads}_{sys.argv[2]}")
        pd.DataFrame(comp_plots_data[num_threads]).to_json(f"{num_threads}_{sys.argv[3]}")

make_plot(data)