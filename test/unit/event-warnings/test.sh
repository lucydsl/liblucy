#!/bin/bash

# The warnings about a machine whose states don't keep to the events it
# declares, and the .d.ts for one that handles an event it doesn't declare.
# The snapshot tests can't hold these: their inputs compile without
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
check expected.d.ts --experimental-dts --print dts

exit $ret
