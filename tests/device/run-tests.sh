#!/bin/sh
# Handkit device test: writes system details and the results of the input self-test and
# the demo smoke test to handkit-report.txt, then opens the demo for a minute for checking
# the buttons, and adds the buttons pressed to the report.
# Uses only BusyBox-compatible shell, so it runs on every firmware's /bin/sh.
cd "$(dirname "$0")" || exit 1
report="$PWD/handkit-report.txt"

section() {
    printf '\n## %s\n\n' "$1"
}

# Prints the first lines of each file that exists.
show() {
    for file in "$@"; do
        if [ -f "$file" ]; then
            echo "$file:"
            head -n 20 "$file" | tr -d '\000'
            echo
        fi
    done
}

{
    echo "# Handkit device report"
    echo
    echo "Handkit: $(cat VERSION 2>/dev/null || echo unknown)"
    echo "Date: $(date)"

    section "Device"
    echo "Model: $(tr -d '\000' </proc/device-tree/model 2>/dev/null || echo unknown)"
    uname -srvm

    section "Firmware"
    show /etc/os-release /mnt/SDCARD/.system/version.txt /opt/muos/config/system/version /usr/share/knulli/knulli.version /mnt/SDCARD/spruce/spruce /etc/version

    section "C library"
    for libc in /lib/libc.so.6 /lib64/libc.so.6 /usr/lib/libc.so.6 /lib/aarch64-linux-gnu/libc.so.6; do
        if [ -x "$libc" ]; then
            "$libc" 2>/dev/null | head -n 1
            break
        fi
    done
    ls -l /lib/libc.so.6 /lib/libc-* /usr/lib/libc.so.6 2>/dev/null

    section "Graphics"
    ls -l /dev/fb0 /dev/dri /dev/mali0 /dev/pvr_sync 2>&1
    echo "Framebuffer size: $(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null), $(cat /sys/class/graphics/fb0/bits_per_pixel 2>/dev/null) bits per pixel"
    ls /usr/lib /usr/lib64 2>/dev/null | grep -iE '^lib(EGL|GLES|mali|pvr|IMG|vulkan|VK_)' | sort -u
    ls /usr/share/vulkan/icd.d /etc/vulkan/icd.d 2>/dev/null

    section "Input devices"
    cat /proc/bus/input/devices

    section "Controller mapping from the firmware"
    echo "SDL_GAMECONTROLLERCONFIG: ${SDL_GAMECONTROLLERCONFIG:-(not set)}"
    echo "SDL_GAMECONTROLLERCONFIG_FILE: ${SDL_GAMECONTROLLERCONFIG_FILE:-(not set)}"

    section "Input self-test"
    ./sdk-input-check
    echo "Exit code: $?"

    # The demo reports on stderr; firmware libraries such as NextUI's libmsettings print
    # mixer settings on stdout, which would only clutter the report.
    section "Demo smoke test"
    ./app --smoke 2>&1 >/dev/null
    echo "Exit code: $?"
} >"$report" 2>&1
sync

# The demo stays open for a minute for checking the buttons by hand; Start + Select exits sooner.
{
    section "Button check"
    ./app 60 2>&1 >/dev/null
    echo "Exit code: $?"
} >>"$report" 2>&1
sync
