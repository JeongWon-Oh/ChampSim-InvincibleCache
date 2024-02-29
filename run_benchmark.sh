#!/bin/bash

# Directory containing the traces
TRACE_DIR="/public/champsim/dp3_traces"

# Array of trace files
TRACES=(
    "600.perlbench_s-210B.champsimtrace.xz"
    "602.gcc_s-734B.champsimtrace.xz"
    "603.bwaves_s-3699B.champsimtrace.xz"
    "605.mcf_s-665B.champsimtrace.xz"
    "607.cactuBSSN_s-2421B.champsimtrace.xz"
    "619.lbm_s-4268B.champsimtrace.xz"
    "620.omnetpp_s-874B.champsimtrace.xz"
    "621.wrf_s-575B.champsimtrace.xz"
    "623.xalancbmk_s-700B.champsimtrace.xz"
    "625.x264_s-18B.champsimtrace.xz"
    "627.cam4_s-573B.champsimtrace.xz"
    "628.pop2_s-17B.champsimtrace.xz"
    "631.deepsjeng_s-928B.champsimtrace.xz"
    "638.imagick_s-10316B.champsimtrace.xz"
    "641.leela_s-800B.champsimtrace.xz"
    "644.nab_s-5853B.champsimtrace.xz"
    "648.exchange2_s-1699B.champsimtrace.xz"
    "649.fotonik3d_s-1176B.champsimtrace.xz"
    "654.roms_s-842B.champsimtrace.xz"
    "657.xz_s-3167B.champsimtrace.xz"
)

# Create results directory if it doesn't exist
mkdir -p ./results-1core-10cycles-200M-200M-final

# Counter for parallel jobs
MAX_JOBS=32 # Adjust this number based on your system's capability
job_count=0

# Loop through each trace and run the simulation
for TRACE in "${TRACES[@]}"
do
    echo "Running simulation for $TRACE"
    ./bin/champsim --warmup-instructions 200000000 --simulation-instructions 200000000 "$TRACE_DIR/$TRACE" > "./results-1core-10cycles-200M-200M-final/${TRACE%.xz}.txt" &
    
    # Increment job count and wait if it reaches the max
    ((job_count++))
    if (( job_count >= MAX_JOBS )); then
        wait -n
        ((job_count--))
    fi
done

# Wait for all background jobs to finish
wait
echo "All simulations completed."
