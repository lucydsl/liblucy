#!/bin/bash

dir=$(dirname "$0")

outdir="$dir/test-out"
srcdir="$dir/src"

# Before
rm -rf $outdir

# Run compiler
$LUCYC --out-dir $outdir $srcdir

# Test to see if expected files exist.
if [ ! -f "$outdir/machine.js" ]; then
  exit 1
fi

if [ ! -f "$outdir/nested/machine.js" ]; then
  exit 1
fi

exit 0