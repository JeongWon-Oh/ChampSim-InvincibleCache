#!/bin/bash

# Directory containing the traces
TRACE_DIR="/public/champsim/dp3_traces"

# Array of trace files
TRACES=(
	"605.mcf_s-665B.champsimtrace.xz"
    "607.cactuBSSN_s-2421B.champsimtrace.xz"
    "619.lbm_s-4268B.champsimtrace.xz"
    "625.x264_s-18B.champsimtrace.xz"
    "638.imagick_s-10316B.champsimtrace.xz"
    "648.exchange2_s-1699B.champsimtrace.xz"
    "654.roms_s-842B.champsimtrace.xz"
)

# Create results directory if it doesn't exist
# mkdir -p ./results-2core-10re
# mkdir -p ./results-2core-100re
# mkdir -p ./results-2core-1000re
# mkdir -p ./results-2core-default

mkdir -p ./results-2core-1re

# Counter for parallel jobs
MAX_JOBS=21 # Adjust this number based on your system's capability
job_count=0

# Loop through each trace and run the simulation
for i in "${!TRACES[@]}"
do
    for j in "${!TRACES[@]}"
    do
        if [ "$j" -le "$i" ]; then
            continue
        fi

        TRACE1=${TRACES[$i]}
        TRACE2=${TRACES[$j]}
        echo "Running simulation for $TRACE1 $TRACE2"
        # ./bin/2core-10re --warmup-instructions 200000000 --simulation-instructions 200000000 "$TRACE_DIR/$TRACE1" "$TRACE_DIR/$TRACE2"> "./results-2core-10re/${TRACE1%.*}_${TRACE2%.*}.txt" &
        # ./bin/2core-100re --warmup-instructions 200000000 --simulation-instructions 200000000 "$TRACE_DIR/$TRACE1" "$TRACE_DIR/$TRACE2"> "./results-2core-100re/${TRACE1%.*}_${TRACE2%.*}.txt" &
        # ./bin/2core-1000re --warmup-instructions 200000000 --simulation-instructions 200000000 "$TRACE_DIR/$TRACE1" "$TRACE_DIR/$TRACE2"> "./results-2core-1000re/${TRACE1%.*}_${TRACE2%.*}.txt" &
        # ./bin/2core-default --warmup-instructions 200000000 --simulation-instructions 200000000 "$TRACE_DIR/$TRACE1" "$TRACE_DIR/$TRACE2"> "./results-2core-default/${TRACE1%.*}_${TRACE2%.*}.txt" &
        ./bin/2core-1re --warmup-instructions 200000000 --simulation-instructions 200000000 "$TRACE_DIR/$TRACE1" "$TRACE_DIR/$TRACE2"> "./results-2core-1re/${TRACE1%.*}_${TRACE2%.*}.txt" &
        
        # Increment job count and wait if it reaches the max
        ((job_count++))
        if (( job_count >= MAX_JOBS )); then
            wait -n
            ((job_count--))
        fi
    done
done

# Wait for all background jobs to finish
wait
echo "All simulations completed."