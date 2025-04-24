EXEC_FILE="./a.out"
PYTHON_EXEC=python3

START_THREADS=1
MAX_THREADS=12
THREAD_VALUES=`seq $START_THREADS $MAX_THREADS` #ive got lazy to rewrite back the while loop
MODES="TASK_PARALLEL TASK_PARALLEL_OPTIMIZED"
PROBLEM_DIR="teste_dirtree_255/"
PROBLEM_SIZE=255
NUM_SAMPLES=30

OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo MODE,NUM_THREADS,ALT_COST,ENTRY_COUNT,TIME_MS > $OUTPUT_FILE


for MODE in $MODES
do
    SAMPLE=1
    while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
    do
        if [ "$MODE" != "SERIAL" ]
        then
            for NUM_THREADS in $THREAD_VALUES
            do
                echo running "$EXEC_FILE $MODE $PROBLEM_DIR $NUM_THREADS -printCSV >> $OUTPUT_FILE"
                $EXEC_FILE $MODE $PROBLEM_DIR $NUM_THREADS -printCSV >> $OUTPUT_FILE
                STATUS=$?
                if [ "$STATUS" -ne 0 ]
                then
                    #intended for when the MAT_SIDE is too big for the program to allocate this much memory
                    echo Some error occurred, stopping early.
                    exit
                fi
            done
        else
            echo running "$EXEC_FILE $MODE $PROBLEM_DIR 1 -printCSV >> $OUTPUT_FILE"
            $EXEC_FILE $MODE $PROBLEM_DIR 1 -printCSV >> $OUTPUT_FILE
            STATUS=$?
            if [ "$STATUS" -ne 0 ]
            then
                #intended for when the MAT_SIDE is too big for the program to allocate this much memory
                echo Some error occurred, stopping early.
                exit
            fi
        fi
        SAMPLE=$(($SAMPLE+1))
    done
done