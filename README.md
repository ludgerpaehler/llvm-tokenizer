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

### Building llvm-tokenizer

To build `llvm-tokenizer`, perform the following steps:

1. Create a new directory for the build. `build/` is reccomended as `.gitignore`
is setup to ignore it:

```bash
mkdir build
cd build
```

2. Run the `cmake` configuration command:

```bash
cmake -GNinja ../
```

3. Run the build:

```bash
cmake --build .
```

4. That's it! The `llvm-tokenizer` binary will be in the `build/` folder.

### Running the llvm-tokenizer tests

The docker container contains all the necessary dependencies to run the tests
for `llvm-tokenizer`. If you're doing a build outside of the container image,
you'll need to make sure you have the `lit` python package and `FileCheck`
(usually through a `llvm-dev` package) installed. After building the the
package, you can simply run the following command in the build folder:

```bash
cmake --build . --target check-llvm-tokenizer
```

That will run all the tests present in the `./test` folder.

