EXEC_FILE="./a.out"
PYTHON_EXEC=python3

NUM_SAMPLES=5
MAX_PROCESSES=6
PROCESS_VALUES=`seq 2 $MAX_PROCESSES`

INITIAL_PROBLEM_SIZE=50
MAX_PROBLEM_SIZE=50000

OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi

echo PROCESS_COUNT,ROW_COUNT,COL_COUNT,TIME_MS > $OUTPUT_FILE

SAMPLE=1
while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
do
    PROBLEM_SIZE=$INITIAL_PROBLEM_SIZE
    while [ "$PROBLEM_SIZE" -le "$MAX_PROBLEM_SIZE" ]
    do
        for PROCESS_COUNT in $PROCESS_VALUES
        do
                echo running "mpiexec -n $PROCESS_COUNT $EXEC_FILE $MODE $PROBLEM_SIZE $PROBLEM_SIZE >> $OUTPUT_FILE"
                mpiexec -n $PROCESS_COUNT $EXEC_FILE $MODE $PROBLEM_SIZE $PROBLEM_SIZE >> $OUTPUT_FILE
                STATUS=$?
                if [ "$STATUS" -ne 0 ]
                then
                    echo Some error occurred, stopping early.
                    exit
                fi
        done
        PROBLEM_SIZE=`$PYTHON_EXEC -c "import math;print($PROBLEM_SIZE + int(50*math.log10($PROBLEM_SIZE)))"`
    done
    SAMPLE=$(($SAMPLE+1))
done