# llvm-tokenizer

This is a intended to be a configurable tool for tokenizing LLVM IR.

### Building the container image

Building the development container image can be done with the following steps:

1. (Optional) If you have a custom certificate that is necessary to access
content behind a proxy, you can start by copying over the `*.crt` file to the
root directory of the repository:

```bash
cp /path/to/certificate.crt ./additional_cert.crt
```

2. Build the container image. In the simplest case, the command looks like this:

```bash
docker build -t llvm-tokenizer .
```

If you have a weird SSL setup and need to do something like provide a custom
certificate, you can specify the `CUSTOM_CERT` and `ENABLE_LEGACY_RENEGOTIATION`
flags as follows:

```bash
docker build -t llvm-tokenizer \
  --build-arg="CUSTOM_CERT=./additional_cert.crt" \
  --build-arg="ENABLE_LEGACY_RENEGOTIATION=ON" .
```

### Building llvm-tokenizer natively

`llvm-tokenizer` builds against any installed LLVM (currently tested with LLVM 19
and 22). You need: a C++17 compiler, `cmake`, `ninja`, `libzstd-dev`, `lit`
(`pip install lit` or `uv tool install lit`), and an LLVM development package
(`llvm-19-dev` / `llvm-22-dev`, which also provides `FileCheck`).

1. Configure, selecting the LLVM you want to build against (required when more
   than one is installed):

```bash
cmake -G Ninja -S . -B build -DLLVM_DIR=$(llvm-config-19 --cmakedir)
```

2. Build:

```bash
cmake --build build
```

The `llvm-tokenizer` binary will be in `build/`.

### Running the tests

```bash
cmake --build build --target check-llvm-tokenizer
```

The tests detect the LLVM major version they were built against and check the
matching expected token values, so the suite passes against each supported LLVM.

