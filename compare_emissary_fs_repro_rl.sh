#!/usr/bin/env bash
set -euo pipefail

TRACE_ROOT=${TRACE_ROOT:-Trace/emissary_fs_repro_rl}
WORKLOAD=${1:-}
BASELINE_NAME=${BASELINE_NAME:-baseline/TreePLRU}
PAPER_NAME=${PAPER_NAME:-paper/LRUEmissary}
RL_SEEDS=${RL_SEEDS:-"1 2 3"}
RL_PREFIX=${RL_PREFIX:-rl/LRUEmissary_seed_}

if [[ -z "${WORKLOAD}" ]]; then
    echo "usage: bash compare_emissary_fs_repro_rl.sh <workload>" >&2
    exit 2
fi

metric_value() {
    local file=$1
    local metric=$2
    awk -v metric="${metric}" '$1 == metric { print $2; exit }' "${file}" 2>/dev/null || true
}

print_metric() {
    local metric=$1
    shift

    printf "%-65s" "${metric}"
    local stats value
    for stats in "$@"; do
        value=$(metric_value "${stats}" "${metric}")
        printf " %18s" "${value:-NA}"
    done
    printf "\n"
}

workload_root="${TRACE_ROOT}/${WORKLOAD}"
baseline_stats="${workload_root}/${BASELINE_NAME}/stats.txt"
paper_stats="${workload_root}/${PAPER_NAME}/stats.txt"

stats_files=("${baseline_stats}" "${paper_stats}")
headers=("baseline" "paper")

for seed in ${RL_SEEDS}; do
    stats_files+=("${workload_root}/${RL_PREFIX}${seed}/stats.txt")
    headers+=("rl_${seed}")
done

printf "%-65s" "metric"
for header in "${headers[@]}"; do
    printf " %18s" "${header}"
done
printf "\n"

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
    system.cpu.fetch.emissaryRLOffSuppressions
    system.cpu.fetch.emissaryRLAdmissionRejects
    system.cpu.fetch.emissaryRLAdmissionAccepts
    system.l2.demandMisses::cpu.inst
    system.l2.demandMissRate::cpu.inst
    system.l2.replacement_policy.preserveVictims
    system.l2.replacement_policy.nonPreserveVictims
    system.l2.replacement_policy.quotaExceededSets
    system.l2.replacement_policy.preserveClears
    system.l2.replacement_policy.preserveFlushes
    system.l2.replacement_policy.qLearningUpdates
    system.l2.replacement_policy.qLearningExplores
    system.l2.replacement_policy.qLearningExploits
    system.l2.replacement_policy.qLearningActionSum
    system.l2.replacement_policy.qAdmissionAccepts
    system.l2.replacement_policy.qAdmissionRejects
    system.l2.replacement_policy.qAdmissionGuardRejects
    system.l2.replacement_policy.preserveHits
)

for metric in "${metrics[@]}"; do
    print_metric "${metric}" "${stats_files[@]}"
done

echo
for file in "${stats_files[@]}"; do
    if [[ ! -f "${file}" ]]; then
        echo "missing stats: ${file}" >&2
    fi
done
