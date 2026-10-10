#!/bin/sh
i=1
for a in "$@"; do
    printf 'arg%d=%s\n' "$i" "$a"
    i=$((i+1))
done
printf 'argc=%d\n' "$#"
