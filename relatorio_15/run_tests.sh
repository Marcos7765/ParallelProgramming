EXEC_FILE="./a.out"
PYTHON_EXEC=python3

MAX_HEAT=10
NUM_SAMPLES=30
MAX_PROCESSES=6
PROCESS_VALUES=`seq 2 $MAX_PROCESSES`

PROBLEM_SIZES=500

MODES="BLOCKING INSTANT_WAIT INSTANT_TEST"

NUM_ITER=100

OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi

echo MODE,PROCESS_COUNT,BARSIZE,NUM_ITER,TIME_MS > $OUTPUT_FILE

SAMPLE=1
while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
do
    for PROBLEM_SIZE in $PROBLEM_SIZES
    do
        for PROCESS_COUNT in $PROCESS_VALUES
        do
            for MODE in $MODES
            do
                echo running "mpiexec -n $PROCESS_COUNT $EXEC_FILE $MODE $PROBLEM_SIZE $MAX_HEAT $NUM_ITER >> $OUTPUT_FILE"
                mpiexec -n $PROCESS_COUNT $EXEC_FILE $MODE $PROBLEM_SIZE $MAX_HEAT $NUM_ITER >> $OUTPUT_FILE
                STATUS=$?
                if [ "$STATUS" -ne 0 ]
                then
                    echo Some error occurred, stopping early.
                    exit
                fi
            done
        done
    done
    SAMPLE=$(($SAMPLE+1))
done