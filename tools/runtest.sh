#!/bin/bash
# Claude's helper (runs in the Cowork VM): bash tools/runtest.sh <unique id> <preset> <max polls of 5 s>
cd "$(dirname "$0")/../.aio-requests" || exit 1
echo $2 > $1.request
for i in $(seq 1 $3); do sleep 5; [ -f $1.result.txt ] && break; done
if [ -f $1.result.txt ]; then head -6 $1.result.txt; grep -E "\(E\)|Can't compile|\[AIO\]" $1.result.txt | head -40; else echo "no result yet (request $1 still queued or running)"; fi
