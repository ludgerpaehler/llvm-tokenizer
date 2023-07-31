ARG LLVM_VERSION=16
ARG CUSTOM_CERT
ARG ENABLE_LEGACY_RENEGOTIATION

FROM ubuntu:22.04

ARG LLVM_VERSION
ARG CUSTOM_CERT
ARG ENABLE_LEGACY_RENEGOTIATION

RUN apt-get -q update \
    && apt-get install -y \
    curl \
    git \
    vim \
    gnupg2 \
    ca-certificates \
    software-properties-common \
    cmake \
    gcc \
    g++ \
    ninja-build \
    zlib1g-dev

COPY README.md $CUSTOM_CERT /usr/local/share/ca-certificates/
RUN rm /usr/local/share/ca-certificates/README.md \
  && update-ca-certificates
RUN if [ -n "$ENABLE_LEGACY_RENEGOTIATION" ]; then echo "Options = UnsafeLegacyRenegotiation" >> /etc/ssl/openssl.cnf ; fi

RUN apt-get -q update \
    && curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key|apt-key add - \
    && apt-add-repository "deb http://apt.llvm.org/`lsb_release -cs`/ llvm-toolchain-`lsb_release -cs`-$LLVM_VERSION main" || true \
    && apt-get -q update \
    && apt-get install -y llvm-$LLVM_VERSION llvm-$LLVM_VERSION-dev

