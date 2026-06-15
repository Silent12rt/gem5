/**
 * Copyright (c) 2026
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Declaration of EMISSARY-aware LRU replacement policy.
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_LRU_EMISSARY_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_LRU_EMISSARY_RP_HH__

#include <cstdint>
#include <memory>
#include <vector>

#include "base/random.hh"
#include "base/statistics.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"

namespace gem5
{

struct LRUEmissaryRPParams;

namespace replacement_policy
{

class LRUEmissary : public Base
{
  protected:
    struct LRUEmissaryReplData : ReplacementData
    {
        Tick lastTouchTick;
        CacheBlk *blk;
        bool demandUsedSinceFlush;
        bool protectedFromEviction;
        bool rescueConsumed;
        int preserveGraceEpochs;
        int admissionState;
        int admissionAction;

        explicit LRUEmissaryReplData(CacheBlk *blk)
          : lastTouchTick(0), blk(blk), demandUsedSinceFlush(false),
            protectedFromEviction(false), rescueConsumed(false),
            preserveGraceEpochs(0), admissionState(-1), admissionAction(-1)
        {}
    };

    struct QAction
    {
        double admissionRate;
        int preserveWays;
    };

  public:
    using Params = LRUEmissaryRPParams;

    int lru_ways;
    int preserve_ways;
    int effective_preserve_ways;
    uint64_t last_tick;
    int numSets;
    int numWays;
    uint64_t flush_freq_in_cycles;
    uint32_t max_age;
    bool adaptive_preserve;
    double adaptive_target_saturation;
    int adaptive_min_preserve_ways;
    bool q_learning_preserve;
    double q_learning_alpha;
    double q_learning_gamma;
    double q_learning_epsilon;
    int q_learning_action_warmup_effective_epochs;
    int q_learning_action_warmup_max_attempts;
    double q_learning_target_saturation;
    double q_learning_target_occupancy;
    int q_learning_min_preserve_ways;
    int q_learning_default_action;
    int q_learning_preserve_grace_epochs;
    double q_learning_reuse_cap;
    double q_learning_inst_baseline_alpha;
    double q_reward_non_preserve_victim;
    double q_penalty_preserve_victim;
    double q_penalty_quota_exceeded;
    double q_penalty_saturation;
    double q_reward_preserve_hit;
    double q_penalty_wasted_rescue;
    double q_reward_inst_fill_reduction;
    double q_reward_total_fill_reduction;
    double q_penalty_inst_fill_regression;
    double q_penalty_data_fill_regression;
    double q_penalty_total_fill_regression;
    double q_penalty_admitted_preserve;
    double q_penalty_admission_pressure;
    double q_penalty_preserve_clear;
    double q_penalty_preserve_occupancy;
    double q_penalty_inst_fill;
    double q_penalty_data_fill;
    bool q_learning_fill_regression_guard;
    bool q_learning_data_regression_guard;
    bool q_learning_data_pollution_guard;
    int q_learning_data_pollution_threshold;
    bool q_learning_set_data_pollution_filter;
    int q_learning_set_data_pollution_cooldown;
    int q_learning_bad_action_cooldown;
    bool q_learning_action_quality_gate;
    double q_learning_action_quality_alpha;
    double q_learning_min_action_quality;
    double q_learning_action_quality_recovery;
    double q_learning_action_quality_sample_cap;
    double q_learning_quality_data_weight;
    double q_learning_quality_total_weight;
    double q_learning_quality_preserved_data_weight;
    bool q_learning_set_guard;
    uint64_t q_learning_seed;
    int q_num_actions;
    int q_num_states;
    int q_last_state;
    int q_last_action;
    bool q_has_last;
    bool q_log_header_written;
    bool q_has_inst_fill_off_baseline;
    bool q_has_fill_off_baseline;
    double q_inst_fill_off_baseline;
    double q_data_fill_off_baseline;
    double q_total_fill_off_baseline;
    std::vector<double> q_values;
    std::vector<QAction> q_actions;
    std::vector<int> q_action_cooldowns;
    std::vector<double> q_action_quality;
    std::vector<uint64_t> q_action_warmup_attempts;
    std::vector<uint64_t> q_action_effective_samples;
    std::vector<int> q_set_data_pollution_cooldowns;
    std::vector<uint64_t> q_pending_useful_credits;
    std::vector<uint64_t> q_pending_wasted_penalties;
    mutable std::vector<uint64_t> epoch_protection_events_by_action;
    mutable std::vector<uint64_t> epoch_useful_hits_by_action;
    Random::RandomPtr q_rng;
    mutable uint64_t epoch_preserve_hits;
    mutable uint64_t epoch_demand_preserve_hits;
    mutable uint64_t epoch_useful_preserve_hits;
    mutable uint64_t epoch_protection_events;
    mutable uint64_t epoch_wasted_protections;
    mutable uint64_t epoch_grace_retentions;
    mutable uint64_t epoch_grace_expirations;
    mutable uint64_t epoch_eligible_demand_hits;
    mutable uint64_t epoch_rescue_capacity_rejects;
    mutable uint64_t epoch_one_shot_evictions;
    mutable double epoch_useful_credit_reward;
    mutable double epoch_wasted_credit_penalty;
    mutable uint64_t epoch_admission_accepts;
    mutable uint64_t epoch_admission_rejects;
    mutable uint64_t epoch_preserve_victims;
    mutable uint64_t epoch_non_preserve_victims;
    mutable uint64_t epoch_quota_exceeded_sets;
    mutable uint64_t epoch_preserve_clears;
    mutable uint64_t epoch_inst_fills;
    mutable uint64_t epoch_data_fills;
    mutable uint64_t epoch_data_fills_preserved_set;
    mutable uint64_t epoch_set_data_pollution_marks;
    mutable uint64_t epoch_set_data_pollution_rejects;
    TaggedIndexingPolicy *indexingPolicy;

    mutable struct LRUEmissaryStats : public statistics::Group
    {
        LRUEmissaryStats(statistics::Group* parent);

        statistics::Scalar preserveVictims;
        statistics::Scalar nonPreserveVictims;
        statistics::Scalar quotaExceededSets;
        statistics::Scalar preserveClears;
        statistics::Scalar preserveFlushes;
        statistics::Scalar adaptiveTightens;
        statistics::Scalar adaptiveRelaxes;
        statistics::Scalar qLearningUpdates;
        statistics::Scalar qLearningExplores;
        statistics::Scalar qLearningExploits;
        statistics::Scalar qLearningWarmupSelections;
        statistics::Scalar qLearningTieBreaks;
        statistics::Scalar qLearningEffectiveActionEpochs;
        statistics::Scalar qLearningActionSum;
        statistics::Scalar qLearningPreserveWaySum;
        statistics::Scalar qAdmissionAccepts;
        statistics::Scalar qAdmissionRejects;
        statistics::Scalar qAdmissionGuardRejects;
        statistics::Scalar qInstFills;
        statistics::Scalar qDataFills;
        statistics::Scalar qDataFillsPreservedSet;
        statistics::Scalar qInstFillBaselineUpdates;
        statistics::Scalar qFillRegressionGuardForces;
        statistics::Scalar qDataPollutionGuardForces;
        statistics::Scalar qSetDataPollutionMarks;
        statistics::Scalar qSetDataPollutionRejects;
        statistics::Scalar qBadActionCooldowns;
        statistics::Scalar qCooldownActionSkips;
        statistics::Scalar qActionQualityUpdates;
        statistics::Scalar qActionQualityRecoveries;
        statistics::Scalar qQualityActionBlocks;
        statistics::Scalar qQualityActionForces;
        statistics::Scalar qQualityActionSkips;
        statistics::Scalar qNoEffectEpochs;
        statistics::Scalar qNoEffectActionForces;
        statistics::Scalar preserveHits;
        statistics::Scalar qDemandPreserveHits;
        statistics::Scalar qProtectionEvents;
        statistics::Scalar qUsefulPreserveHits;
        statistics::Scalar qWastedProtections;
        statistics::Scalar qGraceRetentions;
        statistics::Scalar qGraceExpirations;
        statistics::Scalar qEligibleDemandHits;
        statistics::Scalar qRescueCapacityRejects;
        statistics::Scalar qOneShotEvictions;
        statistics::Scalar qUsefulCreditUpdates;
        statistics::Scalar qUsefulCreditReward;
        statistics::Scalar qWastedCreditUpdates;
        statistics::Scalar qWastedCreditPenalty;
        statistics::Scalar qAuxiliaryTouchSuppressions;
    } stats;

    explicit LRUEmissary(const Params &p);
    ~LRUEmissary() = default;

    void invalidate(
        const std::shared_ptr<ReplacementData>& replacement_data) override;
    void touch(
        const std::shared_ptr<ReplacementData>& replacement_data,
        const PacketPtr pkt) override;
    void touch(
        const std::shared_ptr<ReplacementData>& replacement_data) const
        override;
    void reset(
        const std::shared_ptr<ReplacementData>& replacement_data,
        const PacketPtr pkt) override;
    void reset(
        const std::shared_ptr<ReplacementData>& replacement_data) const
        override;
    ReplaceableEntry* getVictim(
        const ReplacementCandidates& candidates) const override;

    std::shared_ptr<ReplacementData> instantiateEntry() override;
    std::shared_ptr<ReplacementData> instantiateEntry(CacheBlk *blk);

    void dumpPreserveHist();
    void checkToFlushPreserveBits();

  private:
    void checkLRU(const std::shared_ptr<ReplacementData>& replacement_data) const;
    void resetAll(const ReplacementCandidates& candidates, bool preservedWays) const;
    ReplaceableEntry* qGetVictim(
        const ReplacementCandidates& candidates) const;
    double qActionToAdmissionRate(int action) const;
    int qActionToPreserveWays(int action) const;
    int countSetPreserves(CacheBlk *blk) const;
    void qEnsureSetDataPollutionState();
    void qMarkSetDataPolluted(CacheBlk *blk);
    bool qSetDataPollutionBlocked(CacheBlk *blk) const;
    bool qApplyAdmission(
        const std::shared_ptr<ReplacementData>& replacement_data,
        const PacketPtr pkt) const;
    int qState(
        double saturatedPct, double preserveOccupancyPct,
        double preserveVictimPct, double preserveReusePerAdmission,
        double admissionAcceptPct) const;
    bool qActionCoolingDown(int action) const;
    bool qActionQualityBlocked(int action) const;
    void qTickActionCooldowns();
    bool qRecoverActionQuality();
    void qClearLineTracking(
        const std::shared_ptr<LRUEmissaryReplData>& repl_data,
        bool countWastedProtection);
    void qRecordAdmission(
        const std::shared_ptr<LRUEmissaryReplData>& repl_data) const;
    double qApplyDelayedRescueCredits();
    int qChooseAction(int state);
    void qUpdate(
        int nextState, double reward, double saturatedPct,
        double preserveOccupancyPct, double preserveVictimPct,
        double preserveReusePerAdmission, double admissionAcceptPct,
        bool actionEffective,
        double reuseReward, double nonPreserveVictimReward,
        double instFillReductionReward, double instFillRegressionPenalty,
        double instFillOffBaseline, double instFillDelta,
        double totalFillReductionReward,
        double dataFillRegressionPenalty,
        double totalFillRegressionPenalty,
        double dataFillOffBaseline, double dataFillDelta,
        double totalFillOffBaseline, double totalFillDelta,
        double instFillPenalty, double dataFillPenalty,
        double preserveVictimPenalty, double admittedPreservePenalty,
        double admissionPressurePenalty, double preserveClearPenalty,
        double preserveOccupancyPenalty, double quotaPenalty,
        double saturationPenalty);
    void qLogEpoch(
        int state, int activeAction, int nextAction, double reward,
        double saturatedPct,
        double preserveOccupancyPct, double preserveVictimPct,
        double preserveReusePerAdmission, double admissionAcceptPct,
        bool actionEffective, bool noEffectEpoch, bool qValueUpdated,
        double reuseReward, double nonPreserveVictimReward,
        double instFillReductionReward, double instFillRegressionPenalty,
        double instFillOffBaseline, double instFillDelta,
        double totalFillReductionReward,
        double dataFillRegressionPenalty,
        double totalFillRegressionPenalty,
        double dataFillOffBaseline, double dataFillDelta,
        double totalFillOffBaseline, double totalFillDelta,
        bool totalFillRegressionGuarded,
        bool dataFillRegressionGuarded,
        bool fillRegressionGuarded,
        bool dataPollutionGuarded,
        int activeActionCooldown,
        double dataPollutionQualityPenalty,
        double actionQualitySample,
        double actionQualityBefore,
        double actionQualityAfter,
        bool actionQualityUpdated,
        bool actionQualityBlocked,
        bool actionQualityRecovered,
        double instFillPenalty, double dataFillPenalty,
        double preserveVictimPenalty, double admittedPreservePenalty,
        double admissionPressurePenalty, double preserveClearPenalty,
        double preserveOccupancyPenalty, double quotaPenalty,
        double saturationPenalty);
    void resetEpochCounters();
};

} // namespace replacement_policy
} // namespace gem5

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_LRU_EMISSARY_RP_HH__
