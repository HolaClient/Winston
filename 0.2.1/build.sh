#!/bin/bash
set -e

npm install -g @napi-rs/cli

rm -rf target

RUST_BACKTRACE=1 cargo build --release

if [[ "$OSTYPE" == "darwin"* ]]; then
    cp target/release/libhcw.dylib ./hcw.node
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    cp target/release/hcw.dll ./hcw.node
else
    cp target/release/libhcw.so ./hcw.node
fi