# Copyright (c) 2012-2013, 2015-2016 ARM Limited
# Copyright (c) 2020 Barkhausen Institut
# All rights reserved
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2010 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Configure the M5 cache hierarchy config in one place
#

from common import ObjectList
from common.Caches import *

import m5
import sys
from m5.objects import *
from m5.util import fatal

from gem5.isas import ISA


def _parse_number_list(option_name, option_value, convert):
    if option_value is None or option_value == "":
        return []
    try:
        return [
            convert(token)
            for token in option_value.replace(",", " ").split()
            if token
        ]
    except ValueError:
        fatal("Bad value for %s: %s", option_name, option_value)


def _get_hwp(hwp_option):
    if hwp_option == None:
        return NULL

    hwpClass = ObjectList.hwp_list.get(hwp_option)
    return hwpClass()


def _get_cache_opts(level, options):
    opts = {}

    size_attr = f"{level}_size"
    if hasattr(options, size_attr):
        opts["size"] = getattr(options, size_attr)

    assoc_attr = f"{level}_assoc"
    if hasattr(options, assoc_attr):
        opts["assoc"] = getattr(options, assoc_attr)

    prefetcher_attr = f"{level}_hwp_type"
    if hasattr(options, prefetcher_attr):
        opts["prefetcher"] = _get_hwp(getattr(options, prefetcher_attr))

    return opts


def _attach_fdip_prefetcher(options, cpu, icache):
    pf = FetchDirectedPrefetcher(
        use_virtual_addresses=True,
        cpu=cpu,
        pfq_size=options.fdip_pfq_size,
        tq_size=options.fdip_tq_size,
    )
    pf.registerCache(icache)
    pf.registerMMU(cpu.mmu)
    icache.prefetcher = pf


def _make_l3_replacement_policy(options):
    l3_rp = getattr(options, "l3_rp", "LRU")
    rrpv_bits = int(getattr(options, "l3_rrpv_bits", 2))

    if l3_rp in ("LRU", "LRURP"):
        return LRURP()
    if l3_rp in ("Random", "RandomRP"):
        return RandomRP()
    if l3_rp in ("TreePLRU", "TreePLRURP", "PLRU"):
        return TreePLRURP()
    if l3_rp in ("BIP", "BIPRP"):
        rp = BIPRP()
        rp.btp = 3
        return rp
    if l3_rp in ("LIP", "LIPRP"):
        return LIPRP()
    if l3_rp in ("BRRIP", "BRRIPRP"):
        rp = BRRIPRP()
        rp.num_bits = rrpv_bits
        rp.btp = 3
        return rp
    if l3_rp in ("RRIP", "RRIPRP"):
        rp = RRIPRP()
        rp.num_bits = rrpv_bits
        rp.hit_priority = True
        return rp
    if l3_rp in ("DRRIP", "DRRIPRP"):
        rp = DRRIPRP()
        rp.team_size = 16
        rp.constituency_size = 512
        rp.replacement_policy_a.num_bits = rrpv_bits
        rp.replacement_policy_b.num_bits = rrpv_bits
        rp.replacement_policy_a.hit_priority = True
        rp.replacement_policy_b.hit_priority = True
        return rp
    if l3_rp in ("SHiPMem", "SHiPMemRP"):
        rp = SHiPMemRP()
        rp.num_bits = rrpv_bits
        return rp
    if l3_rp in ("SHiPPC", "SHiPPCRP"):
        rp = SHiPPCRP()
        rp.num_bits = rrpv_bits
        return rp

    fatal(f"Unsupported --l3_rp '{l3_rp}'")


def config_cache(options, system):
    if options.external_memory_system and (
        options.caches or options.l2cache or getattr(options, "l3cache", False)
    ):
        print("External caches and internal caches are exclusive options.\n")
        sys.exit(1)

    if getattr(options, "fdip", False) and not options.caches:
        fatal("--fdip requires --caches so the L1I prefetcher can be attached")

    if getattr(options, "l3cache", False) and not options.l2cache:
        fatal("--l3cache requires --l2cache")
    if (
        getattr(options, "l3cache", False)
        and getattr(options, "num_l3caches", 1) != 1
    ):
        fatal("Classic SE --l3cache currently supports --num-l3caches=1")

    if options.external_memory_system:
        ExternalCache = ExternalCacheFactory(options.external_memory_system)

    if options.cpu_type == "O3_ARM_v7a_3":
        try:
            import cores.arm.O3_ARM_v7a as core
        except:
            print("O3_ARM_v7a_3 is unavailable. Did you compile the O3 model?")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            core.O3_ARM_v7a_DCache,
            core.O3_ARM_v7a_ICache,
            core.O3_ARM_v7aL2,
            None,
        )
    elif options.cpu_type == "HPI":
        try:
            import cores.arm.HPI as core
        except:
            print("HPI is unavailable.")
            sys.exit(1)

        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            core.HPI_DCache,
            core.HPI_ICache,
            core.HPI_L2,
            None,
        )
    else:
        dcache_class, icache_class, l2_cache_class, walk_cache_class = (
            L1_DCache,
            L1_ICache,
            L2Cache,
            None,
        )

    # Set the cache line size of the system
    system.cache_line_size = options.cacheline_size

    # If elastic trace generation is enabled, make sure the memory system is
    # minimal so that compute delays do not include memory access latencies.
    # Configure the compulsory L1 caches for the O3CPU, do not configure
    # any more caches.
    if (
        options.l2cache or getattr(options, "l3cache", False)
    ) and options.elastic_trace_en:
        fatal("When elastic trace is enabled, do not configure L2/L3 caches.")

    if options.l2cache:
        # Provide a clock for the L2 and the L1-to-L2 bus here as they
        # are not connected using addTwoLevelCacheHierarchy. Use the
        # same clock as the CPUs.
        system.l2 = l2_cache_class(
            clk_domain=system.cpu_clk_domain, **_get_cache_opts("l2", options)
        )
        emissary_rps = {
            "LRUEmissary": LRUEmissaryRP,
            "LRUEmissaryRP": LRUEmissaryRP,
            "TLRUEmissary": TLRUEmissaryRP,
            "TLRUEmissaryRP": TLRUEmissaryRP,
            "TreeLRUEmissary": TreeLRUEmissaryRP,
            "TreeLRUEmissaryRP": TreeLRUEmissaryRP,
            "OneTreeLRUEmissary": OneTreeLRUEmissaryRP,
            "OneTreeLRUEmissaryRP": OneTreeLRUEmissaryRP,
        }
        l2_rp = getattr(options, "l2_rp", "LRU")
        if l2_rp in emissary_rps:
            rp_class = emissary_rps[l2_rp]
            system.l2.replacement_policy = rp_class()
            assoc = int(system.l2.assoc)
            preserve_ways = max(0, min(int(options.preserve_ways), assoc))
            lru_ways = assoc - preserve_ways
            if hasattr(options, "lru_ways") and options.lru_ways > 0:
                lru_ways = max(1, min(int(options.lru_ways), assoc))
                preserve_ways = assoc - lru_ways
            system.l2.lru_ways = lru_ways
            system.l2.preserve_ways = preserve_ways
            if hasattr(options, "hist_freq_cycles") and options.hist_freq_cycles:
                system.l2.replacement_policy.flush_freq_in_cycles = (
                    options.hist_freq_cycles
                )
            if rp_class is LRUEmissaryRP:
                system.l2.replacement_policy.adaptive_preserve = (
                    options.adaptive_preserve
                )
                system.l2.replacement_policy.adaptive_target_saturation = (
                    options.adaptive_target_saturation
                )
                system.l2.replacement_policy.adaptive_min_preserve_ways = max(
                    0, min(int(options.adaptive_min_preserve_ways), preserve_ways)
                )
                system.l2.replacement_policy.q_learning_preserve = (
                    options.q_learning_preserve
                )
                system.l2.replacement_policy.q_learning_alpha = (
                    options.q_learning_alpha
                )
                system.l2.replacement_policy.q_learning_gamma = (
                    options.q_learning_gamma
                )
                system.l2.replacement_policy.q_learning_epsilon = (
                    options.q_learning_epsilon
                )
                warmup_effective_epochs = max(
                    0,
                    int(options.q_learning_action_warmup_effective_epochs),
                )
                warmup_max_attempts = max(
                    0,
                    int(options.q_learning_action_warmup_max_attempts),
                )
                system.l2.replacement_policy.q_learning_action_warmup_effective_epochs = (
                    warmup_effective_epochs
                )
                system.l2.replacement_policy.q_learning_action_warmup_max_attempts = (
                    warmup_max_attempts
                )
                system.l2.replacement_policy.q_learning_target_saturation = (
                    options.q_learning_target_saturation
                )
                system.l2.replacement_policy.q_learning_target_occupancy = (
                    options.q_learning_target_occupancy
                )
                system.l2.replacement_policy.q_learning_min_preserve_ways = max(
                    0,
                    min(int(options.q_learning_min_preserve_ways), preserve_ways),
                )
                system.l2.replacement_policy.q_learning_default_action = max(
                    0, int(options.q_learning_default_action)
                )
                system.l2.replacement_policy.q_learning_preserve_grace_epochs = max(
                    0, int(options.q_learning_preserve_grace_epochs)
                )
                system.l2.replacement_policy.q_learning_reuse_cap = (
                    options.q_learning_reuse_cap
                )
                system.l2.replacement_policy.q_learning_inst_baseline_alpha = (
                    options.q_learning_inst_baseline_alpha
                )
                q_action_admission_rates = _parse_number_list(
                    "--q-action-admission-rates",
                    options.q_action_admission_rates,
                    float,
                )
                q_action_preserve_ways = _parse_number_list(
                    "--q-action-preserve-ways",
                    options.q_action_preserve_ways,
                    int,
                )
                if q_action_admission_rates or q_action_preserve_ways:
                    if len(q_action_admission_rates) != len(q_action_preserve_ways):
                        fatal(
                            "--q-action-admission-rates and "
                            "--q-action-preserve-ways must have the same length"
                        )
                    system.l2.replacement_policy.q_action_admission_rates = (
                        q_action_admission_rates
                    )
                    system.l2.replacement_policy.q_action_preserve_ways = [
                        max(0, min(int(ways), preserve_ways))
                        for ways in q_action_preserve_ways
                    ]
                system.l2.replacement_policy.q_reward_non_preserve_victim = (
                    options.q_reward_non_preserve_victim
                )
                system.l2.replacement_policy.q_penalty_preserve_victim = (
                    options.q_penalty_preserve_victim
                )
                system.l2.replacement_policy.q_penalty_quota_exceeded = (
                    options.q_penalty_quota_exceeded
                )
                system.l2.replacement_policy.q_penalty_saturation = (
                    options.q_penalty_saturation
                )
                system.l2.replacement_policy.q_reward_preserve_hit = (
                    options.q_reward_preserve_hit
                )
                system.l2.replacement_policy.q_penalty_wasted_rescue = (
                    options.q_penalty_wasted_rescue
                )
                system.l2.replacement_policy.q_reward_inst_fill_reduction = (
                    options.q_reward_inst_fill_reduction
                )
                system.l2.replacement_policy.q_reward_total_fill_reduction = (
                    options.q_reward_total_fill_reduction
                )
                system.l2.replacement_policy.q_penalty_inst_fill_regression = (
                    options.q_penalty_inst_fill_regression
                )
                system.l2.replacement_policy.q_penalty_data_fill_regression = (
                    options.q_penalty_data_fill_regression
                )
                system.l2.replacement_policy.q_penalty_total_fill_regression = (
                    options.q_penalty_total_fill_regression
                )
                system.l2.replacement_policy.q_penalty_admitted_preserve = (
                    options.q_penalty_admitted_preserve
                )
                system.l2.replacement_policy.q_penalty_admission_pressure = (
                    options.q_penalty_admission_pressure
                )
                system.l2.replacement_policy.q_penalty_preserve_clear = (
                    options.q_penalty_preserve_clear
                )
                system.l2.replacement_policy.q_penalty_preserve_occupancy = (
                    options.q_penalty_preserve_occupancy
                )
                system.l2.replacement_policy.q_penalty_inst_fill = (
                    options.q_penalty_inst_fill
                )
                system.l2.replacement_policy.q_penalty_data_fill = (
                    options.q_penalty_data_fill
                )
                system.l2.replacement_policy.q_learning_fill_regression_guard = (
                    not options.q_learning_disable_fill_regression_guard
                )
                system.l2.replacement_policy.q_learning_data_regression_guard = (
                    not options.q_learning_disable_data_regression_guard
                )
                system.l2.replacement_policy.q_learning_data_pollution_guard = (
                    not options.q_learning_disable_data_pollution_guard
                )
                system.l2.replacement_policy.q_learning_data_pollution_threshold = (
                    max(0, int(options.q_learning_data_pollution_threshold))
                )
                system.l2.replacement_policy.q_learning_set_data_pollution_filter = (
                    not options.q_learning_disable_set_data_pollution_filter
                )
                system.l2.replacement_policy.q_learning_set_data_pollution_cooldown = (
                    max(0, int(options.q_learning_set_data_pollution_cooldown))
                )
                system.l2.replacement_policy.q_learning_bad_action_cooldown = (
                    options.q_learning_bad_action_cooldown
                )
                system.l2.replacement_policy.q_learning_action_quality_gate = (
                    not options.q_learning_disable_action_quality_gate
                )
                system.l2.replacement_policy.q_learning_action_quality_alpha = (
                    options.q_learning_action_quality_alpha
                )
                system.l2.replacement_policy.q_learning_min_action_quality = (
                    options.q_learning_min_action_quality
                )
                system.l2.replacement_policy.q_learning_action_quality_recovery = (
                    options.q_learning_action_quality_recovery
                )
                system.l2.replacement_policy.q_learning_action_quality_sample_cap = (
                    options.q_learning_action_quality_sample_cap
                )
                system.l2.replacement_policy.q_learning_quality_data_weight = (
                    options.q_learning_quality_data_weight
                )
                system.l2.replacement_policy.q_learning_quality_total_weight = (
                    options.q_learning_quality_total_weight
                )
                system.l2.replacement_policy.q_learning_quality_preserved_data_weight = (
                    options.q_learning_quality_preserved_data_weight
                )
                system.l2.replacement_policy.q_learning_set_guard = (
                    not options.q_learning_disable_set_guard
                )
                system.l2.replacement_policy.q_learning_seed = (
                    options.q_learning_seed
                )

        system.tol2bus = L2XBar(clk_domain=system.cpu_clk_domain)
        system.l2.cpu_side = system.tol2bus.mem_side_ports

        if getattr(options, "l3cache", False):
            system.l2.writeback_clean = True
            system.l3 = L3Cache(
                clk_domain=system.cpu_clk_domain, **_get_cache_opts("l3", options)
            )
            system.l3.replacement_policy = _make_l3_replacement_policy(options)

            system.tol3bus = L2XBar(clk_domain=system.cpu_clk_domain)
            system.l2.mem_side = system.tol3bus.cpu_side_ports
            system.l3.cpu_side = system.tol3bus.mem_side_ports
            system.l3.mem_side = system.membus.cpu_side_ports
        else:
            system.l2.mem_side = system.membus.cpu_side_ports

    if options.memchecker:
        system.memchecker = MemChecker()

    for i in range(options.num_cpus):
        if options.caches:
            icache = icache_class(**_get_cache_opts("l1i", options))
            dcache = dcache_class(**_get_cache_opts("l1d", options))
            if getattr(options, "fdip", False):
                _attach_fdip_prefetcher(options, system.cpu[i], icache)

            # If we are using ISA.X86 or ISA.RISCV, we set walker caches.
            if ObjectList.cpu_list.get_isa(options.cpu_type) in [
                ISA.RISCV,
                ISA.X86,
            ]:
                iwalkcache = PageTableWalkerCache()
                dwalkcache = PageTableWalkerCache()
            else:
                iwalkcache = None
                dwalkcache = None

            if options.memchecker:
                dcache_mon = MemCheckerMonitor(warn_only=True)
                dcache_real = dcache

                # Do not pass the memchecker into the constructor of
                # MemCheckerMonitor, as it would create a copy; we require
                # exactly one MemChecker instance.
                dcache_mon.memchecker = system.memchecker

                # Connect monitor
                dcache_mon.mem_side = dcache.cpu_side

                # Let CPU connect to monitors
                dcache = dcache_mon

            # When connecting the caches, the clock is also inherited
            # from the CPU in question
            system.cpu[i].addPrivateSplitL1Caches(
                icache, dcache, iwalkcache, dwalkcache
            )

            if options.memchecker:
                # The mem_side ports of the caches haven't been connected yet.
                # Make sure connectAllPorts connects the right objects.
                system.cpu[i].dcache = dcache_real
                system.cpu[i].dcache_mon = dcache_mon

        elif options.external_memory_system:
            # These port names are presented to whatever 'external' system
            # gem5 is connecting to.  Its configuration will likely depend
            # on these names.  For simplicity, we would advise configuring
            # it to use this naming scheme; if this isn't possible, change
            # the names below.
            if ObjectList.cpu_list.get_isa(options.cpu_type) in [
                ISA.X86,
                ISA.ARM,
                ISA.RISCV,
            ]:
                system.cpu[i].addPrivateSplitL1Caches(
                    ExternalCache("cpu%d.icache" % i),
                    ExternalCache("cpu%d.dcache" % i),
                    ExternalCache("cpu%d.itb_walker_cache" % i),
                    ExternalCache("cpu%d.dtb_walker_cache" % i),
                )
            else:
                system.cpu[i].addPrivateSplitL1Caches(
                    ExternalCache("cpu%d.icache" % i),
                    ExternalCache("cpu%d.dcache" % i),
                )

        system.cpu[i].createInterruptController()
        if options.l2cache:
            system.cpu[i].connectAllPorts(
                system.tol2bus.cpu_side_ports,
                system.membus.cpu_side_ports,
                system.membus.mem_side_ports,
            )
        elif options.external_memory_system:
            system.cpu[i].connectUncachedPorts(
                system.membus.cpu_side_ports, system.membus.mem_side_ports
            )
        else:
            system.cpu[i].connectBus(system.membus)

    return system


# ExternalSlave provides a "port", but when that port connects to a cache,
# the connecting CPU SimObject wants to refer to its "cpu_side".
# The 'ExternalCache' class provides this adaptation by rewriting the name,
# eliminating distracting changes elsewhere in the config code.
class ExternalCache(ExternalSlave):
    def __getattr__(cls, attr):
        if attr == "cpu_side":
            attr = "port"
        return super(ExternalSlave, cls).__getattr__(attr)

    def __setattr__(cls, attr, value):
        if attr == "cpu_side":
            attr = "port"
        return super(ExternalSlave, cls).__setattr__(attr, value)


def ExternalCacheFactory(port_type):
    def make(name):
        return ExternalCache(
            port_data=name, port_type=port_type, addr_ranges=[AllMemory]
        )

    return make
