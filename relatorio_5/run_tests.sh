EXEC_FILE="./a.out"
PYTHON_EXEC=python3

RUN_MODES="too_lazy_to_remove"
START_THREADS=1
MAX_THREADS=12

#alternating modes manually
GENERAL_MODES="SERIAL NAIVE_PARALLEL"
PROBLEM_SIZE=10
MAX_PROBLEM_SIZE=50

#block 2 - comment/uncomment to change mode
#GENERAL_MODES="TASK_PARALLEL LOOP_PARALLEL_DYNAMIC"
#PROBLEM_SIZE=10
#MAX_PROBLEM_SIZE=100000000 #10^8
#START_THREADS=2

GROWTH_FACTOR=0.5

regular_scale_problem_size() {
    PROBLEM_SIZE=$($PYTHON_EXEC -c "import math; print($PROBLEM_SIZE + int(max((10**math.log10($PROBLEM_SIZE)//10)*$GROWTH_FACTOR, 5)))")
}

NUM_SAMPLES=5
OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo MODE,MAX_NUMBER,NUM_THREADS,PRIME_COUNT,TIME_MS > $OUTPUT_FILE

while [ "$PROBLEM_SIZE" -le "$MAX_PROBLEM_SIZE" ]
do
    for MODE in $GENERAL_MODES
    do
        SAMPLE=1
        while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
        do
            for RMODE in $RUN_MODES
            do
                THREADS=$START_THREADS
                TARGET_THREADS=$MAX_THREADS
                if [ "$MODE" = "SERIAL" ]
                then
                    TARGET_THREADS=1
                fi
                while [ "$THREADS" -le "$TARGET_THREADS" ]
                do
                    #tis one of the few places where such dquotes matching would be okay and allowed
                    echo running "$EXEC_FILE $MODE $PROBLEM_SIZE $THREADS -printCSV >> $OUTPUT_FILE"
                    $EXEC_FILE $MODE $PROBLEM_SIZE $THREADS -printCSV >> $OUTPUT_FILE
                    STATUS=$?
                    if [ "$STATUS" -ne 0 ]
                    then
                        #intended for when the MAT_SIDE is too big for the program to allocate this much memory
                        echo Some error occurred, stopping early.
                        exit
                    fi
                    THREADS=$(($THREADS+1))
                done
            done
            SAMPLE=$(($SAMPLE+1))
        done
    done
    regular_scale_problem_size
done
