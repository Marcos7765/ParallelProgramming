EXEC_FILE="./a.out"
PYTHON_EXEC=python3

START_THREADS=10
MAX_THREADS=10
THREAD_VALUES=`seq $START_THREADS $MAX_THREADS` #ive got lazy to rewrite back the while loop
MODES="SIMPLE_NAMED SIMPLE_UNNAMED"
PROBLEM_SIZE=100000
NUM_SAMPLES=100

OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi

echo MODE,NUM_BINS,NUM_THREADS,BINS_TOTAL_SIZE,TIME_MS > $OUTPUT_FILE


for MODE in $MODES
do
    SAMPLE=1
    while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
    do
        for NUM_THREADS in $THREAD_VALUES
        do
            echo running "$EXEC_FILE $MODE $PROBLEM_SIZE $NUM_THREADS -printCSV >> $OUTPUT_FILE"
            $EXEC_FILE $MODE $PROBLEM_SIZE $NUM_THREADS -printCSV >> $OUTPUT_FILE
            STATUS=$?
            if [ "$STATUS" -ne 0 ]
            then
                echo Some error occurred, stopping early.
                exit
            fi
        done
        SAMPLE=$(($SAMPLE+1))
    done
done