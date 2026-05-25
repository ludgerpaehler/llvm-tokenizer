import os

import lit.formats

config.name = 'llvm-tokenizer'
config.test_format = lit.formats.ShTest(True)

config.suffixes = ['.ll']

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.obj_root, 'test')

config.substitutions.append(('%llvm-tokenizer',
    os.path.join(config.obj_root, 'llvm-tokenizer')))

# Make the version-matched LLVM tools (e.g. FileCheck) available to RUN lines.
config.environment['PATH'] = os.pathsep.join(
    filter(None, [config.llvm_tools_dir, config.environment.get('PATH', '')]))

# Expose the LLVM major version so tests can select version-specific expected
# values via --check-prefixes=CHECK,CHECK-%llvm_major.
config.substitutions.append(('%llvm_major', config.llvm_version_major))
