#!/usr/bin/env python3
import json
import sys,os
import itertools
import functools
import operator
import copy
from collections import ChainMap

import config.modules as modules

constants_header_name = 'inc/champsim_constants.h'
instantiation_file_name = 'src/core_inst.cc'

###
# Begin format strings
###

cache_fmtstr = 'CACHE {name}("{name}", {frequency}, {fill_level}, {sets}, {ways}, {wq_size}, {rq_size}, {pq_size}, {mshr_size}, {hit_latency}, {fill_latency}, {max_read}, {max_write}, {offset_bits}, {prefetch_as_load:b}, {wq_check_full_addr:b}, {virtual_prefetch:b}, &{lower_level}, {pref_enum_string}, {repl_enum_string});\n'
ptw_fmtstr = 'PageTableWalker {name}("{name}", {cpu}, {fill_level}, {pscl5_set}, {pscl5_way}, {pscl4_set}, {pscl4_way}, {pscl3_set}, {pscl3_way}, {pscl2_set}, {pscl2_way}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, 0, &{lower_level});\n'

cpu_fmtstr = 'O3_CPU {name}({index}, {frequency}, {DIB[sets]}, {DIB[ways]}, {DIB[window_size]}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{ITLB}, &{DTLB}, &{L1I}, &{L1D}, {branch_enum_string}, {btb_enum_string});\n'

pmem_fmtstr = 'MEMORY_CONTROLLER {attrs[name]}({attrs[frequency]});\n'
vmem_fmtstr = 'VirtualMemory vmem({attrs[size]}, 1 << 12, {attrs[num_levels]}, 1, {attrs[minor_fault_penalty]});\n'

###
# Begin default core model definition
###

default_root = { 'executable_name': 'bin/champsim', 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1, 'DIB': {}, 'L1I': {}, 'L1D': {}, 'L2C': {}, 'ITLB': {}, 'DTLB': {}, 'STLB': {}, 'LLC': {}, 'physical_memory': {}, 'virtual_memory': {}}

# Read the config file
if len(sys.argv) >= 2:
    with open(sys.argv[1]) as rfp:
        config_file = ChainMap(json.load(rfp), default_root)
else:
    print("No configuration specified. Building default ChampSim with no prefetching.")
    config_file = ChainMap(default_root)

default_core = { 'frequency' : 4000, 'ifetch_buffer_size': 64, 'decode_buffer_size': 32, 'dispatch_buffer_size': 32, 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'dispatch_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 1, 'dispatch_latency' : 1, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal', 'btb': 'basic_btb' }
default_dib  = { 'window_size': 16,'sets': 32, 'ways': 8 }
default_l1i  = { 'sets': 64, 'ways': 8, 'rq_size': 64, 'wq_size': 64, 'pq_size': 32, 'mshr_size': 8, 'latency': 4, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no_instr', 'replacement': 'lru'}
default_l1d  = { 'sets': 64, 'ways': 12, 'rq_size': 64, 'wq_size': 64, 'pq_size': 8, 'mshr_size': 16, 'latency': 5, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_l2c  = { 'sets': 1024, 'ways': 8, 'rq_size': 32, 'wq_size': 32, 'pq_size': 16, 'mshr_size': 32, 'latency': 10, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_itlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_dtlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_stlb = { 'sets': 128, 'ways': 12, 'rq_size': 32, 'wq_size': 32, 'pq_size': 0, 'mshr_size': 16, 'latency': 8, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_llc  = { 'sets': 2048*config_file['num_cores'], 'ways': 16, 'rq_size': 32*config_file['num_cores'], 'wq_size': 32*config_file['num_cores'], 'pq_size': 32*config_file['num_cores'], 'mshr_size': 64*config_file['num_cores'], 'latency': 20, 'fill_latency': 1, 'max_read': config_file['num_cores'], 'max_write': config_file['num_cores'], 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru', 'name': 'LLC', 'lower_level': 'DRAM' }
default_pmem = { 'name': 'DRAM', 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'lines_per_column': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5, 'turn_around_time': 7.5 }
default_vmem = { 'size': 8589934592, 'num_levels': 5, 'minor_fault_penalty': 200 }
default_ptw = { 'pscl5_set' : 1, 'pscl5_way' : 2, 'pscl4_set' : 1, 'pscl4_way': 4, 'pscl3_set' : 2, 'pscl3_way' : 4, 'pscl2_set' : 4, 'pscl2_way': 8, 'ptw_rq_size': 16, 'ptw_mshr_size': 5, 'ptw_max_read': 2, 'ptw_max_write': 2}

###
# Ensure directories are present
###

os.makedirs(os.path.dirname(config_file['executable_name']), exist_ok=True)
os.makedirs(os.path.dirname(instantiation_file_name), exist_ok=True)
os.makedirs(os.path.dirname(constants_header_name), exist_ok=True)
os.makedirs('obj', exist_ok=True)

###
# Establish default optional values
###

config_file['physical_memory'] = ChainMap(config_file['physical_memory'], default_pmem.copy())
config_file['virtual_memory'] = ChainMap(config_file['virtual_memory'], default_vmem.copy())

cores = config_file.get('ooo_cpu', [{}])

# Index the cache array by names
caches = {c['name']: c for c in config_file.get('cache',[])}

# Default branch predictor and BTB
for i in range(len(cores)):
    cores[i] = ChainMap(cores[i], {'name': 'cpu'+str(i), 'index': i}, copy.deepcopy(dict((k,v) for k,v in config_file.items() if k not in ('ooo_cpu', 'cache'))), default_core.copy())
    cores[i]['DIB'] = ChainMap(cores[i]['DIB'], config_file['DIB'].copy(), default_dib.copy())

# Copy or trim cores as necessary to fill out the specified number of cores
original_size = len(cores)
if original_size <= config_file['num_cores']:
    for i in range(original_size, config_file['num_cores']):
        cores.append(copy.deepcopy(cores[(i-1) % original_size]))
else:
    cores = cores[:(config_file['num_cores'] - original_size)]

# Append LLC to cache array
# LLC operates at maximum freqency of cores, if not already specified
caches['LLC'] = ChainMap(caches.get('LLC',{}), config_file['LLC'].copy(), {'frequency': max(cpu['frequency'] for cpu in cores)}, default_llc.copy())

# If specified in the core, move definition to cache array
for cpu in cores:
    # Assign defaults that are unique per core
    for cache_name in ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB'):
        if isinstance(cpu[cache_name], dict):
            cpu[cache_name] = ChainMap(cpu[cache_name], {'name': cpu['name'] + '_' + cache_name}, config_file[cache_name].copy())
            caches[cpu[cache_name]['name']] = cpu[cache_name]
            cpu[cache_name] = cpu[cache_name]['name']

# Assign defaults that are unique per core
for cpu in cores:
    cpu['PTW'] = ChainMap(cpu.get('PTW',{}), config_file.get('PTW', {}), {'name': cpu['name'] + '_PTW', 'cpu': cpu['index'], 'frequency': cpu['frequency'], 'lower_level': cpu['L1D']}, default_ptw.copy())
    caches[cpu['L1I']] = ChainMap(caches[cpu['L1I']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C'], '_is_instruction_cache': True}, default_l1i.copy())
    caches[cpu['L1D']] = ChainMap(caches[cpu['L1D']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C']}, default_l1d.copy())
    caches[cpu['ITLB']] = ChainMap(caches[cpu['ITLB']], {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}, default_itlb.copy())
    caches[cpu['DTLB']] = ChainMap(caches[cpu['DTLB']], {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}, default_dtlb.copy())

    # L2C
    cache_name = caches[cpu['L1D']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], {'frequency': cpu['frequency'], 'lower_level': 'LLC'}, default_l2c.copy())

    # STLB
    cache_name = caches[cpu['DTLB']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], {'frequency': cpu['frequency'], 'lower_level': cpu['PTW']['name']}, default_l2c.copy())

    # LLC
    cache_name = caches[caches[cpu['L1D']]['lower_level']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], default_llc.copy())

# Remove caches that are inaccessible
accessible = [False]*len(caches)
for i,ll in enumerate(caches.values()):
    accessible[i] |= any(ul['lower_level'] == ll['name'] for ul in caches.values()) # The cache is accessible from another cache
    accessible[i] |= any(ll['name'] in [cpu['L1I'], cpu['L1D'], cpu['ITLB'], cpu['DTLB']] for cpu in cores) # The cache is accessible from a core
caches = dict(itertools.compress(caches.items(), accessible))

# Establish latencies in caches
for cache in caches.values():
    cache['hit_latency'] = cache.get('hit_latency') or (cache['latency'] - cache['fill_latency'])

# Create prefetch activation masks
type_list = ('LOAD', 'RFO', 'PREFETCH', 'WRITEBACK', 'TRANSLATION')
for cache in caches.values():
    cache['prefetch_activate_mask'] = functools.reduce(operator.or_, (1 << i for i,t in enumerate(type_list) if t in cache['prefetch_activate'].split(',')))

# Scale frequencies
config_file['physical_memory']['io_freq'] = config_file['physical_memory']['frequency'] # Save value
freqs = list(itertools.chain(
    [cpu['frequency'] for cpu in cores],
    [cache['frequency'] for cache in caches.values()],
    (config_file['physical_memory']['frequency'],)
))
freqs = [max(freqs)/x for x in freqs]
for freq,src in zip(freqs, itertools.chain(cores, caches.values(), (config_file['physical_memory'],))):
    src['frequency'] = freq

# TLBs use page offsets, Caches use block offsets
for cpu in cores:
    cache_name = cpu['ITLB']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_PAGE_SIZE'
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['DTLB']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_PAGE_SIZE'
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['L1I']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_BLOCK_SIZE'
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['L1D']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_BLOCK_SIZE'
        cache_name = caches[cache_name]['lower_level']

# Try the local module directories, then try to interpret as a path
def default_dir(dirname, f):
    fname = os.path.join(dirname, f)
    if not os.path.exists(fname):
        fname = os.path.relpath(os.path.expandvars(os.path.expanduser(f)))
    if not os.path.exists(fname):
        print('Path "' + fname + '" does not exist. Exiting...')
        sys.exit(1)
    return fname

def wrap_list(attr):
    if not isinstance(attr, list):
        attr = [attr]
    return attr

for cache in caches.values():
    cache['replacement'] = [default_dir('replacement', f) for f in wrap_list(cache.get('replacement', []))]
    cache['prefetcher']  = [default_dir('prefetcher', f) for f in wrap_list(cache.get('prefetcher', []))]

for cpu in cores:
    cpu['branch_predictor'] = [default_dir('branch', f) for f in wrap_list(cpu.get('branch_predictor', []))]
    cpu['btb']              = [default_dir('btb', f) for f in wrap_list(cpu.get('btb', []))]

###
# Check to make sure modules exist and they correspond to any already-built modules.
###

repl_data   = {modules.get_module_name(fname): {'fname':fname, **modules.get_repl_data(modules.get_module_name(fname))} for fname in itertools.chain.from_iterable(cache['replacement'] for cache in caches.values())}
pref_data   = {modules.get_module_name(fname): {'fname':fname, **modules.get_pref_data(modules.get_module_name(fname),is_instr)} for fname,is_instr in itertools.chain.from_iterable(zip(cache['prefetcher'], itertools.repeat(cache.get('_is_instruction_cache',False))) for cache in caches.values())}
branch_data = {modules.get_module_name(fname): {'fname':fname, **modules.get_branch_data(modules.get_module_name(fname))} for fname in itertools.chain.from_iterable(cpu['branch_predictor'] for cpu in cores)}
btb_data    = {modules.get_module_name(fname): {'fname':fname, **modules.get_btb_data(modules.get_module_name(fname))} for fname in itertools.chain.from_iterable(cpu['btb'] for cpu in cores)}

for cpu in cores:
    cpu['branch_predictor'] = [module_name for module_name,data in branch_data.items() if data['fname'] in cpu['branch_predictor']]
    cpu['btb']              = [module_name for module_name,data in btb_data.items() if data['fname'] in cpu['btb']]

for cache in caches.values():
    cache['replacement'] = [module_name for module_name,data in repl_data.items() if data['fname'] in cache['replacement']]
    cache['prefetcher']  = [module_name for module_name,data in pref_data.items() if data['fname'] in cache['prefetcher']]

###
# Perform final preparations for file writing
###

# Add PTW to memory system
ptws = {}
for i in range(len(cores)):
    ptws[cores[i]['PTW']['name']] = cores[i]['PTW']
    cores[i]['PTW'] = cores[i]['PTW']['name']

memory_system = dict(**caches, **ptws)

# Give each element a fill level
active_keys = list(itertools.chain.from_iterable((cpu['ITLB'], cpu['DTLB'], cpu['L1I'], cpu['L1D']) for cpu in cores))
for k in active_keys:
    memory_system[k]['fill_level'] = 1

for fill_level in range(1,len(memory_system)+1):
    for k in active_keys:
        if memory_system[k]['lower_level'] != 'DRAM':
            memory_system[memory_system[k]['lower_level']]['fill_level'] = max(memory_system[memory_system[k]['lower_level']].get('fill_level',0), fill_level+1)
    active_keys = [memory_system[k]['lower_level'] for k in active_keys if memory_system[k]['lower_level'] != 'DRAM']

# Remove name index
memory_system = list(memory_system.values())

memory_system.sort(key=operator.itemgetter('fill_level'), reverse=True)

# Check for lower levels in the array
for i in reversed(range(len(memory_system))):
    ul = memory_system[i]
    if ul['lower_level'] != 'DRAM':
        if not any((ul['lower_level'] == ll['name']) for ll in memory_system[:i]):
            print('Could not find cache "' + ul['lower_level'] + '" in cache array. Exiting...')
            sys.exit(1)

###
# Begin file writing
###

# Instantiation file
with open(instantiation_file_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#include "cache.h"\n')
    wfp.write('#include "champsim.h"\n')
    wfp.write('#include "dram_controller.h"\n')
    wfp.write('#include "ooo_cpu.h"\n')
    wfp.write('#include "ptw.h"\n')
    wfp.write('#include "vmem.h"\n')
    wfp.write('#include "operable.h"\n')
    wfp.write('#include "' + os.path.basename(constants_header_name) + '"\n')
    wfp.write('#include <array>\n')

    wfp.write(vmem_fmtstr.format(attrs=config_file['virtual_memory']))
    wfp.write('\n')
    wfp.write(pmem_fmtstr.format(attrs=config_file['physical_memory']))
    for elem in memory_system:
        if 'pscl5_set' in elem:
            wfp.write(ptw_fmtstr.format(**elem))
        else:
            wfp.write(cache_fmtstr.format(\
                repl_enum_string=' | '.join(f'(1 << CACHE::r{k})' for k in elem['replacement']),\
                pref_enum_string=' | '.join(f'(1 << CACHE::p{k})' for k in elem['prefetcher']),\
                **elem))

    for i,cpu in enumerate(cores):
        wfp.write(cpu_fmtstr.format(\
            branch_enum_string=' | '.join(f'(1 << O3_CPU::b{k})' for k in cpu['branch_predictor']),\
            btb_enum_string=' | '.join(f'(1 << O3_CPU::t{k})' for k in cpu['btb']),
            **cpu))

    wfp.write('std::array<O3_CPU*, NUM_CPUS> ooo_cpu {{\n')
    wfp.write(', '.join('&{name}'.format(**elem) for elem in cores))
    wfp.write('\n}};\n')

    wfp.write('std::array<CACHE*, NUM_CACHES> caches {{\n')
    wfp.write(', '.join('&{name}'.format(**elem) for elem in memory_system if 'pscl5_set' not in elem))
    wfp.write('\n}};\n')

    wfp.write('std::array<champsim::operable*, NUM_OPERABLES> operables {{\n')
    wfp.write(', '.join('&{name}'.format(**elem) for elem in itertools.chain(cores, memory_system, (config_file['physical_memory'],))))
    wfp.write('\n}};\n')

# Core modules file
with open('inc/ooo_cpu_modules.inc', 'wt') as wfp:
    wfp.write(modules.get_branch_string(branch_data))
    wfp.write(modules.get_btb_string(btb_data))

with open('inc/cache_modules.inc', 'wt') as wfp:
    wfp.write(modules.get_repl_string(repl_data))
    wfp.write(modules.get_pref_string(pref_data))

pmem_const_names = {
    # config_file_key : (Type, Needs LOG2?, NAME)
    'io_freq': ('unsigned long', False, 'DRAM_IO_FREQ'),
    'channels': ('unsigned long', True, 'DRAM_CHANNELS'),
    'ranks': ('unsigned long', True, 'DRAM_RANKS'),
    'banks': ('unsigned long', True, 'DRAM_BANKS'),
    'rows': ('unsigned long', True, 'DRAM_ROWS'),
    'columns': ('unsigned long', True, 'DRAM_COLUMNS'),
    'lines_per_column': ('unsigned long', False, 'DRAM_LINES_PER_COLUMN'),
    'channel_width': ('unsigned long', False, 'DRAM_CHANNEL_WIDTH'),
    'wq_size': ('std::size_t', False, 'DRAM_WQ_SIZE'),
    'rq_size': ('std::size_t', False, 'DRAM_RQ_SIZE'),
    'tRP': ('double', False, 'tRP_DRAM_NANOSECONDS'),
    'tRCD': ('double', False, 'tRCD_DRAM_NANOSECONDS'),
    'tCAS': ('double', False, 'tCAS_DRAM_NANOSECONDS'),
    'turn_around_time': ('double', False, 'DBUS_TURN_AROUND_NANOSECONDS')
}

# Constants header
with open(constants_header_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#ifndef CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#define CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#include <cstdlib>\n')
    wfp.write('#include "util.h"\n')
    wfp.write(f"constexpr static unsigned long BLOCK_SIZE = {config_file['block_size']};\n")
    wfp.write('constexpr static auto LOG2_BLOCK_SIZE = lg2(BLOCK_SIZE);\n')
    wfp.write(f"constexpr static unsigned long PAGE_SIZE = {config_file['page_size']};\n")
    wfp.write('constexpr static auto LOG2_PAGE_SIZE = lg2(PAGE_SIZE);\n')
    wfp.write(f"constexpr static unsigned long STAT_PRINTING_PERIOD = {config_file['heartbeat_frequency']};\n")
    wfp.write(f'constexpr static std::size_t NUM_CPUS = {len(cores)};\n')
    wfp.write(f'constexpr static std::size_t NUM_CACHES = {len(caches)};\n')
    wfp.write(f'constexpr static std::size_t NUM_OPERABLES = {len(cores) + len(memory_system) + 1};\n')
    wfp.write(f'constexpr static std::size_t NUM_BRANCH_MODULES = {len(branch_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_BTB_MODULES = {len(btb_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_REPLACEMENT_MODULES = {len(repl_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_PREFETCH_MODULES = {len(pref_data)};\n')

    for k,v in pmem_const_names.items():
        t, l, n = v
        value = config_file['physical_memory'][k]
        wfp.write(f"constexpr static {t} {n} = {value};\n")
        if l:
            wfp.write(f'constexpr static auto LOG2_{n} = lg2({n});\n')

    wfp.write('#endif\n')

# Makefile
module_file_info = tuple( (v['fname'], v['opts']) for k,v in itertools.chain(repl_data.items(), pref_data.items(), branch_data.items(), btb_data.items()) ) # Associate modules with paths
with open('Makefile', 'wt') as wfp:
    wfp.write('CC := ' + config_file.get('CC', 'gcc') + '\n')
    wfp.write('CXX := ' + config_file.get('CXX', 'g++') + '\n')
    #wfp.write('CFLAGS := ' + config_file.get('CFLAGS', '-Wall -O3') + ' -std=gnu99\n')
    wfp.write('CXXFLAGS := ' + config_file.get('CXXFLAGS', '-Wall -O3') + ' -std=c++17\n')
    wfp.write('CPPFLAGS := ' + config_file.get('CPPFLAGS', '') + ' -Iinc -MMD -MP\n')
    wfp.write('LDFLAGS := ' + config_file.get('LDFLAGS', '') + '\n')
    wfp.write('LDLIBS := ' + config_file.get('LDLIBS', '') + '\n')
    wfp.write('\n')
    wfp.write('.phony: all clean\n\n')

    wfp.write('executable_name ?= ' + config_file['executable_name'] + '\n\n')
    wfp.write('all: $(executable_name)\n\n')
    wfp.write('clean: \n')
    wfp.write('\t$(RM) ' + constants_header_name + '\n')
    wfp.write('\t$(RM) ' + instantiation_file_name + '\n')
    wfp.write('\t$(RM) ' + 'inc/cache_modules.inc' + '\n')
    wfp.write('\t$(RM) ' + 'inc/ooo_cpu_modules.inc' + '\n')
    wfp.write('\t find . -name \*.o -delete\n\t find . -name \*.d -delete\n\t $(RM) -r obj\n\n')
    for topdir, _ in module_file_info:
        dir = [topdir]
        for b,dd,files in os.walk(topdir):
            dir.extend(os.path.join(b,d) for d in dd)
        for d in dir:
            wfp.write(f'\t find {d} -name \*.o -delete\n\t find {d} -name \*.d -delete\n')
    wfp.write('\n')

    wfp.write('cxxsources = $(wildcard src/*.cc)\n')
    wfp.write('\n')

    for topdir, opts in module_file_info:
        dir = [topdir]
        for b,dd,files in os.walk(topdir):
            dir.extend(os.path.join(b,d) for d in dd)
            for f in files:
                if os.path.splitext(f)[1] in ['.cc', '.cpp', '.C']:
                    name = os.path.join(b,f)
                    wfp.write(f'cxxsources += {name}\n')
        for d in dir:
            wfp.write(f'{d}/%.o: CPPFLAGS += -I{d}\n')
            wfp.write('\n'.join(f'{d}/%.o: CXXFLAGS += {opt}' for opt in opts))
        wfp.write('\n')
        wfp.write('\n')

    wfp.write('$(executable_name): $(patsubst %.cc,%.o,$(cxxsources))\n')
    wfp.write('\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n')

    wfp.write('-include $(wildcard src/*.d)\n')
    for topdir, _ in module_file_info:
        dir = [topdir]
        for b,dd,files in os.walk(topdir):
            dir.extend(os.path.join(b,d) for d in dd)
        for d in dir:
            wfp.write(f'-include $(wildcard {d}/*.d)\n')
    wfp.write('\n')

# vim: set filetype=python:
