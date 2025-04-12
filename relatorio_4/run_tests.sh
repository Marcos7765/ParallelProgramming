EXEC_FILE="./a.out"
PYTHON_EXEC=python3


GENERAL_MODES="COMP_BOUND MEMO_BOUND"
RUN_MODES="SERIAL THREAD"
START_THREADS=1
MAX_THREADS=12


PROBLEM_SCALE=1
MAX_PROBLEM_SCALE=4

comp_scale_problem_size() {
    PROBLEM_SIZE=$($PYTHON_EXEC -c "print(int(10_000*$1))")
}

memo_scale_problem_size() {
    PROBLEM_SIZE=$($PYTHON_EXEC -c "print(int(10_000*(10**$1)))")
}

NUM_SAMPLES=10
OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo MODE,PROBLEM_SCALE,NUM_THREADS,TIME_MS > $OUTPUT_FILE

while [ "$PROBLEM_SCALE" -le "$MAX_PROBLEM_SCALE" ]
do
    for MODE in $GENERAL_MODES
    do
        if [ "$MODE" = "MEMO_BOUND" ]
        then
            memo_scale_problem_size $PROBLEM_SCALE
        else
            comp_scale_problem_size $PROBLEM_SCALE
        fi
        SAMPLE=1
        while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
        do
            for RMODE in $RUN_MODES
            do
                THREADS=$START_THREADS
                TARGET_THREADS=1
                if [ "$RMODE" = "THREAD" ]
                then
                    TARGET_THREADS=$MAX_THREADS
                fi
                while [ "$THREADS" -le "$TARGET_THREADS" ]
                do
                    #tis one of the few places where such dquotes matching would be okay and allowed
                    echo running "$EXEC_FILE "$MODE"_"$RMODE" $PROBLEM_SIZE $THREADS -printCSV"
                    $EXEC_FILE "$MODE"_"$RMODE" $PROBLEM_SIZE $THREADS -printCSV >> $OUTPUT_FILE
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
    PROBLEM_SCALE=$(($PROBLEM_SCALE+1))
done
