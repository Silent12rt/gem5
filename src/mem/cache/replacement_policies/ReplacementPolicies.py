# Copyright (c) 2018-2020 Inria
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

from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject


class BaseReplacementPolicy(SimObject):
    type = "BaseReplacementPolicy"
    abstract = True
    cxx_class = "gem5::replacement_policy::Base"
    cxx_header = "mem/cache/replacement_policies/base.hh"


class DuelingRP(BaseReplacementPolicy):
    type = "DuelingRP"
    cxx_class = "gem5::replacement_policy::Dueling"
    cxx_header = "mem/cache/replacement_policies/dueling_rp.hh"

    constituency_size = Param.Unsigned(
        "The size of a region containing one sample"
    )
    team_size = Param.Unsigned(
        "Number of entries in a sampling set that belong to a team"
    )
    replacement_policy_a = Param.BaseReplacementPolicy(
        "Sub-replacement policy A"
    )
    replacement_policy_b = Param.BaseReplacementPolicy(
        "Sub-replacement policy B"
    )


class FIFORP(BaseReplacementPolicy):
    type = "FIFORP"
    cxx_class = "gem5::replacement_policy::FIFO"
    cxx_header = "mem/cache/replacement_policies/fifo_rp.hh"


class SecondChanceRP(FIFORP):
    type = "SecondChanceRP"
    cxx_class = "gem5::replacement_policy::SecondChance"
    cxx_header = "mem/cache/replacement_policies/second_chance_rp.hh"


class LFURP(BaseReplacementPolicy):
    type = "LFURP"
    cxx_class = "gem5::replacement_policy::LFU"
    cxx_header = "mem/cache/replacement_policies/lfu_rp.hh"


class LRURP(BaseReplacementPolicy):
    type = "LRURP"
    cxx_class = "gem5::replacement_policy::LRU"
    cxx_header = "mem/cache/replacement_policies/lru_rp.hh"

class LRUEmissaryRP(BaseReplacementPolicy):
    type = "LRUEmissaryRP"
    cxx_class = "gem5::replacement_policy::LRUEmissary"
    cxx_header = "mem/cache/replacement_policies/lru_emissary_rp.hh"
    lru_ways = Param.Int(Parent.lru_ways, "Number of ways allocated to LRU")
    preserve_ways = Param.Int(
        Parent.preserve_ways, "Number of ways allocated to preserve mode"
    )
    flush_freq_in_cycles = Param.Unsigned(
        0, "Frequency in cycles to flush preserve usage counters"
    )
    max_val = Param.Unsigned(32, "Max replacement age value")
    adaptive_preserve = Param.Bool(False, "Adapt preserve quota by epoch")
    adaptive_target_saturation = Param.Float(
        25.0, "Target percentage of saturated sets for adaptive preserve"
    )
    adaptive_min_preserve_ways = Param.Unsigned(
        1, "Minimum effective preserve ways under adaptive preserve"
    )
    q_learning_preserve = Param.Bool(
        False, "Use tabular Q-learning to choose preserve admission rate"
    )
    q_learning_alpha = Param.Float(0.2, "Q-learning learning rate")
    q_learning_gamma = Param.Float(0.8, "Q-learning discount factor")
    q_learning_epsilon = Param.Float(0.05, "Q-learning exploration rate")
    q_learning_target_saturation = Param.Float(
        10.0, "Saturated-set budget used by Q-learning reward"
    )
    q_learning_target_occupancy = Param.Float(
        1.5, "Preserve occupancy budget used by Q-learning reward"
    )
    q_learning_min_preserve_ways = Param.Unsigned(
        0, "Minimum preserve ways action available to Q-learning"
    )
    q_learning_default_action = Param.Unsigned(
        0, "Initial and tie-break Q-learning action index"
    )
    q_learning_reuse_cap = Param.Float(
        1.5, "Cap applied to preserve reuse per accepted admission"
    )
    q_learning_inst_baseline_alpha = Param.Float(
        0.25, "EWMA alpha for OFF-action instruction-fill baseline"
    )
    q_action_admission_rates = VectorParam.Float(
        [0.0, 1.5625, 3.125, 6.25, 12.5],
        "Q-learning preserve admission-rate actions",
    )
    q_action_preserve_ways = VectorParam.Int(
        [0, 1, 2, 3, 4],
        "Q-learning effective preserve-way actions",
    )
    q_reward_non_preserve_victim = Param.Float(
        0.0, "Reward weight for selecting non-preserve victims"
    )
    q_penalty_preserve_victim = Param.Float(
        6.0, "Penalty weight for selecting preserve victims"
    )
    q_penalty_quota_exceeded = Param.Float(
        0.0, "Penalty weight for sets above the effective preserve quota"
    )
    q_penalty_saturation = Param.Float(
        0.0, "Penalty weight for saturated sets above target"
    )
    q_reward_preserve_hit = Param.Float(
        0.05, "Reward weight for reuse per accepted preserve admission"
    )
    q_reward_inst_fill_reduction = Param.Float(
        10.0, "Reward weight when instruction-side fills fall below OFF baseline"
    )
    q_penalty_inst_fill_regression = Param.Float(
        20.0, "Penalty weight when instruction-side fills exceed OFF baseline"
    )
    q_penalty_admitted_preserve = Param.Float(
        1.5, "Penalty weight for accepted preserve admissions"
    )
    q_penalty_admission_pressure = Param.Float(
        0.05, "Penalty weight for high preserve admission acceptance percentage"
    )
    q_penalty_preserve_clear = Param.Float(
        2.0, "Penalty weight for preserved lines cleared without reuse"
    )
    q_penalty_preserve_occupancy = Param.Float(
        3.0, "Penalty weight for preserve occupancy above target"
    )
    q_penalty_inst_fill = Param.Float(
        0.0, "Penalty weight for instruction-side L2 fills in Q-learning"
    )
    q_penalty_data_fill = Param.Float(
        3.0, "Penalty weight for data-side L2 fills in preserved sets"
    )
    q_learning_set_guard = Param.Bool(
        True, "Reject preserve admissions in sets already at preserve capacity"
    )
    q_learning_seed = Param.Unsigned(0, "Non-zero seed for Q-learning")


class TLRUEmissaryRP(BaseReplacementPolicy):
    type = "TLRUEmissaryRP"
    cxx_class = "gem5::replacement_policy::TLRUEmissary"
    cxx_header = "mem/cache/replacement_policies/tlru_emissary_rp.hh"
    lru_ways = Param.Int(Parent.lru_ways, "Number of ways allocated to LRU")
    preserve_ways = Param.Int(
        Parent.preserve_ways, "Number of ways allocated to preserve mode"
    )
    flush_freq_in_cycles = Param.Unsigned(
        0, "Frequency in cycles to flush preserve usage counters"
    )


class TreeLRUEmissaryRP(BaseReplacementPolicy):
    type = "TreeLRUEmissaryRP"
    cxx_class = "gem5::replacement_policy::TreeLRUEmissary"
    cxx_header = "mem/cache/replacement_policies/tree_lru_emissary_rp.hh"
    lru_ways = Param.Int(Parent.lru_ways, "Number of ways allocated to LRU")
    preserve_ways = Param.Int(
        Parent.preserve_ways, "Number of ways allocated to preserve mode"
    )
    flush_freq_in_cycles = Param.Unsigned(
        0, "Frequency in cycles to flush preserve usage counters"
    )
    num_leaves = Param.Int(Parent.assoc, "Number of leaves in each PLRU tree")


class OneTreeLRUEmissaryRP(BaseReplacementPolicy):
    type = "OneTreeLRUEmissaryRP"
    cxx_class = "gem5::replacement_policy::OneTreeLRUEmissary"
    cxx_header = "mem/cache/replacement_policies/one_tree_lru_emissary_rp.hh"
    lru_ways = Param.Int(Parent.lru_ways, "Number of ways allocated to LRU")
    preserve_ways = Param.Int(
        Parent.preserve_ways, "Number of ways allocated to preserve mode"
    )
    flush_freq_in_cycles = Param.Unsigned(
        0, "Frequency in cycles to flush preserve usage counters"
    )
    num_leaves = Param.Int(Parent.assoc, "Number of leaves in each PLRU tree")


class BIPRP(LRURP):
    type = "BIPRP"
    cxx_class = "gem5::replacement_policy::BIP"
    cxx_header = "mem/cache/replacement_policies/bip_rp.hh"
    btp = Param.Percent(3, "Percentage of blocks to be inserted as MRU")


class LIPRP(BIPRP):
    btp = 0


class MRURP(BaseReplacementPolicy):
    type = "MRURP"
    cxx_class = "gem5::replacement_policy::MRU"
    cxx_header = "mem/cache/replacement_policies/mru_rp.hh"


class RandomRP(BaseReplacementPolicy):
    type = "RandomRP"
    cxx_class = "gem5::replacement_policy::Random"
    cxx_header = "mem/cache/replacement_policies/random_rp.hh"


class BRRIPRP(BaseReplacementPolicy):
    type = "BRRIPRP"
    cxx_class = "gem5::replacement_policy::BRRIP"
    cxx_header = "mem/cache/replacement_policies/brrip_rp.hh"
    num_bits = Param.Int(2, "Number of bits per RRPV")
    hit_priority = Param.Bool(
        False, "Prioritize evicting blocks that havent had a hit recently"
    )
    btp = Param.Percent(
        3, "Percentage of blocks to be inserted with long RRPV"
    )


class RRIPRP(BRRIPRP):
    btp = 100


class DRRIPRP(DuelingRP):
    # The constituency_size and the team_size must be manually provided, where:
    #     constituency_size = num_cache_entries /
    #         (num_dueling_sets * num_entries_per_set)
    # The paper assumes that:
    #     num_dueling_sets = 32
    #     team_size = num_entries_per_set
    replacement_policy_a = BRRIPRP()
    replacement_policy_b = RRIPRP()


class NRURP(BRRIPRP):
    btp = 100
    num_bits = 1


class SHiPRP(BRRIPRP):
    type = "SHiPRP"
    abstract = True
    cxx_class = "gem5::replacement_policy::SHiP"
    cxx_header = "mem/cache/replacement_policies/ship_rp.hh"

    shct_size = Param.Unsigned(16384, "Number of SHCT entries")
    # By default any value greater than 0 is enough to change insertion policy
    insertion_threshold = Param.Percent(
        1, "Percentage at which an entry changes insertion policy"
    )
    # Always make hits mark entries as last to be evicted
    hit_priority = True
    # Let the predictor decide when to change insertion policy
    btp = 0


class SHiPMemRP(SHiPRP):
    type = "SHiPMemRP"
    cxx_class = "gem5::replacement_policy::SHiPMem"
    cxx_header = "mem/cache/replacement_policies/ship_rp.hh"


class SHiPPCRP(SHiPRP):
    type = "SHiPPCRP"
    cxx_class = "gem5::replacement_policy::SHiPPC"
    cxx_header = "mem/cache/replacement_policies/ship_rp.hh"


class TreePLRURP(BaseReplacementPolicy):
    type = "TreePLRURP"
    cxx_class = "gem5::replacement_policy::TreePLRU"
    cxx_header = "mem/cache/replacement_policies/tree_plru_rp.hh"
    num_leaves = Param.Int(Parent.assoc, "Number of leaves in each tree")


class WeightedLRURP(LRURP):
    type = "WeightedLRURP"
    cxx_class = "gem5::replacement_policy::WeightedLRU"
    cxx_header = "mem/cache/replacement_policies/weighted_lru_rp.hh"
