/home/gem5/build/X86/gem5.opt \
--outdir=/home/gem5/Trace/emissary \
/home/gem5/configs/deprecated/example/se.py \
--cmd=/home/gem5/benchmarks/emissary_effect_test \
--cpu-type=DerivO3CPU \
--caches --l2cache \
--l2_rp=LRUEmissary \
--l2_size=128kB --l2_assoc=8 \
--lru_ways=4 --preserve_ways=4 \
--emissary-enable \
--starveAtleast=2 --starveRandomness=100
