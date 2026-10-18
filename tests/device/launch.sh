#!/bin/sh
# NextUI and MinUI start a .pak through this script.
cd "$(dirname "$0")" || exit 1
exec ./run-tests.sh
