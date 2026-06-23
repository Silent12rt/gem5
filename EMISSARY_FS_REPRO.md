# EMISSARY FS Reproduction Notes

This repo now has a full-system entry point for comparing:

1. the paper-style baseline,
2. the paper-style EMISSARY policy, and
3. the RL-enhanced EMISSARY policy.

The paper workload package is expected to contain ARM64 Linux checkpoints,
kernel, and disk image files. Download it from:

```text
https://drive.google.com/file/d/1ac60R-nuENQjw-rRBR-0S9rYQEEuCvyp/view?usp=sharing
```

## Build

```bash
scons build/ARM/gem5.opt -j$(nproc)
```

## Docker Environment

Inside your container:

```bash
cd /home/gem5
bash run_emissary_fs_repro_rl.sh list
scons build/ARM/gem5.opt -j$(nproc)
```

If the container is stopped, start it from WSL first:

```bash
sudo docker start -ai gem5_cache
```

If it is already running, enter it from WSL:

```bash
sudo docker exec -it gem5_cache /bin/bash
```

## Run One Workload

```bash
WORKLOAD_ROOT=/path/to/emissary_workloads \
bash run_emissary_fs_repro_rl.sh all tomcat
```

If auto-detection picks the wrong files, pass them explicitly:

```bash
CHECKPOINT_DIR=/path/to/cpt.tomcat \
KERNEL=/path/to/vmlinux \
DISK_IMAGE=/path/to/root.img \
WORKLOAD_ROOT=/path/to/emissary_workloads \
bash run_emissary_fs_repro_rl.sh all tomcat
```

Run all 13 paper workloads:

```bash
WORKLOAD_ROOT=/path/to/emissary_workloads \
bash run_emissary_fs_all_workloads.sh all
```

## Key Defaults

- ARM FS mode: `configs/deprecated/example/fs.py`
- Restore from checkpoint: `--checkpoint-path`
- Warmup: `5,000,000` instructions
- Detailed measurement: `100,000,000` instructions
- Stats reset after warmup: enabled
- L1I/L1D: `32KiB` / `64KiB`, 8-way
- L2: `1MiB`, 16-way
- L3: `2MiB`, 16-way, DRRIP
- FDIP: enabled, 24 FTQ entries, 192-instruction fetch target width

## Compare Results

```bash
bash compare_emissary_fs_repro_rl.sh tomcat
```

## Useful Overrides

```bash
RL_SEEDS="1 2 3 4 5" bash run_emissary_fs_repro_rl.sh rl tomcat
RL_EPSILON=0.05 bash run_emissary_fs_repro_rl.sh rl tomcat
BASELINE_L2_RP=LRU bash run_emissary_fs_repro_rl.sh baseline tomcat
PAPER_L2_RP=TreeLRUEmissary bash run_emissary_fs_repro_rl.sh paper tomcat
RL_EXTRA_ARGS="--q-learning-disable-data-pollution-guard" bash run_emissary_fs_repro_rl.sh rl tomcat
```

If the downloaded checkpoints were created for the paper's QEMU
`virt_machine` gem5 extension, this repo may still need that platform support
ported from the authors' `gem5_FDIP` tree before those checkpoints can restore.
