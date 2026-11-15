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

# Each has its declarations beside it.
if [ ! -f "$outdir/machine.d.ts" ] || [ ! -f "$outdir/nested/machine.d.ts" ]; then
  exit 1
fi

# --out-file names them after the file, whatever its extension.
$LUCYC --out-file "$outdir/single/toggle.mjs" "$srcdir/machine.lucy"
if [ ! -f "$outdir/single/toggle.mjs" ] || [ ! -f "$outdir/single/toggle.d.mts" ]; then
  exit 1
fi

# --ts writes TypeScript modules, which have their types in them.
rm -rf $outdir
$LUCYC --ts --out-dir $outdir $srcdir
if [ ! -f "$outdir/machine.ts" ] || [ ! -f "$outdir/nested/machine.ts" ]; then
  exit 1
fi
if [ -f "$outdir/machine.js" ] || [ -f "$outdir/machine.d.ts" ]; then
  exit 1
fi

exit 0