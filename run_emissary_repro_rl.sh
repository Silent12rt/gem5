#!/usr/bin/env bash
set -euo pipefail

GEM5_ROOT=${GEM5_ROOT:-/home/gem5}
GEM5_BIN=${GEM5_BIN:-${GEM5_ROOT}/build/X86/gem5.opt}
CONFIG=${CONFIG:-${GEM5_ROOT}/configs/deprecated/example/se.py}

BENCH=${BENCH:-${GEM5_ROOT}/benchmarks/623/xalancbmk_s_base.mytest-m64}
BENCH_OPTIONS="${BENCH_OPTIONS:--v benchmarks/623/test.xml benchmarks/623/xalanc.xsl}"
TRACE_ROOT=${TRACE_ROOT:-${GEM5_ROOT}/Trace/emissary_repro_rl}
SUITE=${1:-all}

CPU_TYPE=${CPU_TYPE:-DerivO3CPU}
CACHELINE_SIZE=${CACHELINE_SIZE:-64}
MEM_SIZE=${MEM_SIZE:-}
L1I_SIZE=${L1I_SIZE:-32768B}
L1D_SIZE=${L1D_SIZE:-65536B}
L1I_ASSOC=${L1I_ASSOC:-8}
L1D_ASSOC=${L1D_ASSOC:-8}

L2_SIZE=${L2_SIZE:-1024KiB}
L2_ASSOC=${L2_ASSOC:-16}
L2_LRU_WAYS=${L2_LRU_WAYS:-0}
L2_PRESERVE_WAYS=${L2_PRESERVE_WAYS:-4}

USE_L3=${USE_L3:-1}
L3_SIZE=${L3_SIZE:-2097152B}
L3_ASSOC=${L3_ASSOC:-16}
L3_RP=${L3_RP:-LRU}
L3_RRPV_BITS=${L3_RRPV_BITS:-2}

USE_FDIP=${USE_FDIP:-0}
FDIP_NUM_FTQ_ENTRIES=${FDIP_NUM_FTQ_ENTRIES:-8}
FDIP_FETCH_TARGET_WIDTH=${FDIP_FETCH_TARGET_WIDTH:-64}
FDIP_PFQ_SIZE=${FDIP_PFQ_SIZE:-64}
FDIP_TQ_SIZE=${FDIP_TQ_SIZE:-64}
BP_TYPE=${BP_TYPE:-}
MAXINSTS=${MAXINSTS:-}

PAPER_EPOCH=${PAPER_EPOCH:-1000000}
PAPER_SAMPLE_RATE=${PAPER_SAMPLE_RATE:-3.125}
PAPER_STARVE_ATLEAST=${PAPER_STARVE_ATLEAST:-1}
PAPER_STARVE_RANDOMNESS=${PAPER_STARVE_RANDOMNESS:-100}

RL_SEEDS=${RL_SEEDS:-"1 2 3"}
RL_SAMPLE_RATE=${RL_SAMPLE_RATE:-12.5}
RL_STARVE_ATLEAST=${RL_STARVE_ATLEAST:-1}
RL_STARVE_RANDOMNESS=${RL_STARVE_RANDOMNESS:-${PAPER_STARVE_RANDOMNESS}}
RL_TARGET_SATURATION=${RL_TARGET_SATURATION:-10}
RL_TARGET_OCCUPANCY=${RL_TARGET_OCCUPANCY:-4.0}
RL_EPSILON=${RL_EPSILON:-0.08}
RL_ALPHA=${RL_ALPHA:-0.2}
RL_GAMMA=${RL_GAMMA:-0.8}
RL_MIN_PRESERVE_WAYS=${RL_MIN_PRESERVE_WAYS:-0}
RL_DEFAULT_ACTION=${RL_DEFAULT_ACTION:-0}
RL_REUSE_CAP=${RL_REUSE_CAP:-32.0}
RL_INST_BASELINE_ALPHA=${RL_INST_BASELINE_ALPHA:-0.25}
RL_ACTION_ADMISSION_RATES=${RL_ACTION_ADMISSION_RATES:-0,3.125,6.25,12.5,25}
RL_ACTION_PRESERVE_WAYS=${RL_ACTION_PRESERVE_WAYS:-0,1,1,2,2}
RL_Q_REWARD_NON_PRESERVE_VICTIM=${RL_Q_REWARD_NON_PRESERVE_VICTIM:-0.0}
RL_Q_PENALTY_PRESERVE_VICTIM=${RL_Q_PENALTY_PRESERVE_VICTIM:-10.0}
RL_Q_PENALTY_QUOTA_EXCEEDED=${RL_Q_PENALTY_QUOTA_EXCEEDED:-0.0}
RL_Q_PENALTY_SATURATION=${RL_Q_PENALTY_SATURATION:-0.0}
RL_Q_REWARD_PRESERVE_HIT=${RL_Q_REWARD_PRESERVE_HIT:-0.2}
RL_Q_REWARD_INST_FILL_REDUCTION=${RL_Q_REWARD_INST_FILL_REDUCTION:-8.0}
RL_Q_REWARD_TOTAL_FILL_REDUCTION=${RL_Q_REWARD_TOTAL_FILL_REDUCTION:-2.0}
RL_Q_PENALTY_INST_FILL_REGRESSION=${RL_Q_PENALTY_INST_FILL_REGRESSION:-12.0}
RL_Q_PENALTY_DATA_FILL_REGRESSION=${RL_Q_PENALTY_DATA_FILL_REGRESSION:-16.0}
RL_Q_PENALTY_TOTAL_FILL_REGRESSION=${RL_Q_PENALTY_TOTAL_FILL_REGRESSION:-8.0}
RL_Q_PENALTY_ADMITTED_PRESERVE=${RL_Q_PENALTY_ADMITTED_PRESERVE:-0.5}
RL_Q_PENALTY_ADMISSION_PRESSURE=${RL_Q_PENALTY_ADMISSION_PRESSURE:-0.005}
RL_Q_PENALTY_PRESERVE_CLEAR=${RL_Q_PENALTY_PRESERVE_CLEAR:-2.0}
RL_Q_PENALTY_PRESERVE_OCCUPANCY=${RL_Q_PENALTY_PRESERVE_OCCUPANCY:-0.5}
RL_Q_PENALTY_INST_FILL=${RL_Q_PENALTY_INST_FILL:-0.0}
RL_Q_PENALTY_DATA_FILL=${RL_Q_PENALTY_DATA_FILL:-2.0}
RL_FILL_REGRESSION_GUARD=${RL_FILL_REGRESSION_GUARD:-0}
RL_DATA_REGRESSION_GUARD=${RL_DATA_REGRESSION_GUARD:-0}
RL_DATA_POLLUTION_GUARD=${RL_DATA_POLLUTION_GUARD:-1}
RL_DATA_POLLUTION_THRESHOLD=${RL_DATA_POLLUTION_THRESHOLD:-8}
RL_SET_DATA_POLLUTION_FILTER=${RL_SET_DATA_POLLUTION_FILTER:-1}
RL_SET_DATA_POLLUTION_COOLDOWN=${RL_SET_DATA_POLLUTION_COOLDOWN:-8}
RL_BAD_ACTION_COOLDOWN=${RL_BAD_ACTION_COOLDOWN:-4}
RL_ACTION_QUALITY_GATE=${RL_ACTION_QUALITY_GATE:-0}
RL_ACTION_QUALITY_ALPHA=${RL_ACTION_QUALITY_ALPHA:-0.35}
RL_MIN_ACTION_QUALITY=${RL_MIN_ACTION_QUALITY:--0.2}
RL_ACTION_QUALITY_RECOVERY=${RL_ACTION_QUALITY_RECOVERY:-0.02}
RL_ACTION_QUALITY_SAMPLE_CAP=${RL_ACTION_QUALITY_SAMPLE_CAP:-8.0}
RL_QUALITY_DATA_WEIGHT=${RL_QUALITY_DATA_WEIGHT:-2.0}
RL_QUALITY_TOTAL_WEIGHT=${RL_QUALITY_TOTAL_WEIGHT:-1.0}
RL_QUALITY_PRESERVED_DATA_WEIGHT=${RL_QUALITY_PRESERVED_DATA_WEIGHT:-64.0}

CLEAN_OUTDIR=${CLEAN_OUTDIR:-1}
DRY_RUN=${DRY_RUN:-0}

COMMON_ARGS=(
    "${CONFIG}"
    --cmd="${BENCH}"
    --cpu-type="${CPU_TYPE}"
    --cacheline_size="${CACHELINE_SIZE}"
    --caches --l2cache
    --l1i_size="${L1I_SIZE}"
    --l1d_size="${L1D_SIZE}"
    --l1i_assoc="${L1I_ASSOC}"
    --l1d_assoc="${L1D_ASSOC}"
    --l2_size="${L2_SIZE}"
    --l2_assoc="${L2_ASSOC}"
)

if [[ -n "${BENCH_OPTIONS}" ]]; then
    COMMON_ARGS+=(--options="${BENCH_OPTIONS}")
fi

if [[ "${USE_L3}" == "1" ]]; then
    COMMON_ARGS+=(
        --l3cache
        --l3_size="${L3_SIZE}"
        --l3_assoc="${L3_ASSOC}"
        --l3_rp="${L3_RP}"
        --l3_rrpv_bits="${L3_RRPV_BITS}"
    )
fi

if [[ "${USE_FDIP}" == "1" ]]; then
    COMMON_ARGS+=(
        --fdip
        --fdip-num-ftq-entries="${FDIP_NUM_FTQ_ENTRIES}"
        --fdip-fetch-target-width="${FDIP_FETCH_TARGET_WIDTH}"
        --fdip-pfq-size="${FDIP_PFQ_SIZE}"
        --fdip-tq-size="${FDIP_TQ_SIZE}"
    )
fi

if [[ -n "${MEM_SIZE}" ]]; then
    COMMON_ARGS+=(--mem-size="${MEM_SIZE}")
fi

if [[ -n "${BP_TYPE}" ]]; then
    COMMON_ARGS+=(--bp-type="${BP_TYPE}")
fi

if [[ -n "${MAXINSTS}" ]]; then
    COMMON_ARGS+=(--maxinsts="${MAXINSTS}")
fi

run_case() {
    local name=$1
    shift

    local outdir="${TRACE_ROOT}/${name}"
    if [[ -z "${outdir}" || "${outdir}" == "/" ]]; then
        echo "Refusing to use unsafe outdir: '${outdir}'" >&2
        exit 1
    fi

    echo "==> ${name}"
    echo "    outdir: ${outdir}"

    if [[ "${DRY_RUN}" == "1" ]]; then
        printf '%q ' "${GEM5_BIN}" --outdir="${outdir}" "${COMMON_ARGS[@]}" "$@"
        printf '\n'
        return
    fi

    if [[ "${CLEAN_OUTDIR}" == "1" ]]; then
        rm -rf "${outdir}"
    fi
    mkdir -p "${outdir}"

    "${GEM5_BIN}" --outdir="${outdir}" "${COMMON_ARGS[@]}" "$@"
}

run_emissary_case() {
    local name=$1
    shift

    run_case "${name}" \
        --l2_rp=LRUEmissary \
        --lru_ways="${L2_LRU_WAYS}" \
        --preserve_ways="${L2_PRESERVE_WAYS}" \
        "$@"
}

run_baseline_suite() {
    run_case "baseline/lru" \
        --l2_rp=LRU
}

run_paper_suite() {
    run_emissary_case "paper/lru_emissary" \
        --hist_freq_cycles="${PAPER_EPOCH}" \
        --emissary-enable \
        --emissary-require-iq-empty \
        --emissary-sample-rate="${PAPER_SAMPLE_RATE}" \
        --starveAtleast="${PAPER_STARVE_ATLEAST}" \
        --starveRandomness="${PAPER_STARVE_RANDOMNESS}"
}

run_rl_suite() {
    local fill_guard_args=()
    if [[ "${RL_FILL_REGRESSION_GUARD}" != "1" ]]; then
        fill_guard_args+=(--q-learning-disable-fill-regression-guard)
    fi
    local data_guard_args=()
    if [[ "${RL_DATA_REGRESSION_GUARD}" != "1" ]]; then
        data_guard_args+=(--q-learning-disable-data-regression-guard)
    fi
    local data_pollution_guard_args=()
    if [[ "${RL_DATA_POLLUTION_GUARD}" != "1" ]]; then
        data_pollution_guard_args+=(--q-learning-disable-data-pollution-guard)
    fi
    local set_data_pollution_filter_args=()
    if [[ "${RL_SET_DATA_POLLUTION_FILTER}" != "1" ]]; then
        set_data_pollution_filter_args+=(
            --q-learning-disable-set-data-pollution-filter)
    fi
    local quality_gate_args=()
    if [[ "${RL_ACTION_QUALITY_GATE}" != "1" ]]; then
        quality_gate_args+=(--q-learning-disable-action-quality-gate)
    fi

    local seed
    for seed in ${RL_SEEDS}; do
        run_emissary_case "rl/q_learning_seed_${seed}" \
            --hist_freq_cycles="${PAPER_EPOCH}" \
            --q-learning-preserve \
            --q-learning-seed="${seed}" \
            --q-learning-alpha="${RL_ALPHA}" \
            --q-learning-gamma="${RL_GAMMA}" \
            --q-learning-epsilon="${RL_EPSILON}" \
            --q-learning-target-saturation="${RL_TARGET_SATURATION}" \
            --q-learning-target-occupancy="${RL_TARGET_OCCUPANCY}" \
            --q-learning-min-preserve-ways="${RL_MIN_PRESERVE_WAYS}" \
            --q-learning-default-action="${RL_DEFAULT_ACTION}" \
            --q-learning-reuse-cap="${RL_REUSE_CAP}" \
            --q-learning-inst-baseline-alpha="${RL_INST_BASELINE_ALPHA}" \
            --q-action-admission-rates="${RL_ACTION_ADMISSION_RATES}" \
            --q-action-preserve-ways="${RL_ACTION_PRESERVE_WAYS}" \
            --q-reward-non-preserve-victim="${RL_Q_REWARD_NON_PRESERVE_VICTIM}" \
            --q-penalty-preserve-victim="${RL_Q_PENALTY_PRESERVE_VICTIM}" \
            --q-penalty-quota-exceeded="${RL_Q_PENALTY_QUOTA_EXCEEDED}" \
            --q-penalty-saturation="${RL_Q_PENALTY_SATURATION}" \
            --q-reward-preserve-hit="${RL_Q_REWARD_PRESERVE_HIT}" \
            --q-reward-inst-fill-reduction="${RL_Q_REWARD_INST_FILL_REDUCTION}" \
            --q-reward-total-fill-reduction="${RL_Q_REWARD_TOTAL_FILL_REDUCTION}" \
            --q-penalty-inst-fill-regression="${RL_Q_PENALTY_INST_FILL_REGRESSION}" \
            --q-penalty-data-fill-regression="${RL_Q_PENALTY_DATA_FILL_REGRESSION}" \
            --q-penalty-total-fill-regression="${RL_Q_PENALTY_TOTAL_FILL_REGRESSION}" \
            --q-penalty-admitted-preserve="${RL_Q_PENALTY_ADMITTED_PRESERVE}" \
            --q-penalty-admission-pressure="${RL_Q_PENALTY_ADMISSION_PRESSURE}" \
            --q-penalty-preserve-clear="${RL_Q_PENALTY_PRESERVE_CLEAR}" \
            --q-penalty-preserve-occupancy="${RL_Q_PENALTY_PRESERVE_OCCUPANCY}" \
            --q-penalty-inst-fill="${RL_Q_PENALTY_INST_FILL}" \
            --q-penalty-data-fill="${RL_Q_PENALTY_DATA_FILL}" \
            --q-learning-bad-action-cooldown="${RL_BAD_ACTION_COOLDOWN}" \
            --q-learning-data-pollution-threshold="${RL_DATA_POLLUTION_THRESHOLD}" \
            --q-learning-set-data-pollution-cooldown="${RL_SET_DATA_POLLUTION_COOLDOWN}" \
            --q-learning-action-quality-alpha="${RL_ACTION_QUALITY_ALPHA}" \
            --q-learning-min-action-quality="${RL_MIN_ACTION_QUALITY}" \
            --q-learning-action-quality-recovery="${RL_ACTION_QUALITY_RECOVERY}" \
            --q-learning-action-quality-sample-cap="${RL_ACTION_QUALITY_SAMPLE_CAP}" \
            --q-learning-quality-data-weight="${RL_QUALITY_DATA_WEIGHT}" \
            --q-learning-quality-total-weight="${RL_QUALITY_TOTAL_WEIGHT}" \
            --q-learning-quality-preserved-data-weight="${RL_QUALITY_PRESERVED_DATA_WEIGHT}" \
            "${fill_guard_args[@]}" \
            "${data_guard_args[@]}" \
            "${data_pollution_guard_args[@]}" \
            "${set_data_pollution_filter_args[@]}" \
            "${quality_gate_args[@]}" \
            --emissary-rng-seed="${seed}" \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate="${RL_SAMPLE_RATE}" \
            --starveAtleast="${RL_STARVE_ATLEAST}" \
            --starveRandomness="${RL_STARVE_RANDOMNESS}"
    done
}

usage() {
    echo "usage: $0 [baseline|paper|rl|all]" >&2
}

case "${SUITE}" in
    baseline)
        run_baseline_suite
        ;;
    paper)
        run_paper_suite
        ;;
    rl)
        run_rl_suite
        ;;
    all)
        run_baseline_suite
        run_paper_suite
        run_rl_suite
        ;;
    *)
        usage
        exit 2
        ;;
esac
