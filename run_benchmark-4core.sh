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
mkdir -p ./results-4core-200M-200M-final

# Counter for parallel jobs
MAX_JOBS=36 # Adjust this number based on your system's capability
job_count=0

# Loop through each trace and run the simulation
for i in "${!TRACES[@]}"
do
    for j in "${!TRACES[@]}"
    do
        for k in "${!TRACES[@]}"
        do
            for l in "${!TRACES[@]}"
            do
                if [ "$j" -le "$i" ] || [ "$k" -le "$j" ] || [ "$l" -le "$k" ]; then
                    continue
                fi

                TRACE1=${TRACES[$i]}
                TRACE2=${TRACES[$j]}
                TRACE3=${TRACES[$k]}
                TRACE4=${TRACES[$l]}
                echo "Running simulation for $TRACE1 $TRACE2 $TRACE3 $TRACE4"
                ./bin/champsim --warmup-instructions 200000000 --simulation-instructions 200000000 \
                    "$TRACE_DIR/$TRACE1" "$TRACE_DIR/$TRACE2" "$TRACE_DIR/$TRACE3" "$TRACE_DIR/$TRACE4" > \
                    "./results-4core-200M-200M-final/${TRACE1%.*}_${TRACE2%.*}_${TRACE3%.*}_${TRACE4%.*}.txt" &
                
                # Increment job count and wait if it reaches the max
                ((job_count++))
                if (( job_count >= MAX_JOBS )); then
                    wait -n
                    ((job_count--))
                fi
            done
        done
    done
done

# Wait for all background jobs to finish
wait
echo "All simulations completed."
