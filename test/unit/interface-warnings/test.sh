#!/bin/bash

# The warning about a machine that spawns or invokes a machine of the same
# file that needs options, which nothing supplies to it: once for each such
# machine. The snapshot tests can't hold these: their inputs compile without
# warnings, as the wasm tests compare the compiler's output alone.

dir=$(dirname "$0")
ret=0

check() {
  local expected=$1
  shift
  local actual
  actual=$($LUCYC "$@" "$dir/input.lucy" 2>&1)
  if [ "$actual" != "$(cat "$dir/$expected")" ]; then
    echo "FAILED - $dir/$expected"
    diff "$dir/$expected" <(echo "$actual")
    ret=1
  fi
}

check expected.js

exit $ret
