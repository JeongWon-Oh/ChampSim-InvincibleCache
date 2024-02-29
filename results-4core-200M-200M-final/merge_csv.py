import os
import csv
import re

# Current directory for output .txt files
output_dir = '.'

# CSV file to store the merged results
csv_file = 'merged_results.csv'

# List of output .txt files
files = [f for f in os.listdir(output_dir) if f.endswith('.txt')]

# Function to extract statistics from a file
def extract_statistics(file_path):
    with open(file_path, 'r') as file:
        content = file.read()

        pre_sim_section, sim_section = content.split("Region of Interest Statistics")

        # Extract the last occurrence of "Simulation Time"
        simulation_time = re.findall(r'Simulation complete .* \(Simulation time: ([\d\s\w]+)\)', pre_sim_section)[-1].strip()

        # Adjust the regular expression to accurately capture the instructions, IPC, and cycles
        ipc_stats = re.findall(r'CPU (\d) cumulative IPC: ([\d\.]+) instructions: (\d+) cycles: (\d+)', sim_section)
        #print(ipc_stats)
        stats = {
            'Simulation Time': simulation_time
        }

        # Processing IPC, instructions, and cycles for each CPU
        for cpu_id, ipc, instructions, cycles in ipc_stats:
            prefix = f'CPU{cpu_id} '
            stats[prefix + 'IPC'] = ipc
            stats[prefix + 'Instructions'] = instructions
            stats[prefix + 'Cycles'] = cycles

        # Extract and process LLC stats and invincible related stats
        llc_stats = re.findall(r'LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)', sim_section)
        invincible_stats = re.findall(r'LLC INVINCIBLE ACTIVATED: (\d+) BLOCKED READ: (\d+) BLOCKED WRITE: (\d+) FREED: (\d+)', sim_section)

        if llc_stats:
            for cpu_id, (total_access, hit, miss) in enumerate(llc_stats):
                prefix = f'CPU{cpu_id} LLC '
                stats.update({
                    prefix + 'Total Access': total_access,
                    prefix + 'Hit': hit,
                    prefix + 'Miss': miss
                })

        if invincible_stats:
            invincible_activated, blocked_read, blocked_write, freed = invincible_stats[0]
            stats.update({
                'LLC Invincible Activated': invincible_activated,
                'LLC Blocked Read': blocked_read,
                'LLC Blocked Write': blocked_write,
                'LLC Freed': freed
            })

        return stats

# Collect all statistics in a matrix
stat_matrix = {}

# Process each file and accumulate its statistics
for filename in files:
    file_stats = extract_statistics(os.path.join(output_dir, filename))
    stat_matrix[filename] = file_stats

# Write to CSV
with open(csv_file, mode='w', newline='') as file:
    writer = csv.writer(file)

    # Determine headers based on the collected keys
    headers = ['Benchmark'] + sorted(set(key for stats in stat_matrix.values() for key in stats))
    writer.writerow(headers)

    for filename, stats in stat_matrix.items():
        row = [filename] + [stats.get(header, '') for header in headers[1:]]
        writer.writerow(row)

print(f"Results merged into {csv_file}")
