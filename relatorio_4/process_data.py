import pandas as pd
import matplotlib.pyplot as plt
import sys

if len(sys.argv) < 4: print(f"Too few arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <json_output_filepath>"); exit(0)
if len(sys.argv) > 4: print(f"Too many arguments, use {sys.argv[0]} <input_filepath> <img_output_filepath> <json_output_filepath>"); exit(0)

data = pd.read_csv(sys.argv[1])

data = data.groupby(['MODE', 'NUM_THREADS', 'PROBLEM_SCALE'], as_index=False).mean()

size = (16,9)
fig, ax = plt.subplots(figsize=size)

comp_bound_data =  data[data['MODE'].str.startswith("COMP_BOUND")]
memo_bound_data =  data[data['MODE'].str.startswith("MEMO_BOUND")]

thread_vals = pd.unique(data['NUM_THREADS'])

lines = []

def make_plot(comp_bound_data):

    comp_plots_data = {pscale:{} for pscale in pd.unique(comp_bound_data['PROBLEM_SCALE'])}

    for pscale in pd.unique(comp_bound_data['PROBLEM_SCALE']):
        filtered = comp_bound_data[comp_bound_data['PROBLEM_SCALE']==pscale]
        comp_plots_data[pscale]["div_scale"] = filtered['TIME_MS'].max()
        for mode in pd.unique(filtered['MODE']):
            filtered_2 = filtered[filtered['MODE']==mode]
            if mode.endswith("SERIAL"):
                comp_plots_data[pscale][mode] = {"x": [i for i in thread_vals], "y": list(filtered_2['TIME_MS']/
                    comp_plots_data[pscale]["div_scale"])*len(thread_vals)}
            else:
                comp_plots_data[pscale][mode] = {"x": [x for x in filtered_2['NUM_THREADS']],
                    "y": [x for x in filtered_2['TIME_MS']/comp_plots_data[pscale]["div_scale"]]}

    colorrange1 = plt.get_cmap('tab10', len(pd.unique(comp_bound_data['PROBLEM_SCALE'])))
    dashstyles = ["-", ":", "-.", "--"]
    markers = ['*', 'o', '+', 'x']

    counter = 0
    for pscale in comp_plots_data:
        for mode in comp_plots_data[pscale]:
            if mode == "div_scale": continue
            if mode.endswith("SERIAL"):
                line, = ax.plot(comp_plots_data[pscale][mode]["x"], 
                    comp_plots_data[pscale][mode]["y"], label=f"{mode} {pscale} scale", color=colorrange1(counter), 
                    linestyle=dashstyles[counter%4])
            else:
                line, = ax.plot(comp_plots_data[pscale][mode]["x"], 
                    comp_plots_data[pscale][mode]["y"], label=f"{mode} {pscale} scale", marker=markers[counter%4],
                    linestyle=dashstyles[counter%4])
            lines.append(line)
        counter +=1
    
    return pd.DataFrame(comp_plots_data)

selected_data = None
match input("Select: COMP or MEMO. "):
    case "COMP":
        selected_data = comp_bound_data
    case "MEMO":
        selected_data = memo_bound_data
    case _:
        print("Invalid selection.")
        exit(1)

make_plot(selected_data).to_json(sys.argv[3])

ax.set_xlabel('NUM_THREADS')
ax.set_ylabel('Normalized per problem scale time')
ax.set_title("10 samples per NUM_THREAD, ran on a AMD Ryzen 5 7600, compiled with -O3")
fig.suptitle("TIme")

linear = True
#linear = False
if linear:
    ax.set_xlim(left=data['NUM_THREADS'].min(), right=data['NUM_THREADS'].max())
    ax.set_xticks([i for i in range(1,13)])
else:
    ax.set_xscale('log')
    ax.set_yscale('log')


lines = sorted(lines, key= lambda a: a.get_label())
ax.legend(lines, [line.get_label() for line in lines])
plt.tight_layout()
fig.savefig(sys.argv[2])