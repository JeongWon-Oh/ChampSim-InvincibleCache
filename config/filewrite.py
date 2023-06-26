#    Copyright 2023 The ChampSim Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import difflib
import hashlib
import itertools
import operator
import os
import json

from .makefile import get_makefile_lines
from .instantiation_file import get_instantiation_lines
from .constants_file import get_constants_file
from . import modules
from . import util

makefile_file_name = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), '_configuration.mk')

cxx_generated_warning = (
'/***',
' * THIS FILE IS AUTOMATICALLY GENERATED',
' * Do not edit this file. It will be overwritten when the configure script is run.',
' ***/',
''
)

make_generated_warning = (
'###',
'# THIS FILE IS AUTOMATICALLY GENERATED',
'# Do not edit this file. It will be overwritten when the configure script is run.',
'###',
''
)

def files_are_different(rfp, new_rfp):
    ''' Determine if the two files are different, excluding whitespace at the beginning or end of lines '''
    old_file_lines = list(l.strip() for l in rfp)
    new_file_lines = list(l.strip() for l in new_rfp)
    return difflib.SequenceMatcher(a=old_file_lines, b=new_file_lines).ratio() < 1

def write_if_different(fname, new_file_string):
    '''
    Write to a file if and only if it differs from an existing file with the same name.

    :param fname: the name of the destination file
    :param new_file_string: the desired contents of the file
    '''
    should_write = True
    if os.path.exists(fname):
        with open(fname, 'rt') as rfp:
            should_write = files_are_different(rfp, new_file_string.splitlines())

    if should_write:
        with open(fname, 'wt') as wfp:
            wfp.write(new_file_string)

def generate_module_information(containing_dir, module_info):
    ''' Generates all of the include-files with module information '''
    # Core modules file
    core_declarations, core_definitions = modules.get_ooo_cpu_module_lines(module_info['branch'], module_info['btb'])

    # Cache modules file
    cache_declarations, cache_definitions = modules.get_cache_module_lines(module_info['pref'], module_info['repl'])

    yield os.path.join(containing_dir, 'ooo_cpu_module_decl.inc'), core_declarations
    yield os.path.join(containing_dir, 'ooo_cpu_module_def.inc'), core_definitions
    yield os.path.join(containing_dir, 'cache_module_decl.inc'), cache_declarations
    yield os.path.join(containing_dir, 'cache_module_def.inc'), cache_definitions

    joined_info_items = itertools.chain(*(v.items() for v in module_info.values()))
    for k,v in joined_info_items:
        fname = os.path.join(containing_dir, k, 'config.options')
        yield fname, modules.get_module_opts_lines(v)

def generate_build_information(inc_dir, config_flags):
    ''' Generates all of the build-level include-files module '''
    fname = os.path.join(inc_dir, 'config.options')
    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    global_inc_dir = os.path.join(champsim_root, 'inc')

    vcpkg_dir = os.path.join(champsim_root, 'vcpkg_installed')
    vcpkg_triplet = next(filter(lambda triplet: triplet != 'vcpkg', os.listdir(vcpkg_dir)), None)
    vcpkg_inc_dir = os.path.join(vcpkg_dir, vcpkg_triplet, 'include')

    yield fname, itertools.chain((f'-I{inc_dir}',f'-I{global_inc_dir}',f'-isystem {vcpkg_inc_dir}'), config_flags)

class FileWriter:
    def __init__(self, bindir_name=None, objdir_name=None):
        champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        core_sources = os.path.join(champsim_root, 'src')

        self.fileparts = []
        self.bindir_name = bindir_name
        self.core_sources = core_sources
        self.objdir_name = objdir_name

    def __enter__(self):
        self.fileparts = []
        return self

    def write_files(self, parsed_config, bindir_name=None, srcdir_names=None, objdir_name=None):
        '''
        Examines the given config and prepares to write the needed files.

        :param parsed_config: the result of parsing a configuration file
        :param bindir_name: the directory in which to place the binaries
        :param srcdir_name: the directory to search for source files
        :param objdir_name: the directory to place object files
        '''
        build_id = hashlib.shake_128(json.dumps(parsed_config).encode('utf-8')).hexdigest(4)

        local_bindir_name = bindir_name or self.bindir_name
        local_srcdir_names = (*(srcdir_names or []), self.core_sources)
        local_objdir_name = os.path.abspath(os.path.join(objdir_name or self.objdir_name, build_id))

        inc_dir = os.path.join(local_objdir_name, 'inc')

        executable, elements, modules_to_compile, module_info, config_file, env = parsed_config

        joined_module_info = util.subdict(util.chain(*module_info.values()), modules_to_compile) # remove module type tag
        makefile_lines = get_makefile_lines(local_objdir_name, build_id, os.path.join(local_bindir_name, executable), local_srcdir_names, joined_module_info)

        self.fileparts.extend((
            # Instantiation file
            (os.path.join(inc_dir, 'core_inst.inc'), get_instantiation_lines(**elements)),

            # Constants header
            (os.path.join(inc_dir, 'champsim_constants.h'), get_constants_file(config_file, elements['pmem'])),

            # Module name mangling
            *generate_module_information(inc_dir, module_info),

            # Build-level compile flags
            *generate_build_information(inc_dir, util.subdict(env, ('CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS')).values()),

            # Makefile generation
            (makefile_file_name, makefile_lines)
        ))

    def finish(self):
        ''' Write out all prepared files '''
        get_fname = operator.itemgetter(0)
        grouped_fileparts = sorted(self.fileparts, key=get_fname)
        grouped_fileparts = itertools.groupby(grouped_fileparts, key=get_fname)
        grouped_fileparts = ((name, itertools.chain(*(f[1] for f in contents))) for name, contents in grouped_fileparts)
        for fname, fcontents in grouped_fileparts:
            os.makedirs(os.path.abspath(os.path.dirname(fname)), exist_ok=True)
            if os.path.splitext(fname)[1] in ('.cc', '.h', '.inc'):
                write_if_different(fname, '\n'.join((*cxx_generated_warning, *fcontents)))
            elif os.path.splitext(fname)[1] in ('.mk',):
                write_if_different(fname, '\n'.join((*make_generated_warning, *fcontents)))
            else:
                write_if_different(fname, '\n'.join(tuple(fcontents))) # no header

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type is None:
            self.finish()
