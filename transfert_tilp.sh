#!/bin/bash
set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

tilpcmd=tilp2

if ! command -v tilp2 2>&1 >/dev/null
then
    if command -v tilp 2>&1 >/dev/null
    then
        tilpcmd=tilp
    else
        echo "error: tilp2 not found. See https://tipla.net/tilpinst"
        exit 1
    fi
fi

tilpcmd="${tilpcmd} --cable=DirectLink -ns"

pushd $SCRIPT_DIR

for i in $(seq -w 00 44); do
    if [ ! -f "app/AppIns${i}.8xv" ]; then
        break
    fi
    echo "Transferring AppIns${i}.8xv..."
    $tilpcmd "app/AppIns${i}.8xv" &>/dev/null
done

echo "Transferring arTIfiCE and the app installer..."
$tilpcmd arTIfiCE.8xp &>/dev/null
$tilpcmd INST.8xp &>/dev/null

popd
