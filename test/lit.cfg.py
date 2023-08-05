import lit.formats

config.name = 'llvm-tokenizer'
config.test_format = lit.formats.ShTest(True)

config.suffixes = ['.ll']

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.obj_root, 'test')

config.substitutions.append(('%llvm-tokenizer',
    os.path.join(config.obj_root, 'llvm-tokenizer')))
