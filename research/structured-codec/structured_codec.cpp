// Track B CLI. encode|decode subcommand contract matches the harness:
//   structured-codec encode <input.ll> <tokens>
//   structured-codec decode <tokens> <output.ll>
// M0: only `encode` and `decode` are wired (no flags). Subcommand bodies are
// added in later tasks; this file just dispatches.
#include <cstdio>
#include <cstring>
#include <string>

#include "encoder.h"
#include "decoder.h"

static int usage() {
  std::fprintf(stderr,
               "usage: structured-codec encode <in.ll> <tokens>\n"
               "       structured-codec decode <tokens> <out.ll>\n");
  return 2;
}

int main(int argc, char **argv) {
  if (argc != 4) return usage();
  const std::string cmd = argv[1], in = argv[2], out = argv[3];
  if (cmd == "encode") return structured_codec::encodeFile(in, out);
  if (cmd == "decode") return structured_codec::decodeFile(in, out);
  return usage();
}
