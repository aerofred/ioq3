#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
test_binary=$(mktemp /tmp/ioq3-aim-assist-test.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
"${CC:-cc}" -std=c99 -o "$test_binary" \
  misc/ios/tests/aim_assist_test.c code/ios/ios_aim_assist.c \
  code/qcommon/q_shared.c code/qcommon/q_math.c -lm
"$test_binary"
