#!/usr/bin/env bash
# set -euo pipefail

GEM5_ROOT=${GEM5_ROOT:-/home/gem5}
GEM5_BIN=${GEM5_BIN:-${GEM5_ROOT}/build/X86/gem5.opt}
CONFIG=${CONFIG:-${GEM5_ROOT}/configs/deprecated/example/se.py}
BENCH=${BENCH:-${GEM5_ROOT}/benchmarks/emissary_effect_test}
TRACE_ROOT=${TRACE_ROOT:-${GEM5_ROOT}/Trace}

COMMON_ARGS=(
    "${CONFIG}"
    --cmd="${BENCH}"
    --cpu-type=DerivO3CPU
    --caches --l2cache
    --l2_size=128kB --l2_assoc=8
)

rm -rf \
    "${TRACE_ROOT}/baseline" \
    "${TRACE_ROOT}/emissary_strong" \
    "${TRACE_ROOT}/emissary_paper"

"${GEM5_BIN}" \
    --outdir="${TRACE_ROOT}/baseline" \
    "${COMMON_ARGS[@]}"

"${GEM5_BIN}" \
    --outdir="${TRACE_ROOT}/emissary_strong" \
    "${COMMON_ARGS[@]}" \
    --l2_rp=LRUEmissary \
    --lru_ways=4 --preserve_ways=4 \
    --emissary-enable \
    --starveAtleast=2 --starveRandomness=100

"${GEM5_BIN}" \
    --outdir="${TRACE_ROOT}/emissary_paper" \
    "${COMMON_ARGS[@]}" \
    --l2_rp=LRUEmissary \
    --lru_ways=4 --preserve_ways=4 \
    --emissary-enable \
    --emissary-require-iq-empty \
    --emissary-sample-rate=3.125 \
    --starveAtleast=1 --starveRandomness=100
