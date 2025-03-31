EXEC_FILE="./a.out"
PYTHON_EXEC=python3
MODES="ROW_ORDER COL_ORDER COL_ORDER_ALT"
MAT_SIDE=10
MAX_SIDE=100000000 #10^8
NUM_SAMPLES=10
STEP=5
GROWTH_FACTOR=0.5
OUTPUT_FILE=$1
STATUS=0

if [ -z "$OUTPUT_FILE" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output>"
    exit
fi
echo MODE,MAT_COL,MAT_ROW,TIME_MS > $OUTPUT_FILE

while [ "$MAT_SIDE" -lt "$MAX_SIDE" ]
do
    for MODE in $MODES
    do
        SAMPLE=1
        while [ "$SAMPLE" -le "$NUM_SAMPLES" ]
        do
            echo running "$EXEC_FILE $MODE $MAT_SIDE $MAT_SIDE -printCSV"
            $EXEC_FILE $MODE $MAT_SIDE $MAT_SIDE -printCSV >> $OUTPUT_FILE
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
    MAT_SIDE=$(($MAT_SIDE+$STEP))
    STEP=$($PYTHON_EXEC -c "import math; print(int(max((10**math.log10($MAT_SIDE)//10)*$GROWTH_FACTOR, $STEP)))")
done
