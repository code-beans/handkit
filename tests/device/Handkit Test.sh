#!/bin/sh
# Starts the Handkit device test from a firmware's ports menu (knulli, muOS and others).
# The handkit-test folder sits next to this script, or in muOS's separate ports folder.
here=$(dirname "$0")
for dir in "$here/handkit-test" /mnt/mmc/ports/handkit-test /mnt/sdcard/ports/handkit-test; do
    if [ -x "$dir/run-tests.sh" ]; then
        cd "$dir" || exit 1
        exec ./run-tests.sh
    fi
done
echo "Handkit Test: the handkit-test folder was not found" >&2
exit 1
