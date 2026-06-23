#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
RUNNER=${RUNNER:-${SCRIPT_DIR}/run_emissary_fs_repro_rl.sh}
SUITE=${1:-all}
CONTINUE_ON_FAIL=${CONTINUE_ON_FAIL:-0}

workloads=(
    tomcat
    kafka
    tpcc
    wikipedia
    data-serving
    media-streaming
    web-search
    xapian
    specjbb
    finagle-http
    finagle-chirper
    verilator
    speedometer2.0
)

for workload in "${workloads[@]}"; do
    echo
    echo "===== ${workload}: ${SUITE} ====="
    if ! bash "${RUNNER}" "${SUITE}" "${workload}"; then
        if [[ "${CONTINUE_ON_FAIL}" == "1" ]]; then
            echo "warning: ${workload} failed; continuing" >&2
            continue
        fi
        exit 1
    fi
done
