#!/bin/bash

# Directory containing the traces
TRACE_DIR="/public/champsim/dp3_traces"

# List of trace files as mentioned
TRACE_FILES=(
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

# Create results directories if they don't exist
for re_dir in default 1re 10re 100re 1000re; do
    mkdir -p ./results-8core-$re_dir
done

# Read from the mixes file
MIX_FILE="./randomly_generated_mixes.txt"  # Update this if necessary
MAX_JOBS=12
job_count=0

# Loop through each mix in the file
while IFS= read -r line; do
    # Extract trace names from the line (skip "Mix X: ")
    traces=$(echo "$line" | cut -d':' -f2)

    # Convert the extracted trace names into an array
    IFS=' ' read -r -a trace_array <<< "$traces"

    # Skip if the mix does not contain exactly 8 traces
    if [ "${#trace_array[@]}" -ne 8 ]; then
        echo "Skipping: Incorrect number of traces in line: $line"
        continue
    fi

    # Map trace names from the file to TRACE_FILES array
    trace_paths=()
    for trace in "${trace_array[@]}"; do
        for t in "${TRACE_FILES[@]}"; do
            if [[ "$t" == *"$trace"* ]]; then
                trace_paths+=("$TRACE_DIR/$t")
                break
            fi
        done
    done

    # Ensure exactly 8 trace files have been mapped
    if [ "${#trace_paths[@]}" -ne 8 ]; then
        echo "Skipping: Could not map all traces for line: $line"
        continue
    fi

    # Create a unique file identifier based on the trace names
    file_name="${trace_array[*]// /_}"

    # Output 5 variations of the executable commands
    echo "./bin/8core-default --warmup-instructions 200000000 --simulation-instructions 200000000 ${trace_paths[@]} > \"./results-8core-default/$file_name.txt\""
    echo "./bin/8core-allevict-1re --warmup-instructions 200000000 --simulation-instructions 200000000 ${trace_paths[@]} > \"./results-8core-1re/$file_name.txt\""
    echo "./bin/8core-allevict-10re --warmup-instructions 200000000 --simulation-instructions 200000000 ${trace_paths[@]} > \"./results-8core-10re/$file_name.txt\""
    echo "./bin/8core-allevict-100re --warmup-instructions 200000000 --simulation-instructions 200000000 ${trace_paths[@]} > \"./results-8core-100re/$file_name.txt\""
    echo "./bin/8core-allevict-1000re --warmup-instructions 200000000 --simulation-instructions 200000000 ${trace_paths[@]} > \"./results-8core-1000re/$file_name.txt\""

done < "$MIX_FILE" > executable_commands.txt

echo "Executable commands have been written to 'executable_commands.txt'."