EXEC_FILE="./a.out"
PYTHON_EXEC=python3
INPUT_SIZE=1
MAX_SIZE=1000 #10^8
NUM_SAMPLES=5
STEP=1
GROWTH_FACTOR=0.5
OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo NUM_TERMS,DECIMAL_PLACES_PRECISION,DECIMAL_PLACES_VAR,TIME_MS > $OUTPUT_FILE

while [ "$INPUT_SIZE" -lt "$MAX_SIZE" ]
do
    SAMPLE=1
    while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
    do
        echo running "$EXEC_FILE $INPUT_SIZE -printCSV"
        $EXEC_FILE $INPUT_SIZE -printCSV >> $OUTPUT_FILE
        STATUS=$?
        if [ "$STATUS" -ne 0 ]
        then
            if [ "$STATUS" -eq 7 ]
            then
                echo Reached maximum decimal places for the given precision
                exit
            fi
            #intended for when the MAT_SIDE is too big for the program to allocate this much memory
            echo Some error occurred, stopping early.
            exit
        fi
        SAMPLE=$(($SAMPLE+1))
    done
    INPUT_SIZE=$(($INPUT_SIZE+$STEP))
done
