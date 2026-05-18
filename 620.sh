#!/bin/bash

# =========================
# gem5 executable
# =========================
GEM5=/home/lh/gem5-stable/build/X86/gem5.opt

# gem5 config
CONFIG=/home/lh/gem5-stable/configs/deprecated/example/se.py

# checkpoint dir
CKPT_DIR=/home/lh/gem5-bootcamp-env-main/gem5/623_Trace

# output dir
OUT_BASE=/home/lh/gem5-bootcamp-env-main/gem5/623_Trace

# simpoint range
START=1
END=20
MAX_PARALLEL=1
running=0

# =========================
# common args
# =========================
COMMON_ARGS="
--cpu-type=DerivO3CPU
--restore-with-cpu=AtomicSimpleCPU
--cpu-clock=2GHz
--sys-clock=2GHz
--mem-size=8GB
-c xalancbmk_s_base.mytest-m64
--cacheline_size=128
--caches
--l2cache
--l3cache
--l1i_size=32768B
--l1d_size=65536B
--l2_size=1048576B
--l3_size=2097152B
--restore-simpoint-checkpoint
--checkpoint-dir ${CKPT_DIR}
--bp-type=TAGE_SC_L_64KB
"

echo "RUN Simpoint : ${START} to ${END}"

# =========================
# loop checkpoints
# =========================
for (( num=${START}; num<${END}; num++ ))
do
    OUTDIR="${OUT_BASE}/rl_point_${num}"

    mkdir -p "${OUTDIR}"

    echo "================================="
    echo "Run checkpoint = ${CKPT}"
    echo "restore        = ${num}"
    echo "outdir         = ${OUTDIR}"
    echo "================================="

    ${GEM5} \
        --outdir=${OUTDIR} \
        ${CONFIG} \
        ${COMMON_ARGS} \
        --options="-v t5.xml xalanc.xsl" \
        -r ${num}

    ((running++))
    if ((running >= MAX_PARALLEL)); then
        wait -n
        ((running--))
    fi

done

echo "All done."