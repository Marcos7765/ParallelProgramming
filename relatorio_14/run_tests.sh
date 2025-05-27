EXEC_FILE="mpiexec -n 2 ./a.out"
PYTHON_EXEC=python3

MAX_EXP=30
EXPONENT_VALUES=`seq 0 $MAX_EXP`
NUM_SAMPLES=30

OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi

echo BYTESIZE,TIME_MS > $OUTPUT_FILE

SAMPLE=1
while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
do
    for EXPONENT in $EXPONENT_VALUES
    do
        PROBLEM_SIZE=`$PYTHON_EXEC -c "print(2**$EXPONENT)"`
        echo running "$EXEC_FILE $PROBLEM_SIZE >> $OUTPUT_FILE"
        $EXEC_FILE $PROBLEM_SIZE >> $OUTPUT_FILE
        STATUS=$?
        if [ "$STATUS" -ne 0 ]
        then
            echo Some error occurred, stopping early.
            exit
        fi
    done
    SAMPLE=$(($SAMPLE+1))
done