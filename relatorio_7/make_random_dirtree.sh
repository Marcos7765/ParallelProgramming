OUTPUT_PATH=$1
MAX_ENTRIES=$2

BRANCHING_FREQUENCY=0.5
ENTRY_FREQ=0.9

ENTRY_COUNT=0
LEVEL=0
GLOBAL_SEED=`head -c 2 /dev/urandom | od -An -tu2`

TOTAL_COST=0

try_generate(){
    LOCAL_SEED=$(($GLOBAL_SEED+$1))
    awk -v seed="$LOCAL_SEED" -v prob_entry="$ENTRY_FREQ" -v prob_dir="$BRANCHING_FREQUENCY" 'BEGIN {
        srand(seed);
        val1 = rand();
        val2 = rand();
        if (val1 <= prob_entry){
            if (val2 <= prob_dir){
                print "dir";
            } else{
                print "file";
            }
        } else {
            print "nothing";
        }
    }'
}

populate_level() {
    GLOBAL_PROGRESS=$1
    GLOBAL_MAX=$2
    LOCAL_LEVEL=$3
    LOCAL_PATH=$4

    NEXT_LEVEL=$(($LOCAL_LEVEL+1))
    while [ "$GLOBAL_PROGRESS" -lt "$GLOBAL_MAX" ]
    do
        NEXT=`try_generate $GLOBAL_PROGRESS`
        if [ "$NEXT" = "dir" ]
        then
            GLOBAL_PROGRESS=$(($GLOBAL_PROGRESS+1))
            echo mkdir "$LOCAL_PATH/dir_$GLOBAL_PROGRESS"
            mkdir "$LOCAL_PATH/dir_$GLOBAL_PROGRESS"
            
            placeholder=$(populate_level $GLOBAL_PROGRESS $GLOBAL_MAX $NEXT_LEVEL $LOCAL_PATH/dir_$GLOBAL_PROGRESS)
            GLOBAL_PROGRESS=$?
            echo $placeholder
        fi
        if [ "$NEXT" = "file" ]
        then
            GLOBAL_PROGRESS=$(($GLOBAL_PROGRESS+1))
            echo touch "$LOCAL_PATH/file_$GLOBAL_PROGRESS"
            touch "$LOCAL_PATH/file_$GLOBAL_PROGRESS"
        fi
        if [ "$NEXT" = "nothing" ]
        then
            GLOBAL_SEED=$(($GLOBAL_SEED+1))
            if [ "$LOCAL_LEVEL" -gt 0 ]
            then
                echo closing $LOCAL_PATH
                echo $GLOBAL_PROGRESS
                return $GLOBAL_PROGRESS
            fi
            echo rolled nothing at level 0, ignoring
        fi
    done
    return $GLOBAL_PROGRESS
}

if [ -z "$OUTPUT_PATH" ]
then
    echo "Pass the path to an output file, use: $0 <path/to/output> <number_of_elements>"
    exit
fi

if [ -z "$MAX_ENTRIES" ]
then
    echo "Pass the number of elements you want, use: $0 <path/to/output> <number_of_elements>"
    exit
fi

populate_level $ENTRY_COUNT $MAX_ENTRIES 0 $OUTPUT_PATH