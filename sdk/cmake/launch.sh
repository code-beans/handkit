#!/bin/sh
cd "$(dirname "$0")" || exit 1
./app > app.log 2>&1
