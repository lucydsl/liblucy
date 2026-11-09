#!/bin/bash

dir=$(dirname "$0")

ret=0
input="$dir/input.lucy"
expected="$dir/expected.js"

# Before
tmp="$(mktemp -d)/out.js"

# Run compiler
$LUCYC --remote-imports --out-file $tmp $input

COLORDIFF=colordiff
command -v colordiff &> /dev/null || COLORDIFF=cat
d=$(diff $expected $tmp | $COLORDIFF)

if [ ${#d} -ge 1 ]; then
  echo -e "${red}FAILED${nc} - $input"
  echo ""
  echo "$d"

  ret=1
fi

exit $ret