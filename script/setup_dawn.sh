#!/usr/bin/env bash

ROOT=$(dirname "$(dirname "$0")")
DIVIDER=$(python3 -c "print('-' * 80)")

echo "$DIVIDER"
echo "SETUP DEPOT_TOOLS"
pushd "$ROOT/dep/depot_tools" || exit 1
  NEW_PATH="$(pwd):$PATH"
  export PATH=$NEW_PATH
popd || exit 1

echo "$DIVIDER"
echo "SETUP DAWN"
pushd "$ROOT/dep/dawn" || exit 1
  cp scripts/standalone.gclient .gclient || exit 1
  gclient sync || exit 1
popd || exit 1
