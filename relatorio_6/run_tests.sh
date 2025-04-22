EXEC_FILE="./a.out"
PYTHON_EXEC=python3

THREAD_VALUES="1 3 6 12"

#alternating modes manually
MODES="SERIAL NAIVE_PARALLEL PARALLEL"
PROBLEM_SIZE=10000000 #10^8
NUM_SAMPLES=30

OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo MODE,NUM_SAMPLES,NUM_THREADS,ESTIMATED_PI,TIME_MS > $OUTPUT_FILE


for MODE in $MODES
do
    SAMPLE=1
    while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
    do
        if [ "$MODE" != "SERIAL" ]
        then
            for NUM_THREADS in $THREAD_VALUES
            do
                echo running "$EXEC_FILE $MODE $PROBLEM_SIZE $NUM_THREADS -printCSV >> $OUTPUT_FILE"
                $EXEC_FILE $MODE $PROBLEM_SIZE $NUM_THREADS -printCSV >> $OUTPUT_FILE
                STATUS=$?
                if [ "$STATUS" -ne 0 ]
                then
                    #intended for when the MAT_SIDE is too big for the program to allocate this much memory
                    echo Some error occurred, stopping early.
                    exit
                fi
            done
        else
            echo running "$EXEC_FILE $MODE $PROBLEM_SIZE 1 -printCSV >> $OUTPUT_FILE"
            $EXEC_FILE $MODE $PROBLEM_SIZE 1 -printCSV >> $OUTPUT_FILE
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