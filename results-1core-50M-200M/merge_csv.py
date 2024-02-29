import os
import csv
import re

# Current directory for output .txt files
output_dir = '.'

# CSV file to store the merged results
csv_file = 'merged_results.csv'

# List of output .txt files
files = [f for f in os.listdir(output_dir) if f.endswith('.txt')]

# Regular expression patterns for each stat category
stat_patterns = {
    'LLC': r'LLC\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
    'cpu0_DTLB': r'cpu0_DTLB\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
    'cpu0_ITLB': r'cpu0_ITLB\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
    'cpu0_L1D': r'cpu0_L1D\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
    'cpu0_L1I': r'cpu0_L1I\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
    'cpu0_L2C': r'cpu0_L2C\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
    'cpu0_STLB': r'cpu0_STLB\s(TOTAL|LOAD|RFO|WRITE|TRANSLATION|PREFETCH)\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)',
}

# Function to extract statistics from a file
def extract_statistics(file_path):
    with open(file_path, 'r') as file:
        content = file.read()

        # Split content at "=== Simulation ==="
        pre_sim_section, sim_section = content.split("=== Simulation ===")

        # Extract the last occurrence of "Simulation Time"
        simulation_time = re.findall(r'Simulation time: ([\d\s\w]+)\)', pre_sim_section)[-1].strip()

        # Extract specific statistics from simulation section
        ipc = re.search(r'cumulative IPC: ([\d\.]+)', sim_section).group(1).strip()
        instructions = re.search(r'instructions: ([\d]+)', sim_section).group(1).strip()
        cycles = re.search(r'cycles: ([\d]+)', sim_section).group(1).strip()
        inva = re.search(r'ACTIVATED: ([\d]+)', sim_section).group(1).strip()
        invbr = re.search(r'BLOCKED READ: ([\d]+)', sim_section).group(1).strip()
        invbw = re.search(r'BLOCKED WRITE: ([\d]+)', sim_section).group(1).strip()
        invf = re.search(r'FREED: ([\d]+)', sim_section).group(1).strip()

        # Extract detailed cache statistics
        stats = {'IPC': ipc, 'Instructions': instructions, 'Cycles': cycles, 'Simulation Time': simulation_time, 'Invincible Activated': inva, 'Blocked Read': invbr, 'Blocked Write': invbw, 'Invincible Freed': invf}
        for category, pattern in stat_patterns.items():
            matches = re.findall(pattern, sim_section)
            for match in matches:
                stat_type = match[0]  # TOTAL, LOAD, RFO, etc.
                stats[f'{category} {stat_type} Access'] = match[1]
                stats[f'{category} {stat_type} Hit'] = match[2]
                stats[f'{category} {stat_type} Miss'] = match[3]

        return stats

# Collect all statistics in a matrix
stat_matrix = {}

# Process each file and accumulate its statistics
for filename in files:
    file_stats = extract_statistics(os.path.join(output_dir, filename))
    for stat_name, stat_value in file_stats.items():
        if stat_name not in stat_matrix:
            stat_matrix[stat_name] = []
        stat_matrix[stat_name].append(stat_value)

# Write to CSV
with open(csv_file, mode='w', newline='') as file:
    writer = csv.writer(file)
    
    # Ordering headers with 'Simulation Time' earlier
    ordered_headers = ['Benchmark', 'IPC', 'Instructions', 'Cycles', 'Simulation Time']
    additional_headers = sorted(set(stat_matrix.keys()) - set(ordered_headers))
    headers = ordered_headers + additional_headers
    writer.writerow(headers)

    for filename in files:
        row = [filename]
        for stat in headers[1:]:
            row.append(stat_matrix[stat][files.index(filename)])
        writer.writerow(row)

print(f"Results merged into {csv_file}")
