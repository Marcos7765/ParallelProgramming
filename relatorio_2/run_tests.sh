EXEC_FILE="./a.out"
PYTHON_EXEC=python3
MODES="SINGLE BLOCK2 BLOCK4 BLOCK8 BLOCK16"
INPUT_SIZE=10
MAX_SIZE=100000000 #10^8
NUM_SAMPLES=5
STEP=5
GROWTH_FACTOR=0.5
OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo MODE,VEC_SIZE,TIME_MS > $OUTPUT_FILE

while [ "$INPUT_SIZE" -lt "$MAX_SIZE" ]
do
    for MODE in $MODES
    do
        SAMPLE=1
        while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
        do
            echo running "$EXEC_FILE $MODE $INPUT_SIZE -printCSV"
            $EXEC_FILE $MODE $INPUT_SIZE -printCSV >> $OUTPUT_FILE
            STATUS=$?
            if [ "$STATUS" -ne 0 ]
            then
                #intended for when the MAT_SIDE is too big for the program to allocate this much memory
                echo Some error occurred, stopping early.
                exit
            fi
            SAMPLE=$(($SAMPLE+1))
        done
    done
    INPUT_SIZE=$(($INPUT_SIZE+$STEP))
    STEP=$($PYTHON_EXEC -c "import math; print(int(max((10**math.log10($INPUT_SIZE)//10)*$GROWTH_FACTOR, $STEP)))")
done
