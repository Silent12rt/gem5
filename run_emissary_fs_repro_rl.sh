#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

usage() {
    cat >&2 <<'EOF'
usage:
  bash run_emissary_fs_repro_rl.sh [baseline|paper|rl|all] <workload>
  bash run_emissary_fs_repro_rl.sh list

examples:
  WORKLOAD_ROOT=/path/to/emissary_workloads bash run_emissary_fs_repro_rl.sh all tomcat
  CHECKPOINT_DIR=/path/to/cpt.tomcat KERNEL=/path/to/vmlinux DISK_IMAGE=/path/to/root.img bash run_emissary_fs_repro_rl.sh paper tomcat

workloads:
  tomcat kafka tpcc wikipedia data-serving media-streaming web-search xapian
  specjbb finagle-http finagle-chirper verilator speedometer2.0
EOF
}

list_workloads() {
    cat <<'EOF'
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
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

find_one_file() {
    local root=$1
    shift
    local matches=()
    local pattern
    for pattern in "$@"; do
        while IFS= read -r path; do
            matches+=("${path}")
        done < <(find "${root}" -type f -iname "${pattern}" | sort)
    done

    if (( ${#matches[@]} == 0 )); then
        return 1
    fi
    printf '%s\n' "${matches[0]}"
}

find_checkpoint() {
    local root=$1
    local workload=$2
    local matches=()

    while IFS= read -r path; do
        matches+=("${path}")
    done < <(
        find "${root}" -type d \
            \( -iname 'cpt.*' -o -iname '*checkpoint*' \) |
        grep -i -- "${workload}" |
        sort
    )

    if (( ${#matches[@]} == 0 )); then
        return 1
    fi
    if (( ${#matches[@]} > 1 )); then
        echo "Multiple checkpoint directories matched ${workload}:" >&2
        printf '  %s\n' "${matches[@]}" >&2
        die "set CHECKPOINT_DIR to the exact checkpoint directory"
    fi
    printf '%s\n' "${matches[0]}"
}

suite=${1:-}
workload=${2:-}

if [[ "${suite}" == "list" ]]; then
    list_workloads
    exit 0
fi

case "${suite}" in
    baseline|paper|rl|all) ;;
    *) usage; exit 2 ;;
esac

if [[ -z "${workload}" ]]; then
    usage
    exit 2
fi

case "${workload}" in
    tomcat|kafka|tpcc|wikipedia|data-serving|media-streaming|web-search|xapian|specjbb|finagle-http|finagle-chirper|verilator|speedometer2.0) ;;
    *) die "unknown workload '${workload}'. Run: bash run_emissary_fs_repro_rl.sh list" ;;
esac

GEM5_ROOT=${GEM5_ROOT:-${SCRIPT_DIR}}
GEM5_BIN=${GEM5_BIN:-${GEM5_ROOT}/build/ARM/gem5.opt}
CONFIG=${CONFIG:-${GEM5_ROOT}/configs/deprecated/example/fs.py}
WORKLOAD_ROOT=${WORKLOAD_ROOT:-${GEM5_ROOT}/cache_paper/emissary_workloads}
TRACE_ROOT=${TRACE_ROOT:-${GEM5_ROOT}/Trace/emissary_fs_repro_rl/${workload}}

if [[ ! -x "${GEM5_BIN}" ]]; then
    die "gem5 ARM binary not found or not executable: ${GEM5_BIN}. Build it with: scons build/ARM/gem5.opt -j$(nproc)"
fi
if [[ ! -f "${CONFIG}" ]]; then
    die "config script not found: ${CONFIG}"
fi
if [[ ! -d "${WORKLOAD_ROOT}" ]]; then
    die "WORKLOAD_ROOT not found: ${WORKLOAD_ROOT}"
fi

CHECKPOINT_DIR=${CHECKPOINT_DIR:-}
KERNEL=${KERNEL:-}
DISK_IMAGE=${DISK_IMAGE:-}

if [[ -z "${CHECKPOINT_DIR}" ]]; then
    CHECKPOINT_DIR=$(find_checkpoint "${WORKLOAD_ROOT}" "${workload}") || \
        die "could not auto-detect checkpoint for '${workload}'. Set CHECKPOINT_DIR=/path/to/checkpoint"
fi
if [[ -z "${KERNEL}" ]]; then
    KERNEL=$(find_one_file "${WORKLOAD_ROOT}" 'vmlinux*' 'Image*') || \
        die "could not auto-detect kernel. Set KERNEL=/path/to/vmlinux"
fi
if [[ -z "${DISK_IMAGE}" ]]; then
    DISK_IMAGE=$(find_one_file "${WORKLOAD_ROOT}" '*.img' '*.qcow2' '*.raw') || \
        die "could not auto-detect disk image. Set DISK_IMAGE=/path/to/root.img"
fi

if [[ ! -d "${CHECKPOINT_DIR}" ]]; then
    die "CHECKPOINT_DIR is not a directory: ${CHECKPOINT_DIR}"
fi
if [[ ! -f "${KERNEL}" ]]; then
    die "KERNEL is not a file: ${KERNEL}"
fi
if [[ ! -f "${DISK_IMAGE}" ]]; then
    die "DISK_IMAGE is not a file: ${DISK_IMAGE}"
fi

CPU_TYPE=${CPU_TYPE:-DerivO3CPU}
RESTORE_WITH_CPU=${RESTORE_WITH_CPU:-}
NUM_CPUS=${NUM_CPUS:-1}
MEM_SIZE=${MEM_SIZE:-8GiB}
ROOT_DEVICE=${ROOT_DEVICE:-/dev/vda1}
MACHINE_TYPE=${MACHINE_TYPE:-VExpress_GEM5_V1}
DTB=${DTB:-}
BOOTLOADER=${BOOTLOADER:-}
OS_TYPE=${OS_TYPE:-linux}

WARMUP_INSTS=${WARMUP_INSTS:-5000000}
MAXINSTS=${MAXINSTS:-100000000}
STANDARD_SWITCH=${STANDARD_SWITCH:-1}
RESET_STATS_AFTER_WARMUP=${RESET_STATS_AFTER_WARMUP:-1}

CACHELINE_SIZE=${CACHELINE_SIZE:-64}
L1I_SIZE=${L1I_SIZE:-32KiB}
L1D_SIZE=${L1D_SIZE:-64KiB}
L1I_ASSOC=${L1I_ASSOC:-8}
L1D_ASSOC=${L1D_ASSOC:-8}
L2_SIZE=${L2_SIZE:-1MiB}
L2_ASSOC=${L2_ASSOC:-16}
BASELINE_L2_RP=${BASELINE_L2_RP:-TreePLRU}
PAPER_L2_RP=${PAPER_L2_RP:-LRUEmissary}
RL_L2_RP=${RL_L2_RP:-LRUEmissary}
L2_LRU_WAYS=${L2_LRU_WAYS:-8}
L2_PRESERVE_WAYS=${L2_PRESERVE_WAYS:-8}

USE_L3=${USE_L3:-1}
L3_SIZE=${L3_SIZE:-2MiB}
L3_ASSOC=${L3_ASSOC:-16}
L3_RP=${L3_RP:-DRRIP}
L3_RRPV_BITS=${L3_RRPV_BITS:-2}

USE_FDIP=${USE_FDIP:-1}
FDIP_NUM_FTQ_ENTRIES=${FDIP_NUM_FTQ_ENTRIES:-24}
FDIP_FETCH_TARGET_WIDTH=${FDIP_FETCH_TARGET_WIDTH:-192}
FDIP_PFQ_SIZE=${FDIP_PFQ_SIZE:-64}
FDIP_TQ_SIZE=${FDIP_TQ_SIZE:-64}
BP_TYPE=${BP_TYPE:-}

PAPER_EPOCH=${PAPER_EPOCH:-1000000}
PAPER_SAMPLE_RATE=${PAPER_SAMPLE_RATE:-3.125}
PAPER_STARVE_ATLEAST=${PAPER_STARVE_ATLEAST:-1}
PAPER_STARVE_RANDOMNESS=${PAPER_STARVE_RANDOMNESS:-100}

RL_SEEDS=${RL_SEEDS:-"1 2 3"}
RL_SAMPLE_RATE=${RL_SAMPLE_RATE:-12.5}
RL_STARVE_ATLEAST=${RL_STARVE_ATLEAST:-1}
RL_STARVE_RANDOMNESS=${RL_STARVE_RANDOMNESS:-100}
RL_ALPHA=${RL_ALPHA:-0.2}
RL_GAMMA=${RL_GAMMA:-0.8}
RL_EPSILON=${RL_EPSILON:-0.10}
RL_TARGET_SATURATION=${RL_TARGET_SATURATION:-10}
RL_TARGET_OCCUPANCY=${RL_TARGET_OCCUPANCY:-4.0}
RL_ACTION_ADMISSION_RATES=${RL_ACTION_ADMISSION_RATES:-0,3.125,6.25,12.5,25}
RL_ACTION_PRESERVE_WAYS=${RL_ACTION_PRESERVE_WAYS:-0,1,1,2,2}
RL_MIN_PRESERVE_WAYS=${RL_MIN_PRESERVE_WAYS:-0}
RL_DEFAULT_ACTION=${RL_DEFAULT_ACTION:-0}
RL_PRESERVE_GRACE_EPOCHS=${RL_PRESERVE_GRACE_EPOCHS:-32}
RL_REUSE_CAP=${RL_REUSE_CAP:-32.0}
RL_INST_BASELINE_ALPHA=${RL_INST_BASELINE_ALPHA:-0.25}
RL_EXTRA_ARGS=${RL_EXTRA_ARGS:-}

CLEAN_OUTDIR=${CLEAN_OUTDIR:-1}
DRY_RUN=${DRY_RUN:-0}

COMMON_ARGS=(
    "${CONFIG}"
    --kernel="${KERNEL}"
    --disk-image="${DISK_IMAGE}"
    --checkpoint-path="${CHECKPOINT_DIR}"
    --root-device="${ROOT_DEVICE}"
    --machine-type="${MACHINE_TYPE}"
    --os-type="${OS_TYPE}"
    --cpu-type="${CPU_TYPE}"
    --num-cpus="${NUM_CPUS}"
    --mem-size="${MEM_SIZE}"
    --cacheline_size="${CACHELINE_SIZE}"
    --caches --l2cache
    --l1i_size="${L1I_SIZE}"
    --l1d_size="${L1D_SIZE}"
    --l1i_assoc="${L1I_ASSOC}"
    --l1d_assoc="${L1D_ASSOC}"
    --l2_size="${L2_SIZE}"
    --l2_assoc="${L2_ASSOC}"
    --standard-switch="${STANDARD_SWITCH}"
    --warmup-insts="${WARMUP_INSTS}"
    --maxinsts="${MAXINSTS}"
)

if [[ "${RESET_STATS_AFTER_WARMUP}" == "1" ]]; then
    COMMON_ARGS+=(--reset-stats-after-warmup)
fi
if [[ -n "${RESTORE_WITH_CPU}" ]]; then
    COMMON_ARGS+=(--restore-with-cpu="${RESTORE_WITH_CPU}")
fi
if [[ -n "${DTB}" ]]; then
    COMMON_ARGS+=(--dtb-filename="${DTB}")
fi
if [[ -n "${BOOTLOADER}" ]]; then
    COMMON_ARGS+=(--bootloader="${BOOTLOADER}")
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
if [[ -n "${BP_TYPE}" ]]; then
    COMMON_ARGS+=(--bp-type="${BP_TYPE}")
fi

run_case() {
    local name=$1
    shift

    local outdir="${TRACE_ROOT}/${name}"
    echo "==> ${workload}/${name}"
    echo "    checkpoint: ${CHECKPOINT_DIR}"
    echo "    kernel:     ${KERNEL}"
    echo "    disk:       ${DISK_IMAGE}"
    echo "    outdir:     ${outdir}"

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

run_baseline() {
    run_case "baseline/${BASELINE_L2_RP}" \
        --l2_rp="${BASELINE_L2_RP}"
}

run_paper() {
    run_case "paper/${PAPER_L2_RP}" \
        --l2_rp="${PAPER_L2_RP}" \
        --lru_ways="${L2_LRU_WAYS}" \
        --preserve_ways="${L2_PRESERVE_WAYS}" \
        --hist_freq_cycles="${PAPER_EPOCH}" \
        --emissary-enable \
        --emissary-require-iq-empty \
        --emissary-sample-rate="${PAPER_SAMPLE_RATE}" \
        --starveAtleast="${PAPER_STARVE_ATLEAST}" \
        --starveRandomness="${PAPER_STARVE_RANDOMNESS}"
}

run_rl() {
    local seed
    for seed in ${RL_SEEDS}; do
        # shellcheck disable=SC2206
        local extra_args=(${RL_EXTRA_ARGS})
        run_case "rl/${RL_L2_RP}_seed_${seed}" \
            --l2_rp="${RL_L2_RP}" \
            --lru_ways="${L2_LRU_WAYS}" \
            --preserve_ways="${L2_PRESERVE_WAYS}" \
            --hist_freq_cycles="${PAPER_EPOCH}" \
            --emissary-enable \
            --emissary-require-iq-empty \
            --emissary-sample-rate="${RL_SAMPLE_RATE}" \
            --starveAtleast="${RL_STARVE_ATLEAST}" \
            --starveRandomness="${RL_STARVE_RANDOMNESS}" \
            --emissary-rng-seed="${seed}" \
            --q-learning-preserve \
            --q-learning-seed="${seed}" \
            --q-learning-alpha="${RL_ALPHA}" \
            --q-learning-gamma="${RL_GAMMA}" \
            --q-learning-epsilon="${RL_EPSILON}" \
            --q-learning-target-saturation="${RL_TARGET_SATURATION}" \
            --q-learning-target-occupancy="${RL_TARGET_OCCUPANCY}" \
            --q-learning-min-preserve-ways="${RL_MIN_PRESERVE_WAYS}" \
            --q-learning-default-action="${RL_DEFAULT_ACTION}" \
            --q-learning-preserve-grace-epochs="${RL_PRESERVE_GRACE_EPOCHS}" \
            --q-learning-reuse-cap="${RL_REUSE_CAP}" \
            --q-learning-inst-baseline-alpha="${RL_INST_BASELINE_ALPHA}" \
            --q-action-admission-rates="${RL_ACTION_ADMISSION_RATES}" \
            --q-action-preserve-ways="${RL_ACTION_PRESERVE_WAYS}" \
            "${extra_args[@]}"
    done
}

echo "EMISSARY FS reproduction"
echo "  suite: ${suite}"
echo "  workload: ${workload}"
echo "  L2 baseline: ${BASELINE_L2_RP}; paper: ${PAPER_L2_RP}; rl: ${RL_L2_RP}"
echo "  warmup/detail insts: ${WARMUP_INSTS}/${MAXINSTS}; reset_stats_after_warmup=${RESET_STATS_AFTER_WARMUP}"
echo "  FDIP: ${USE_FDIP}; FTQ=${FDIP_NUM_FTQ_ENTRIES}; fetch_target_width=${FDIP_FETCH_TARGET_WIDTH}"

case "${suite}" in
    baseline) run_baseline ;;
    paper) run_paper ;;
    rl) run_rl ;;
    all)
        run_baseline
        run_paper
        run_rl
        ;;
esac
