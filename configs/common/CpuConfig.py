# Copyright (c) 2012, 2017-2018 ARM Limited
# All rights reserved.
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

import m5.objects
from m5 import fatal

from gem5.isas import ISA

isa_string_map = {
    ISA.X86: "X86",
    ISA.ARM: "Arm",
    ISA.RISCV: "Riscv",
    ISA.SPARC: "Sparc",
    ISA.POWER: "Power",
    ISA.MIPS: "Mips",
}


def config_etrace(cpu_cls, cpu_list, options):
    if issubclass(cpu_cls, m5.objects.DerivO3CPU):
        # Assign the same file name to all cpus for now. This must be
        # revisited when creating elastic traces for multi processor systems.
        for cpu in cpu_list:
            # Attach the elastic trace probe listener. Set the protobuf trace
            # file names. Set the dependency window size equal to the cpu it
            # is attached to.
            cpu.traceListener = m5.objects.ElasticTrace(
                instFetchTraceFile=options.inst_trace_file,
                dataDepTraceFile=options.data_trace_file,
                depWindowSize=3 * cpu.numROBEntries,
            )
            # Make the number of entries in the ROB, LQ and SQ very
            # large so that there are no stalls due to resource
            # limitation as such stalls will get captured in the trace
            # as compute delay. For replay, ROB, LQ and SQ sizes are
            # modelled in the Trace CPU.
            cpu.numROBEntries = 512
            cpu.LQEntries = 128
            cpu.SQEntries = 128
    else:
        fatal(
            "%s does not support data dependency tracing. Use a CPU model of"
            " type or inherited from DerivO3CPU.",
            cpu_cls,
        )


def create_fdip_branch_predictor(isa):
    class FDIPBTB(m5.objects.SimpleBTB):
        numEntries = 16 * 1024
        associativity = 8

    bp = m5.objects.BranchPredictor(
        btb=FDIPBTB(),
        conditionalBranchPred=m5.objects.TAGE_SC_L_64KB(),
        requiresBTBHit=True,
        takenOnlyHistory=True,
    )

    if isa == ISA.ARM:
        bp.instShiftAmt = 2
    elif isa == ISA.RISCV:
        bp.instShiftAmt = 1
    else:
        bp.instShiftAmt = 0
    return bp


def config_fdip_emissary(cpu, options, isa):
    if getattr(options, "fdip", False) and hasattr(cpu, "decoupledFrontEnd"):
        cpu.decoupledFrontEnd = True
        cpu.numFTQEntries = options.fdip_num_ftq_entries
        cpu.fetchTargetWidth = options.fdip_fetch_target_width
        if isa == ISA.ARM:
            cpu.minInstSize = 4
        elif isa == ISA.RISCV:
            cpu.minInstSize = 2
        else:
            cpu.minInstSize = 1

    if getattr(options, "emissary_enable", False) or getattr(
        options, "emissary_retirement", False
    ):
        cpu.enableStarvationEMISSARY = True

    cpu.starveRandomness = options.starveRandomness
    cpu.starveAtleast = options.starveAtleast
    cpu.randomStarve = options.randomStarve
    cpu.emissaryRequireIQEmpty = options.emissary_require_iq_empty
    cpu.emissarySampleRate = options.emissary_sample_rate
    cpu.emissaryRngSeed = options.emissary_rng_seed


def retarget_fdip_prefetchers(system, cpu_list):
    for i, cpu in enumerate(cpu_list):
        if i >= len(system.cpu):
            continue
        icache = getattr(system.cpu[i], "icache", None)
        prefetcher = getattr(icache, "prefetcher", None)
        if not isinstance(prefetcher, m5.objects.FetchDirectedPrefetcher):
            continue
        prefetcher.cpu = cpu
        prefetcher._mmus = []
        prefetcher.registerMMU(cpu.mmu)
