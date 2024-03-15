#!/bin/sh

set -e

if test $# -ne 3; then
  echo "USAGE: $0 <path-to-xnbcli> <path-to-jump-king-dir> <output-dir>"
  exit -1
fi

xnbcli=$1
input_dir="$2/Content"
output_dir=$3

unpack_one() {
  outpath="$output_dir/$(dirname $1)"
  mkdir -p "$outpath"
  $xnbcli unpack "$input_dir/$1" "$outpath"
}

unpack_dir() {
  outpath="$output_dir/$1"
  mkdir -p "$outpath"
  $xnbcli unpack "$input_dir/$1" "$outpath"
}

unpack_one "king/base.xnb"
unpack_one "level.xnb"
unpack_dir "screens/background"
unpack_dir "screens/midground"
unpack_dir "screens/foreground"
find "$output_dir" -type f -name "*.json" -delete
