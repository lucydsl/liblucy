#!/bin/bash

dir=$(dirname "$0")

ret=0
input="$dir/input.lucy"
expected="$dir/expected.js"

# Before
tmp=$(mktemp)

# Run compiler
$LUCYC --remote-imports --out-file $tmp $input

d=$(diff $expected $tmp | colordiff)

if [ ${#d} -ge 1 ]; then
  echo -e "${red}FAILED${nc} - $input"
  echo ""
  echo "$d"

  ret=1
fi

exit $ret