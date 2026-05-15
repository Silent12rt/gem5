#!/usr/bin/env bash
set -euo pipefail

GEM5_ROOT=${GEM5_ROOT:-/home/gem5}
GEM5_BIN=${GEM5_BIN:-${GEM5_ROOT}/build/X86/gem5.opt}
CONFIG=${CONFIG:-${GEM5_ROOT}/configs/deprecated/example/se.py}
BENCH=${BENCH:-${GEM5_ROOT}/benchmarks/emissary_effect_test}
TRACE_ROOT=${TRACE_ROOT:-${GEM5_ROOT}/Trace/emissary_sweeps}
SUITE=${1:-all}

COMMON_ARGS=(
    "${CONFIG}"
    --cmd="${BENCH}"
    --cpu-type=DerivO3CPU
    --caches --l2cache
    --l2_assoc=8
)

run_case() {
    local name=$1
    shift

    local outdir="${TRACE_ROOT}/${name}"
    echo "==> ${name}"
    rm -rf "${outdir}"
    "${GEM5_BIN}" --outdir="${outdir}" "${COMMON_ARGS[@]}" "$@"
}

run_baseline() {
    local name=$1
    local l2_size=$2
    run_case "${name}" --l2_size="${l2_size}"
}

run_emissary() {
    local name=$1
    local l2_size=$2
    local lru_ways=$3
    local preserve_ways=$4
    shift 4

    run_case "${name}" \
        --l2_size="${l2_size}" \
        --l2_rp=LRUEmissary \
        --lru_ways="${lru_ways}" \
        --preserve_ways="${preserve_ways}" \
        "$@"
}

run_epoch_suite() {
    run_baseline "epoch/baseline_128kB" 128kB
    for epoch in 1000000 5000000; do
        run_emissary "epoch/paper_128kB_epoch_${epoch}" 128kB 4 4 \
            --hist_freq_cycles="${epoch}" \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
    done
}

run_ablation_suite() {
    run_baseline "ablation/baseline_lru" 128kB
    run_emissary "ablation/s_only" 128kB 4 4 \
        --hist_freq_cycles=1000000 \
        --emissary-enable \
        --emissary-sample-rate=100 \
        --starveAtleast=1 --starveRandomness=100
    run_emissary "ablation/s_e" 128kB 4 4 \
        --hist_freq_cycles=1000000 \
        --emissary-enable \
        --emissary-require-iq-empty \
        --emissary-sample-rate=100 \
        --starveAtleast=1 --starveRandomness=100
    run_emissary "ablation/s_e_r_1_32" 128kB 4 4 \
        --hist_freq_cycles=1000000 \
        --emissary-enable \
        --emissary-require-iq-empty \
        --emissary-sample-rate=3.125 \
        --starveAtleast=1 --starveRandomness=100
    run_emissary "ablation/strong" 128kB 4 4 \
        --hist_freq_cycles=1000000 \
        --emissary-enable \
        --emissary-sample-rate=100 \
        --starveAtleast=2 --starveRandomness=100
}

run_l2_size_suite() {
    for size in 128kB 256kB 512kB 1MB 2MB; do
        run_baseline "l2_size/baseline_${size}" "${size}"
        run_emissary "l2_size/paper_${size}" "${size}" 4 4 \
            --hist_freq_cycles=1000000 \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
    done
}

run_preserve_ways_suite() {
    run_baseline "preserve_ways/baseline_128kB" 128kB
    for preserve_ways in 1 2 3 4; do
        local lru_ways=$((8 - preserve_ways))
        run_emissary "preserve_ways/p${preserve_ways}_lru${lru_ways}" \
            128kB "${lru_ways}" "${preserve_ways}" \
            --hist_freq_cycles=1000000 \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
    done
}

run_input_suite() {
    local cases=(
        "low_hot:80 4 1"
        "default:80 8 1"
        "high_hot:80 12 1"
        "strided_cold:80 8 4"
    )

    for case in "${cases[@]}"; do
        local name=${case%%:*}
        local options=${case#*:}
        run_case "inputs/baseline_${name}" \
            --l2_size=128kB \
            --options="${options}"
        run_emissary "inputs/paper_${name}" 128kB 4 4 \
            --options="${options}" \
            --hist_freq_cycles=1000000 \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
    done
}

run_seed_suite() {
    run_baseline "seeds/baseline_default" 128kB
    for seed in 1 2 3; do
        run_emissary "seeds/paper_seed_${seed}" 128kB 4 4 \
            --hist_freq_cycles=1000000 \
            --emissary-rng-seed="${seed}" \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
    done
}

run_adaptive_suite() {
    run_baseline "adaptive/baseline_128kB" 128kB
    for epoch in 1000000 5000000; do
        run_emissary "adaptive/static_epoch_${epoch}" 128kB 4 4 \
            --hist_freq_cycles="${epoch}" \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
        run_emissary "adaptive/adaptive_epoch_${epoch}" 128kB 4 4 \
            --hist_freq_cycles="${epoch}" \
            --adaptive-preserve \
            --adaptive-target-saturation=25 \
            --adaptive-min-preserve-ways=1 \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate=3.125 \
            --starveAtleast=1 --starveRandomness=100
    done
}

case "${SUITE}" in
    epoch)
        run_epoch_suite
        ;;
    ablation)
        run_ablation_suite
        ;;
    l2-size)
        run_l2_size_suite
        ;;
    preserve-ways)
        run_preserve_ways_suite
        ;;
    inputs)
        run_input_suite
        ;;
    seeds)
        run_seed_suite
        ;;
    adaptive)
        run_adaptive_suite
        ;;
    validation)
        run_input_suite
        run_seed_suite
        ;;
    all)
        run_epoch_suite
        run_ablation_suite
        run_l2_size_suite
        run_preserve_ways_suite
        run_input_suite
        run_seed_suite
        run_adaptive_suite
        ;;
    *)
        echo "usage: $0 [epoch|ablation|l2-size|preserve-ways|inputs|seeds|adaptive|validation|all]" >&2
        exit 2
        ;;
esac
