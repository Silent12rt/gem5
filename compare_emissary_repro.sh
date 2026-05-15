#!/usr/bin/env bash
set -euo pipefail

TRACE_ROOT=${TRACE_ROOT:-/home/gem5/Trace}

metrics=(
    simTicks
    simInsts
    system.cpu.ipc
    system.cpu.cpi
    system.cpu.fetchStats0.icacheStallCycles
    system.cpu.fetch.emissaryCandidates
    system.cpu.fetch.emissaryIQRejects
    system.cpu.fetch.emissarySampleRejects
    system.cpu.fetch.emissaryThresholdRejects
    system.cpu.fetch.emissaryMarks
    system.cpu.fetch.emissaryPreserves
    system.l2.replacement_policy.preserveVictims
    system.l2.replacement_policy.nonPreserveVictims
    system.l2.replacement_policy.quotaExceededSets
    system.l2.replacement_policy.preserveClears
    system.l2.replacement_policy.preserveFlushes
    system.l2.replacement_policy.adaptiveTightens
    system.l2.replacement_policy.adaptiveRelaxes
    system.cpu.icache.demandMisses::cpu.inst
    system.cpu.icache.ReadReq.accesses::cpu.inst
    system.l2.demandMisses::cpu.inst
    system.l2.demandMissRate::cpu.inst
)

get_metric() {
    local file=$1
    local metric=$2
    awk -v metric="${metric}" '$1 == metric { print $2; exit }' "${file}"
}

printf "%-55s %15s %15s %15s %15s\n" \
    "metric" "baseline" "strong" "paper" "paper_delta"
for metric in "${metrics[@]}"; do
    baseline=$(get_metric "${TRACE_ROOT}/baseline/stats.txt" "${metric}" || true)
    strong=$(get_metric "${TRACE_ROOT}/emissary_strong/stats.txt" "${metric}" || true)
    paper=$(get_metric "${TRACE_ROOT}/emissary_paper/stats.txt" "${metric}" || true)
    delta=""
    if [[ -n "${baseline}" && -n "${paper}" ]]; then
        delta=$(awk -v b="${baseline}" -v p="${paper}" \
            'BEGIN { if (b != 0) printf "%+.4f%%", 100 * (p - b) / b; else printf "n/a" }')
    fi
    printf "%-55s %15s %15s %15s %15s\n" \
        "${metric}" "${baseline:-NA}" "${strong:-NA}" "${paper:-NA}" "${delta:-NA}"
done

for run in emissary_strong emissary_paper; do
    hist="${TRACE_ROOT}/${run}/set_hist.csv"
    if [[ -f "${hist}" ]]; then
        echo
        echo "${run} set_hist tail:"
        tail -n 5 "${hist}"
    fi
done
