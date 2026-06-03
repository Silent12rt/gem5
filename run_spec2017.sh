#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

GEM5_ROOT=${GEM5_ROOT:-${SCRIPT_DIR}}
RUNNER=${RUNNER:-${GEM5_ROOT}/run_emissary_repro_rl.sh}

usage() {
    cat >&2 <<'EOF'
usage:
  bash run_spec2017.sh [baseline|paper|rl|all] <spec_id>
  bash run_spec2017.sh list

examples:
  bash run_spec2017.sh baseline 605
  bash run_spec2017.sh paper 623
  bash run_spec2017.sh rl 620
  bash run_spec2017.sh all 657

supported spec_id:
  602 605 620 623 631 641 657 600 648 625
EOF
}

list_specs() {
    cat <<'EOF'
602  sgcc       benchmarks/602/sgcc_base.mytest-m64
605  mcf        benchmarks/605/mcf_s_base.mytest-m64
620  omnetpp    benchmarks/620/omnetpp_s_base.mytest-m64
623  xalancbmk  benchmarks/623/xalancbmk_s_base.mytest-m64
631  deepsjeng  benchmarks/631/deepsjeng_s_base.mytest-m64
641  leela      benchmarks/641/leela_s_base.mytest-m64
657  xz         benchmarks/657/xz_s_base.mytest-m64
600  perlbench  benchmarks/600/perlbench_s_base.mytest-m64
648  exchange2  benchmarks/648/exchange2_s_base.mytest-m64
625  x264       benchmarks/625/x264_s_base.mytest-m64
EOF
}

if [[ "${1:-}" == "list" ]]; then
    list_specs
    exit 0
fi

SUITE=${1:-}
SPEC_ID=${2:-}

case "${SUITE}" in
    baseline|paper|rl|all)
        ;;
    *)
        usage
        exit 2
        ;;
esac

case "${SPEC_ID}" in
    602)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/602/sgcc_base.mytest-m64"
        bench_options="benchmarks/602/t1.c -O3 -finline-limit=50000 -o benchmarks/602/t1.opts-O3_-finline-limit_50000.s"
        ;;
    605)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/605/mcf_s_base.mytest-m64"
        bench_options="benchmarks/605/inp.in"
        ;;
    620)
        run_dir="${GEM5_ROOT}/benchmarks/620"
        bench_rel="benchmarks/620/omnetpp_s_base.mytest-m64"
        bench_options="-c General -r 0"
        ;;
    623)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/623/xalancbmk_s_base.mytest-m64"
        bench_options="-v benchmarks/623/test.xml benchmarks/623/xalanc.xsl"
        ;;
    631)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/631/deepsjeng_s_base.mytest-m64"
        bench_options="benchmarks/631/test.txt"
        ;;
    641)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/641/leela_s_base.mytest-m64"
        bench_options="benchmarks/641/test.sgf"
        ;;
    657)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/657/xz_s_base.mytest-m64"
        bench_options="benchmarks/657/cpu2006docs.tar.xz 4 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 1548636 1555348 0"
        ;;
    600)
        run_dir="${GEM5_ROOT}/benchmarks/600"
        bench_rel="benchmarks/600/perlbench_s_base.mytest-m64"
        bench_options="-I. -I./lib test.pl"
        ;;
    648)
        run_dir="${GEM5_ROOT}/benchmarks/648"
        bench_rel="benchmarks/648/exchange2_s_base.mytest-m64"
        bench_options="0"
        ;;
    625)
        run_dir="${GEM5_ROOT}"
        bench_rel="benchmarks/625/x264_s_base.mytest-m64"
        bench_options="--dumpyuv 50 --frames 156 -o benchmarks/625/BuckBunny_New.264 benchmarks/625/BuckBunny.yuv 1280x720"
        ;;
    *)
        usage
        exit 2
        ;;
esac

# Common experiment knobs. Edit defaults here, or override them on the
# command line, for example: RL_EPSILON=0.08 bash run_spec2017.sh rl 620
export GEM5_ROOT
export GEM5_BIN="${GEM5_BIN:-${GEM5_ROOT}/build/X86/gem5.opt}"
export CONFIG="${CONFIG:-${GEM5_ROOT}/configs/deprecated/example/se.py}"

export CPU_TYPE="${CPU_TYPE:-DerivO3CPU}"
export CACHELINE_SIZE="${CACHELINE_SIZE:-64}"
export MEM_SIZE="${MEM_SIZE:-8GiB}"

export L1I_SIZE="${L1I_SIZE:-32768B}"
export L1D_SIZE="${L1D_SIZE:-65536B}"
export L1I_ASSOC="${L1I_ASSOC:-8}"
export L1D_ASSOC="${L1D_ASSOC:-8}"

export L2_SIZE="${L2_SIZE:-1024KiB}"
export L2_ASSOC="${L2_ASSOC:-16}"
export L2_LRU_WAYS="${L2_LRU_WAYS:-0}"
export L2_PRESERVE_WAYS="${L2_PRESERVE_WAYS:-4}"

export USE_L3="${USE_L3:-1}"
export L3_SIZE="${L3_SIZE:-2097152B}"
export L3_ASSOC="${L3_ASSOC:-16}"
export L3_RP="${L3_RP:-LRU}"
export L3_RRPV_BITS="${L3_RRPV_BITS:-2}"

export USE_FDIP="${USE_FDIP:-0}"
export FDIP_NUM_FTQ_ENTRIES="${FDIP_NUM_FTQ_ENTRIES:-8}"
export FDIP_FETCH_TARGET_WIDTH="${FDIP_FETCH_TARGET_WIDTH:-64}"
export FDIP_PFQ_SIZE="${FDIP_PFQ_SIZE:-64}"
export FDIP_TQ_SIZE="${FDIP_TQ_SIZE:-64}"
export BP_TYPE="${BP_TYPE:-}"
export MAXINSTS="${MAXINSTS-100000000}"

export PAPER_EPOCH="${PAPER_EPOCH:-500000}"
export PAPER_SAMPLE_RATE="${PAPER_SAMPLE_RATE:-3.125}"
export PAPER_STARVE_ATLEAST="${PAPER_STARVE_ATLEAST:-1}"
export PAPER_STARVE_RANDOMNESS="${PAPER_STARVE_RANDOMNESS:-100}"

export RL_SEEDS="${RL_SEEDS:-1 2 3}"
export RL_SAMPLE_RATE="${RL_SAMPLE_RATE:-12.5}"
export RL_TARGET_SATURATION="${RL_TARGET_SATURATION:-10}"
export RL_TARGET_OCCUPANCY="${RL_TARGET_OCCUPANCY:-1.5}"
export RL_EPSILON="${RL_EPSILON:-0.01}"
export RL_ALPHA="${RL_ALPHA:-0.2}"
export RL_GAMMA="${RL_GAMMA:-0.8}"
export RL_MIN_PRESERVE_WAYS="${RL_MIN_PRESERVE_WAYS:-0}"
export RL_DEFAULT_ACTION="${RL_DEFAULT_ACTION:-0}"
export RL_REUSE_CAP="${RL_REUSE_CAP:-1.5}"
export RL_INST_BASELINE_ALPHA="${RL_INST_BASELINE_ALPHA:-0.25}"
export RL_ACTION_ADMISSION_RATES="${RL_ACTION_ADMISSION_RATES:-0,0.390625,0.78125,1.5625,3.125}"
export RL_ACTION_PRESERVE_WAYS="${RL_ACTION_PRESERVE_WAYS:-0,1,1,1,2}"
export RL_Q_REWARD_NON_PRESERVE_VICTIM="${RL_Q_REWARD_NON_PRESERVE_VICTIM:-0.0}"
export RL_Q_PENALTY_PRESERVE_VICTIM="${RL_Q_PENALTY_PRESERVE_VICTIM:-10.0}"
export RL_Q_PENALTY_QUOTA_EXCEEDED="${RL_Q_PENALTY_QUOTA_EXCEEDED:-0.0}"
export RL_Q_PENALTY_SATURATION="${RL_Q_PENALTY_SATURATION:-0.0}"
export RL_Q_REWARD_PRESERVE_HIT="${RL_Q_REWARD_PRESERVE_HIT:-0.05}"
export RL_Q_REWARD_INST_FILL_REDUCTION="${RL_Q_REWARD_INST_FILL_REDUCTION:-4.0}"
export RL_Q_REWARD_TOTAL_FILL_REDUCTION="${RL_Q_REWARD_TOTAL_FILL_REDUCTION:-4.0}"
export RL_Q_PENALTY_INST_FILL_REGRESSION="${RL_Q_PENALTY_INST_FILL_REGRESSION:-50.0}"
export RL_Q_PENALTY_DATA_FILL_REGRESSION="${RL_Q_PENALTY_DATA_FILL_REGRESSION:-80.0}"
export RL_Q_PENALTY_TOTAL_FILL_REGRESSION="${RL_Q_PENALTY_TOTAL_FILL_REGRESSION:-60.0}"
export RL_Q_PENALTY_ADMITTED_PRESERVE="${RL_Q_PENALTY_ADMITTED_PRESERVE:-2.0}"
export RL_Q_PENALTY_ADMISSION_PRESSURE="${RL_Q_PENALTY_ADMISSION_PRESSURE:-0.05}"
export RL_Q_PENALTY_PRESERVE_CLEAR="${RL_Q_PENALTY_PRESERVE_CLEAR:-2.0}"
export RL_Q_PENALTY_PRESERVE_OCCUPANCY="${RL_Q_PENALTY_PRESERVE_OCCUPANCY:-3.0}"
export RL_Q_PENALTY_INST_FILL="${RL_Q_PENALTY_INST_FILL:-0.0}"
export RL_Q_PENALTY_DATA_FILL="${RL_Q_PENALTY_DATA_FILL:-8.0}"
export RL_FILL_REGRESSION_GUARD="${RL_FILL_REGRESSION_GUARD:-1}"
export RL_DATA_REGRESSION_GUARD="${RL_DATA_REGRESSION_GUARD:-1}"
export RL_DATA_POLLUTION_GUARD="${RL_DATA_POLLUTION_GUARD:-1}"
export RL_DATA_POLLUTION_THRESHOLD="${RL_DATA_POLLUTION_THRESHOLD:-0}"
export RL_BAD_ACTION_COOLDOWN="${RL_BAD_ACTION_COOLDOWN:-12}"
export RL_ACTION_QUALITY_GATE="${RL_ACTION_QUALITY_GATE:-1}"
export RL_ACTION_QUALITY_ALPHA="${RL_ACTION_QUALITY_ALPHA:-0.35}"
export RL_MIN_ACTION_QUALITY="${RL_MIN_ACTION_QUALITY:--0.35}"
export RL_ACTION_QUALITY_RECOVERY="${RL_ACTION_QUALITY_RECOVERY:-0.03}"
export RL_ACTION_QUALITY_SAMPLE_CAP="${RL_ACTION_QUALITY_SAMPLE_CAP:-8.0}"
export RL_QUALITY_DATA_WEIGHT="${RL_QUALITY_DATA_WEIGHT:-2.0}"
export RL_QUALITY_TOTAL_WEIGHT="${RL_QUALITY_TOTAL_WEIGHT:-1.0}"
export RL_QUALITY_PRESERVED_DATA_WEIGHT="${RL_QUALITY_PRESERVED_DATA_WEIGHT:-32.0}"

if [[ ! -f "${RUNNER}" ]]; then
    echo "runner not found: ${RUNNER}" >&2
    exit 1
fi

export BENCH="${BENCH:-${GEM5_ROOT}/${bench_rel}}"
export BENCH_OPTIONS="${BENCH_OPTIONS:-${bench_options}}"
export TRACE_ROOT="${TRACE_ROOT:-${GEM5_ROOT}/Trace/${SPEC_ID}_emissary_repro_rl}"
export RUN_DIR="${RUN_DIR:-${run_dir}}"

echo "SPEC ${SPEC_ID}, suite ${SUITE}"
echo "  BENCH: ${BENCH}"
echo "  BENCH_OPTIONS: ${BENCH_OPTIONS}"
echo "  TRACE_ROOT: ${TRACE_ROOT}"
echo "  RUN_DIR: ${RUN_DIR}"
echo "  MEM_SIZE: ${MEM_SIZE}"
echo "  L2: ${L2_SIZE}, assoc ${L2_ASSOC}, preserve_cap ${L2_PRESERVE_WAYS}/${L2_ASSOC}, lru_ways ${L2_LRU_WAYS}"
echo "  L3: USE_L3=${USE_L3}, ${L3_SIZE}, assoc ${L3_ASSOC}, rp ${L3_RP}"
echo "  FDIP: USE_FDIP=${USE_FDIP}"
echo "  RL actions: rates=${RL_ACTION_ADMISSION_RATES}, ways=${RL_ACTION_PRESERVE_WAYS}, default=${RL_DEFAULT_ACTION}"
echo "  RL targets: saturation=${RL_TARGET_SATURATION}, occupancy=${RL_TARGET_OCCUPANCY}"
echo "  RL_Q: preserve_hit=${RL_Q_REWARD_PRESERVE_HIT}, inst_reduction_reward=${RL_Q_REWARD_INST_FILL_REDUCTION}, total_reduction_reward=${RL_Q_REWARD_TOTAL_FILL_REDUCTION}, inst_regression_penalty=${RL_Q_PENALTY_INST_FILL_REGRESSION}, data_regression_penalty=${RL_Q_PENALTY_DATA_FILL_REGRESSION}, total_regression_penalty=${RL_Q_PENALTY_TOTAL_FILL_REGRESSION}, admitted_penalty=${RL_Q_PENALTY_ADMITTED_PRESERVE}, pressure_penalty=${RL_Q_PENALTY_ADMISSION_PRESSURE}, clear_penalty=${RL_Q_PENALTY_PRESERVE_CLEAR}, occupancy_penalty=${RL_Q_PENALTY_PRESERVE_OCCUPANCY}, inst_fill_penalty=${RL_Q_PENALTY_INST_FILL}, data_fill_penalty=${RL_Q_PENALTY_DATA_FILL}, fill_guard=${RL_FILL_REGRESSION_GUARD}, data_guard=${RL_DATA_REGRESSION_GUARD}, data_pollution_guard=${RL_DATA_POLLUTION_GUARD}, data_pollution_threshold=${RL_DATA_POLLUTION_THRESHOLD}, bad_action_cooldown=${RL_BAD_ACTION_COOLDOWN}, quality_gate=${RL_ACTION_QUALITY_GATE}, min_quality=${RL_MIN_ACTION_QUALITY}, quality_alpha=${RL_ACTION_QUALITY_ALPHA}, quality_recovery=${RL_ACTION_QUALITY_RECOVERY}, quality_weights=data:${RL_QUALITY_DATA_WEIGHT}/total:${RL_QUALITY_TOTAL_WEIGHT}/preserved_data:${RL_QUALITY_PRESERVED_DATA_WEIGHT}, reuse_cap=${RL_REUSE_CAP}, inst_baseline_alpha=${RL_INST_BASELINE_ALPHA}"
if [[ -n "${MAXINSTS}" ]]; then
    echo "  MAXINSTS: ${MAXINSTS}"
fi

cd "${RUN_DIR}"
exec bash "${RUNNER}" "${SUITE}"
