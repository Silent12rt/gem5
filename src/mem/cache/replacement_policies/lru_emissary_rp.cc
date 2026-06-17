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

#include "mem/cache/replacement_policies/lru_emissary_rp.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <map>

#include "base/output.hh"
#include "base/trace.hh"
#include "debug/EMISSARY.hh"
#include "mem/cache/replacement_policies/emissary_rl_control.hh"
#include "params/LRUEmissaryRP.hh"
#include "sim/cur_tick.hh"
#include "sim/core.hh"

namespace gem5
{

namespace replacement_policy
{

LRUEmissary::LRUEmissary(const Params &p)
    : Base(p),
      lru_ways(p.lru_ways),
      preserve_ways(p.preserve_ways),
      effective_preserve_ways(p.preserve_ways),
      last_tick(0),
      numSets(0),
      numWays(0),
      flush_freq_in_cycles(p.flush_freq_in_cycles),
      max_age(p.max_val),
      adaptive_preserve(p.adaptive_preserve),
      adaptive_target_saturation(p.adaptive_target_saturation),
      adaptive_min_preserve_ways(p.adaptive_min_preserve_ways),
      q_learning_preserve(p.q_learning_preserve),
      q_learning_alpha(p.q_learning_alpha),
      q_learning_gamma(p.q_learning_gamma),
      q_learning_epsilon(p.q_learning_epsilon),
      q_learning_action_warmup_effective_epochs(
          p.q_learning_action_warmup_effective_epochs),
      q_learning_action_warmup_max_attempts(
          p.q_learning_action_warmup_max_attempts),
      q_learning_target_saturation(p.q_learning_target_saturation),
      q_learning_target_occupancy(p.q_learning_target_occupancy),
      q_learning_min_preserve_ways(p.q_learning_min_preserve_ways),
      q_learning_default_action(p.q_learning_default_action),
      q_learning_preserve_grace_epochs(
          p.q_learning_preserve_grace_epochs),
      q_learning_reuse_cap(p.q_learning_reuse_cap),
      q_learning_inst_baseline_alpha(p.q_learning_inst_baseline_alpha),
      q_reward_non_preserve_victim(p.q_reward_non_preserve_victim),
      q_penalty_preserve_victim(p.q_penalty_preserve_victim),
      q_penalty_quota_exceeded(p.q_penalty_quota_exceeded),
      q_penalty_saturation(p.q_penalty_saturation),
      q_reward_preserve_hit(p.q_reward_preserve_hit),
      q_penalty_wasted_rescue(p.q_penalty_wasted_rescue),
      q_reward_inst_fill_reduction(p.q_reward_inst_fill_reduction),
      q_reward_total_fill_reduction(p.q_reward_total_fill_reduction),
      q_penalty_inst_fill_regression(p.q_penalty_inst_fill_regression),
      q_penalty_data_fill_regression(p.q_penalty_data_fill_regression),
      q_penalty_total_fill_regression(p.q_penalty_total_fill_regression),
      q_penalty_admitted_preserve(p.q_penalty_admitted_preserve),
      q_penalty_admission_pressure(p.q_penalty_admission_pressure),
      q_penalty_preserve_clear(p.q_penalty_preserve_clear),
      q_penalty_preserve_occupancy(p.q_penalty_preserve_occupancy),
      q_penalty_inst_fill(p.q_penalty_inst_fill),
      q_penalty_data_fill(p.q_penalty_data_fill),
      q_learning_fill_regression_guard(p.q_learning_fill_regression_guard),
      q_learning_data_regression_guard(p.q_learning_data_regression_guard),
      q_learning_data_pollution_guard(p.q_learning_data_pollution_guard),
      q_learning_data_pollution_threshold(
          p.q_learning_data_pollution_threshold),
      q_learning_set_data_pollution_filter(
          p.q_learning_set_data_pollution_filter),
      q_learning_set_data_pollution_cooldown(
          p.q_learning_set_data_pollution_cooldown),
      q_learning_bad_action_cooldown(p.q_learning_bad_action_cooldown),
      q_learning_action_quality_gate(p.q_learning_action_quality_gate),
      q_learning_action_quality_alpha(p.q_learning_action_quality_alpha),
      q_learning_min_action_quality(p.q_learning_min_action_quality),
      q_learning_action_quality_recovery(p.q_learning_action_quality_recovery),
      q_learning_action_quality_sample_cap(
          p.q_learning_action_quality_sample_cap),
      q_learning_quality_data_weight(p.q_learning_quality_data_weight),
      q_learning_quality_total_weight(p.q_learning_quality_total_weight),
      q_learning_quality_preserved_data_weight(
          p.q_learning_quality_preserved_data_weight),
      q_learning_rescue_quality_gate(p.q_learning_rescue_quality_gate),
      q_learning_rescue_quality_min_samples(
          p.q_learning_rescue_quality_min_samples),
      q_learning_min_rescue_useful_pct(p.q_learning_min_rescue_useful_pct),
      q_learning_rescue_quality_cooldown(
          p.q_learning_rescue_quality_cooldown),
      q_learning_global_rescue_quality_gate(
          p.q_learning_global_rescue_quality_gate),
      q_learning_global_rescue_quality_min_samples(
          p.q_learning_global_rescue_quality_min_samples),
      q_learning_global_min_rescue_useful_pct(
          p.q_learning_global_min_rescue_useful_pct),
      q_learning_global_rescue_quality_cooldown(
          p.q_learning_global_rescue_quality_cooldown),
      q_learning_set_guard(p.q_learning_set_guard),
      q_learning_seed(p.q_learning_seed),
      q_num_actions(0),
      q_num_states(162),
      q_last_state(0),
      q_last_action(0),
      q_has_last(false),
      q_log_header_written(false),
      q_has_inst_fill_off_baseline(false),
      q_has_fill_off_baseline(false),
      q_inst_fill_off_baseline(0.0),
      q_data_fill_off_baseline(0.0),
      q_total_fill_off_baseline(0.0),
      q_global_rescue_samples(0),
      q_global_rescue_successes(0),
      q_global_rescue_wastes(0),
      q_global_rescue_quality_cooldown(0),
      q_rng(q_learning_seed ?
            Random::genRandom(static_cast<uint32_t>(q_learning_seed)) :
            Random::genRandom()),
      epoch_preserve_hits(0),
      epoch_demand_preserve_hits(0),
      epoch_useful_preserve_hits(0),
      epoch_protection_events(0),
      epoch_wasted_protections(0),
      epoch_grace_retentions(0),
      epoch_grace_expirations(0),
      epoch_eligible_demand_hits(0),
      epoch_rescue_capacity_rejects(0),
      epoch_one_shot_evictions(0),
      epoch_useful_credit_reward(0.0),
      epoch_wasted_credit_penalty(0.0),
      epoch_admission_accepts(0),
      epoch_admission_rejects(0),
      epoch_preserve_victims(0),
      epoch_non_preserve_victims(0),
      epoch_quota_exceeded_sets(0),
      epoch_preserve_clears(0),
      epoch_inst_fills(0),
      epoch_data_fills(0),
      epoch_data_fills_preserved_set(0),
      epoch_set_data_pollution_marks(0),
      epoch_set_data_pollution_rejects(0),
      indexingPolicy(nullptr),
      stats(this)
{
    if (adaptive_min_preserve_ways > preserve_ways) {
        adaptive_min_preserve_ways = preserve_ways;
    }
    if (q_learning_min_preserve_ways > preserve_ways) {
        q_learning_min_preserve_ways = preserve_ways;
    }
    if (q_learning_min_preserve_ways < 0) {
        q_learning_min_preserve_ways = 0;
    }
    q_learning_preserve_grace_epochs =
        std::max(0, q_learning_preserve_grace_epochs);
    q_learning_action_warmup_effective_epochs =
        std::max(0, q_learning_action_warmup_effective_epochs);
    q_learning_action_warmup_max_attempts = std::max(
        q_learning_action_warmup_effective_epochs,
        q_learning_action_warmup_max_attempts);
    if (q_learning_reuse_cap <= 0.0) {
        q_learning_reuse_cap = 1.0;
    }
    if (q_learning_target_occupancy < 0.0) {
        q_learning_target_occupancy = 0.0;
    }
    q_learning_inst_baseline_alpha = std::max(
        0.0, std::min(1.0, q_learning_inst_baseline_alpha));
    if (q_learning_bad_action_cooldown < 0) {
        q_learning_bad_action_cooldown = 0;
    }
    if (q_learning_data_pollution_threshold < 0) {
        q_learning_data_pollution_threshold = 0;
    }
    if (q_learning_set_data_pollution_cooldown < 0) {
        q_learning_set_data_pollution_cooldown = 0;
    }
    q_learning_action_quality_alpha = std::max(
        0.0, std::min(1.0, q_learning_action_quality_alpha));
    q_learning_action_quality_recovery =
        std::max(0.0, q_learning_action_quality_recovery);
    q_learning_action_quality_sample_cap =
        std::max(0.0, q_learning_action_quality_sample_cap);
    q_learning_quality_preserved_data_weight =
        std::max(0.0, q_learning_quality_preserved_data_weight);
    q_learning_rescue_quality_min_samples =
        std::max(0, q_learning_rescue_quality_min_samples);
    q_learning_min_rescue_useful_pct = std::max(
        0.0, std::min(100.0, q_learning_min_rescue_useful_pct));
    q_learning_rescue_quality_cooldown =
        std::max(0, q_learning_rescue_quality_cooldown);
    q_learning_global_rescue_quality_min_samples =
        std::max(0, q_learning_global_rescue_quality_min_samples);
    q_learning_global_min_rescue_useful_pct = std::max(
        0.0, std::min(100.0, q_learning_global_min_rescue_useful_pct));
    q_learning_global_rescue_quality_cooldown =
        std::max(0, q_learning_global_rescue_quality_cooldown);
    q_penalty_wasted_rescue = std::max(0.0, q_penalty_wasted_rescue);

    const std::size_t actionCount = std::min(
        p.q_action_admission_rates.size(), p.q_action_preserve_ways.size());
    for (std::size_t action = 0; action < actionCount; action++) {
        const double rate = std::max(
            0.0, std::min(100.0, p.q_action_admission_rates[action]));
        const int ways = std::max(
            q_learning_min_preserve_ways,
            std::min(static_cast<int>(p.q_action_preserve_ways[action]),
                preserve_ways));
        q_actions.push_back({rate, ways});
    }
    if (q_actions.empty()) {
        q_actions = {
            {0.0, 0},
            {3.125, 1},
            {6.25, 1},
            {12.5, 2},
            {25.0, 2},
        };
    }
    q_num_actions = static_cast<int>(q_actions.size());
    q_learning_default_action = std::max(
        0, std::min(q_learning_default_action, q_num_actions - 1));
    q_values.assign(q_num_states * q_num_actions, 0.0);
    q_action_cooldowns.assign(q_num_actions, 0);
    q_action_quality.assign(q_num_actions, 0.0);
    q_action_warmup_attempts.assign(q_num_actions, 0);
    q_action_rescue_samples.assign(q_num_actions, 0);
    q_action_rescue_successes.assign(q_num_actions, 0);
    q_action_rescue_wastes.assign(q_num_actions, 0);
    q_rescue_quality_cooldowns.assign(q_num_actions, 0);
    q_pending_useful_credits.assign(q_num_states * q_num_actions, 0);
    q_pending_wasted_penalties.assign(q_num_states * q_num_actions, 0);
    epoch_protection_events_by_action.assign(q_num_actions, 0);
    epoch_useful_hits_by_action.assign(q_num_actions, 0);
    if (q_learning_preserve) {
        q_last_state = qState(0.0, 0.0, 0.0, 0.0, 0.0);
        q_last_action = q_learning_default_action;
        q_has_last = true;
        effective_preserve_ways = qActionToPreserveWays(q_last_action);
    }
    EmissaryRLControl::configure(
        q_learning_preserve,
        q_learning_preserve ? qActionToAdmissionRate(q_last_action) : 0.0);
    registerExitCallback([this]() { dumpPreserveHist(); });
}

void
LRUEmissary::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
{
    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    qClearLineTracking(repl_data, true);
    repl_data->lastTouchTick = Tick(0);
}

void
LRUEmissary::checkLRU(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    if (!repl_data->blk || !indexingPolicy || numWays == 0) {
        return;
    }

    CacheBlk *cur_blk = repl_data->blk;
    const auto set = cur_blk->getSet();
    const bool preserve = cur_blk->isPreserve();

    DPRINTF(EMISSARY, "Age of set %d :", set);
    for (int way = 0; way < numWays; way++) {
        auto *entry = indexingPolicy->getEntry(set, way);
        auto *blk = static_cast<CacheBlk*>(entry);
        auto candidate_repl_data =
            std::static_pointer_cast<LRUEmissaryReplData>(blk->replacementData);
        if ((blk->isPreserve() == preserve) &&
            candidate_repl_data->lastTouchTick > 1) {
            candidate_repl_data->lastTouchTick--;
        }
        DPRINTFR(EMISSARY, " %d", candidate_repl_data->lastTouchTick);
        if (blk->isPreserve()) {
            DPRINTFR(EMISSARY, "(P)");
        }
    }
    DPRINTFR(EMISSARY, "\n");
}

void
LRUEmissary::touch(
    const std::shared_ptr<ReplacementData>& replacement_data,
    const PacketPtr pkt)
{
    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    const bool demandInstructionHit =
        q_learning_preserve && pkt && pkt->isRead() &&
        !pkt->isStarved() && pkt->req && pkt->req->isInstFetch();
    if (demandInstructionHit && repl_data->admissionAction >= 0) {
        repl_data->demandUsedSinceFlush = true;
        if (!repl_data->protectedFromEviction) {
            stats.qEligibleDemandHits++;
            epoch_eligible_demand_hits++;
        }
    }

    if (pkt && pkt->isRead() && repl_data->blk &&
        repl_data->protectedFromEviction) {
        stats.preserveHits++;
        epoch_preserve_hits++;

        if (demandInstructionHit) {
            stats.qDemandPreserveHits++;
            epoch_demand_preserve_hits++;

            if (repl_data->admissionState >= 0 &&
                repl_data->admissionState < q_num_states &&
                repl_data->admissionAction > 0 &&
                repl_data->admissionAction < q_num_actions) {
                const int creditedAction = repl_data->admissionAction;
                const int creditIndex =
                    repl_data->admissionState * q_num_actions +
                    creditedAction;
                q_pending_useful_credits[creditIndex]++;
                qRecordRescueOutcome(creditedAction, true);
                epoch_useful_hits_by_action[creditedAction]++;
                stats.qUsefulPreserveHits++;
                epoch_useful_preserve_hits++;
                repl_data->blk->clearPreserve();
                qClearLineTracking(repl_data, false);
            }
        }
    }
    const bool preserveMark =
        q_learning_preserve && pkt && pkt->isPreserve();
    if (!qApplyAdmission(replacement_data, pkt)) {
        return;
    }
    if (preserveMark &&
        (pkt->isStarved() || pkt->isEmissaryPreAdmitted())) {
        stats.qAuxiliaryTouchSuppressions++;
        return;
    }
    touch(replacement_data);
}

void
LRUEmissary::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    const_cast<LRUEmissary*>(this)->checkToFlushPreserveBits();
    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    repl_data->lastTouchTick = q_learning_preserve ? curTick() : max_age;
}

void
LRUEmissary::reset(
    const std::shared_ptr<ReplacementData>& replacement_data,
    const PacketPtr pkt)
{
    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    if (q_learning_preserve) {
        qClearLineTracking(repl_data, false);
    }
    if (q_learning_preserve && pkt && pkt->isRead() && pkt->req) {
        if (pkt->req->isInstFetch()) {
            stats.qInstFills++;
            epoch_inst_fills++;
        } else {
            stats.qDataFills++;
            epoch_data_fills++;
            if (repl_data->blk && countSetPreserves(repl_data->blk) > 0) {
                stats.qDataFillsPreservedSet++;
                epoch_data_fills_preserved_set++;
                qMarkSetDataPolluted(repl_data->blk);
            }
        }
    }
    qApplyAdmission(replacement_data, pkt);
    reset(replacement_data);
}

void
LRUEmissary::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto *non_const_this = const_cast<LRUEmissary*>(this);
    non_const_this->checkToFlushPreserveBits();
    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    if (q_learning_preserve) {
        repl_data->lastTouchTick = curTick();
    } else {
        checkLRU(replacement_data);
        repl_data->lastTouchTick = max_age;
    }
}

void
LRUEmissary::resetAll(
    const ReplacementCandidates& candidates, bool preservedWays) const
{
    for (const auto& candidate : candidates) {
        auto *blk = static_cast<CacheBlk*>(candidate);
        if (preservedWays == blk->isPreserve()) {
            std::static_pointer_cast<LRUEmissaryReplData>(
                candidate->replacementData)->lastTouchTick = 0;
        }
    }
}

ReplaceableEntry*
LRUEmissary::getVictim(const ReplacementCandidates& candidates) const
{
    assert(!candidates.empty());
    const_cast<LRUEmissary*>(this)->checkToFlushPreserveBits();

    if (q_learning_preserve) {
        return qGetVictim(candidates);
    }

    ReplaceableEntry *victim_not_preserved = candidates[0];
    ReplaceableEntry *preserved_victim = candidates[0];
    ReplaceableEntry *ordinary_lru_victim = candidates[0];
    int num_not_preserved = 0;
    int num_preserved = 0;
    bool reset_preserved = true;
    bool reset_non_preserved = true;

    for (const auto& candidate : candidates) {
        auto repl =
            std::static_pointer_cast<LRUEmissaryReplData>(candidate->replacementData);
        if (repl->lastTouchTick == 0) {
            auto *blk = static_cast<CacheBlk*>(candidate);
            if (blk->isPreserve()) {
                stats.preserveVictims++;
                epoch_preserve_victims++;
            } else {
                stats.nonPreserveVictims++;
                epoch_non_preserve_victims++;
            }
            return candidate;
        }

        auto *blk = static_cast<CacheBlk*>(candidate);
        if (repl->lastTouchTick <
            std::static_pointer_cast<LRUEmissaryReplData>(
                ordinary_lru_victim->replacementData)->lastTouchTick) {
            ordinary_lru_victim = candidate;
        }
        if (blk->isPreserve()) {
            num_preserved++;
            if (num_preserved == 1 ||
                repl->lastTouchTick <
                    std::static_pointer_cast<LRUEmissaryReplData>(
                        preserved_victim->replacementData)->lastTouchTick) {
                preserved_victim = candidate;
                reset_preserved = false;
            }
        } else {
            num_not_preserved++;
            if (num_not_preserved == 1 ||
                repl->lastTouchTick <
                    std::static_pointer_cast<LRUEmissaryReplData>(
                        victim_not_preserved->replacementData)->lastTouchTick) {
                victim_not_preserved = candidate;
                reset_non_preserved = false;
            }
        }
    }

    if (reset_preserved) {
        resetAll(candidates, true);
    }
    if (reset_non_preserved) {
        resetAll(candidates, false);
    }

    ReplaceableEntry *selected_victim = victim_not_preserved;
    if (num_preserved > effective_preserve_ways) {
        stats.preserveVictims++;
        epoch_preserve_victims++;
        selected_victim = preserved_victim;
    } else {
        stats.nonPreserveVictims++;
        epoch_non_preserve_victims++;
    }

    if (q_learning_preserve &&
        selected_victim != ordinary_lru_victim) {
        auto *ordinary_blk = static_cast<CacheBlk*>(ordinary_lru_victim);
        auto ordinary_repl =
            std::static_pointer_cast<LRUEmissaryReplData>(
                ordinary_lru_victim->replacementData);
        if (ordinary_blk->isPreserve() &&
            ordinary_repl->admissionState >= 0 &&
            ordinary_repl->admissionAction > 0 &&
            !ordinary_repl->protectedFromEviction) {
            ordinary_repl->protectedFromEviction = true;
            epoch_protection_events_by_action[
                ordinary_repl->admissionAction]++;
            stats.qProtectionEvents++;
            epoch_protection_events++;
        }
    }

    return selected_victim;
}

ReplaceableEntry*
LRUEmissary::qGetVictim(const ReplacementCandidates& candidates) const
{
    ReplaceableEntry *ordinary_lru_victim = candidates[0];
    bool ordinaryInitialized = false;

    for (const auto& candidate : candidates) {
        auto repl_data =
            std::static_pointer_cast<LRUEmissaryReplData>(
                candidate->replacementData);
        if (repl_data->lastTouchTick == 0) {
            auto *blk = static_cast<CacheBlk*>(candidate);
            if (repl_data->protectedFromEviction || blk->isPreserve()) {
                stats.preserveVictims++;
                epoch_preserve_victims++;
            } else {
                stats.nonPreserveVictims++;
                epoch_non_preserve_victims++;
            }
            return candidate;
        }
        if (!ordinaryInitialized ||
            repl_data->lastTouchTick <
                std::static_pointer_cast<LRUEmissaryReplData>(
                    ordinary_lru_victim->replacementData)->lastTouchTick) {
            ordinary_lru_victim = candidate;
            ordinaryInitialized = true;
        }
    }

    auto *ordinaryBlk = static_cast<CacheBlk*>(ordinary_lru_victim);
    auto ordinaryRepl =
        std::static_pointer_cast<LRUEmissaryReplData>(
            ordinary_lru_victim->replacementData);

    // A rescued line receives exactly one reprieve. If it reaches the LRU
    // position again before a demand reuse, evict it and charge it as wasted.
    if (ordinaryRepl->protectedFromEviction) {
        stats.preserveVictims++;
        epoch_preserve_victims++;
        stats.qOneShotEvictions++;
        epoch_one_shot_evictions++;
        ordinaryBlk->clearPreserve();
        const_cast<LRUEmissary*>(this)->qClearLineTracking(
            ordinaryRepl, true);
        return ordinary_lru_victim;
    }

    const bool rescueEligible =
        ordinaryRepl->admissionState >= 0 &&
        ordinaryRepl->admissionAction > 0 &&
        ordinaryRepl->admissionAction < q_num_actions &&
        !ordinaryRepl->rescueConsumed;
    if (rescueEligible) {
        int activeRescues = 0;
        for (const auto& candidate : candidates) {
            auto repl_data =
                std::static_pointer_cast<LRUEmissaryReplData>(
                    candidate->replacementData);
            activeRescues += repl_data->protectedFromEviction ? 1 : 0;
        }

        const int rescueCapacity =
            qActionToPreserveWays(ordinaryRepl->admissionAction);
        if (activeRescues < rescueCapacity) {
            ReplaceableEntry *alternative = nullptr;
            for (const auto& candidate : candidates) {
                if (candidate == ordinary_lru_victim) {
                    continue;
                }
                auto repl_data =
                    std::static_pointer_cast<LRUEmissaryReplData>(
                        candidate->replacementData);
                if (repl_data->protectedFromEviction) {
                    continue;
                }
                if (!alternative ||
                    repl_data->lastTouchTick <
                        std::static_pointer_cast<LRUEmissaryReplData>(
                            alternative->replacementData)->lastTouchTick) {
                    alternative = candidate;
                }
            }

            if (alternative) {
                ordinaryRepl->protectedFromEviction = true;
                ordinaryRepl->rescueConsumed = true;
                ordinaryRepl->preserveGraceEpochs = 0;
                ordinaryBlk->setPreserve();
                epoch_protection_events_by_action[
                    ordinaryRepl->admissionAction]++;
                stats.qProtectionEvents++;
                epoch_protection_events++;

                auto *alternativeBlk = static_cast<CacheBlk*>(alternative);
                auto alternativeRepl =
                    std::static_pointer_cast<LRUEmissaryReplData>(
                        alternative->replacementData);
                if (alternativeRepl->protectedFromEviction ||
                    alternativeBlk->isPreserve()) {
                    stats.preserveVictims++;
                    epoch_preserve_victims++;
                } else {
                    stats.nonPreserveVictims++;
                    epoch_non_preserve_victims++;
                }
                return alternative;
            }
        } else {
            stats.qRescueCapacityRejects++;
            epoch_rescue_capacity_rejects++;
        }
    }

    if (ordinaryBlk->isPreserve()) {
        ordinaryBlk->clearPreserve();
    }
    stats.nonPreserveVictims++;
    epoch_non_preserve_victims++;
    return ordinary_lru_victim;
}

std::shared_ptr<ReplacementData>
LRUEmissary::instantiateEntry()
{
    return std::make_shared<LRUEmissaryReplData>(nullptr);
}

std::shared_ptr<ReplacementData>
LRUEmissary::instantiateEntry(CacheBlk *blk)
{
    return std::make_shared<LRUEmissaryReplData>(blk);
}

void
LRUEmissary::checkToFlushPreserveBits()
{
    if (!flush_freq_in_cycles || !indexingPolicy) {
        return;
    }

    const uint64_t cur_tick = curTick();
    if (((cur_tick - last_tick) / 500) >= flush_freq_in_cycles) {
        dumpPreserveHist();
        stats.preserveFlushes++;
        last_tick = cur_tick;
    }
}

void
LRUEmissary::dumpPreserveHist()
{
    if (!indexingPolicy || numSets <= 0 || numWays <= 0) {
        return;
    }

    std::ofstream histOut;
    histOut.open(simout.directory() + "/set_hist.csv", std::fstream::app);

    histOut << curTick() << ",";
    std::map<int, int> preserveCountHist;
    for (int i = 0; i < numWays; i++) {
        preserveCountHist[i] = 0;
    }

    int saturatedSets = 0;
    uint64_t totalPreserved = 0;
    for (int set = 0; set < numSets; set++) {
        int numPreserved = 0;
        for (int way = 0; way < numWays; way++) {
            auto *entry = indexingPolicy->getEntry(set, way);
            auto *blk = static_cast<CacheBlk*>(entry);
            auto repl_data =
                std::static_pointer_cast<LRUEmissaryReplData>(
                    entry->replacementData);
            if (q_learning_preserve) {
                if (repl_data->protectedFromEviction) {
                    numPreserved++;
                } else if (repl_data->admissionAction >= 0) {
                    if (repl_data->preserveGraceEpochs > 0) {
                        repl_data->preserveGraceEpochs--;
                        stats.qGraceRetentions++;
                        epoch_grace_retentions++;
                    } else {
                        stats.qGraceExpirations++;
                        epoch_grace_expirations++;
                        stats.preserveClears++;
                        epoch_preserve_clears++;
                        qClearLineTracking(repl_data, false);
                    }
                } else if (blk->isPreserve()) {
                    // RL mode only sets the real preserve bit after rescue.
                    // Clear stale bits propagated from upper cache levels.
                    blk->clearPreserve();
                }
            } else {
                const bool usedSinceFlush = blk->isUsed();
                if (blk->isPreserve() && !usedSinceFlush) {
                    stats.preserveClears++;
                    epoch_preserve_clears++;
                    blk->clearPreserve();
                }
                if (blk->isPreserve()) {
                    numPreserved++;
                }
            }
            repl_data->demandUsedSinceFlush = false;
            blk->clearUsed();
        }
        totalPreserved += numPreserved;

        if (effective_preserve_ways > 0 &&
            numPreserved >= effective_preserve_ways) {
            saturatedSets++;
        }
        if (numPreserved > effective_preserve_ways) {
            stats.quotaExceededSets++;
            epoch_quota_exceeded_sets++;
        }
        if (numPreserved >= preserve_ways) {
            preserveCountHist[preserve_ways]++;
        } else {
            preserveCountHist[numPreserved]++;
        }
    }

    for (int i = 0; i < numWays; i++) {
        histOut << preserveCountHist[i] << ",";
    }
    histOut << "\n";

    const double saturatedPct = 100.0 * saturatedSets / numSets;
    const double preserveCapacity =
        static_cast<double>(numSets) *
        static_cast<double>(std::max(1, effective_preserve_ways));
    const double preserveOccupancyPct = preserveCapacity > 0.0 ?
        100.0 * static_cast<double>(totalPreserved) / preserveCapacity : 0.0;
    const uint64_t victimTotal =
        epoch_preserve_victims + epoch_non_preserve_victims;
    const double preserveVictimPct = victimTotal ?
        100.0 * epoch_preserve_victims / victimTotal : 0.0;
    const int epochAction =
        q_has_last ? q_last_action : q_learning_default_action;
    const uint64_t admissionTotal =
        epoch_admission_accepts + epoch_admission_rejects;
    const double admissionAcceptPct = q_learning_preserve ?
        qActionToAdmissionRate(epochAction) :
        (admissionTotal ?
            100.0 * epoch_admission_accepts / admissionTotal : 0.0);
    const double preserveReusePerAdmission = epoch_admission_accepts ?
        static_cast<double>(epoch_useful_preserve_hits) /
            static_cast<double>(epoch_admission_accepts) : 0.0;
    const double admittedPreserveK =
        static_cast<double>(epoch_admission_accepts) / 1000.0;

    if (q_learning_preserve && numSets > 0) {
        qApplyDelayedRescueCredits();
        const int activeAction = epochAction;
        const bool activeOff =
            qActionToAdmissionRate(activeAction) <= 0.0 ||
            qActionToPreserveWays(activeAction) <= 0;
        const bool actionEffective = epoch_admission_accepts > 0;
        const bool scoreActiveAction = !activeOff && actionEffective;
        const bool causalFillFeedback =
            activeAction >= 0 && activeAction < q_num_actions &&
            (epoch_protection_events_by_action[activeAction] > 0 ||
             epoch_useful_hits_by_action[activeAction] > 0);
        const double epochInstFills =
            static_cast<double>(epoch_inst_fills);
        const double epochDataFills =
            static_cast<double>(epoch_data_fills);
        const double epochTotalFills = epochInstFills + epochDataFills;
        double instFillOffBaseline = q_has_inst_fill_off_baseline ?
            q_inst_fill_off_baseline : -1.0;
        double dataFillOffBaseline = q_has_fill_off_baseline ?
            q_data_fill_off_baseline : -1.0;
        double totalFillOffBaseline = q_has_fill_off_baseline ?
            q_total_fill_off_baseline : -1.0;
        double instFillDelta = 0.0;
        double dataFillDelta = 0.0;
        double totalFillDelta = 0.0;
        double instFillReductionReward = 0.0;
        double totalFillReductionReward = 0.0;
        double instFillRegressionPenalty = 0.0;
        double dataFillRegressionPenalty = 0.0;
        double totalFillRegressionPenalty = 0.0;

        if (!activeOff && actionEffective &&
            q_has_inst_fill_off_baseline) {
            instFillDelta = q_inst_fill_off_baseline -
                epochInstFills;
            if (causalFillFeedback) {
                instFillReductionReward =
                    q_reward_inst_fill_reduction *
                    (std::max(0.0, instFillDelta) / 1000.0);
                instFillRegressionPenalty =
                    q_penalty_inst_fill_regression *
                    (std::max(0.0, -instFillDelta) / 1000.0);
            }
        }

        if (!activeOff && actionEffective && q_has_fill_off_baseline) {
            dataFillDelta = q_data_fill_off_baseline - epochDataFills;
            totalFillDelta = q_total_fill_off_baseline - epochTotalFills;
            if (causalFillFeedback) {
                totalFillReductionReward =
                    q_reward_total_fill_reduction *
                    (std::max(0.0, totalFillDelta) / 1000.0);
                dataFillRegressionPenalty =
                    q_penalty_data_fill_regression *
                    (std::max(0.0, -dataFillDelta) / 1000.0);
                totalFillRegressionPenalty =
                    q_penalty_total_fill_regression *
                    (std::max(0.0, -totalFillDelta) / 1000.0);
            }
        }

        if (activeOff) {
            if (!q_has_inst_fill_off_baseline) {
                q_inst_fill_off_baseline = epochInstFills;
                q_has_inst_fill_off_baseline = true;
            } else {
                q_inst_fill_off_baseline =
                    (1.0 - q_learning_inst_baseline_alpha) *
                        q_inst_fill_off_baseline +
                    q_learning_inst_baseline_alpha * epochInstFills;
            }
            instFillOffBaseline = q_inst_fill_off_baseline;
            if (!q_has_fill_off_baseline) {
                q_data_fill_off_baseline = epochDataFills;
                q_total_fill_off_baseline = epochTotalFills;
                q_has_fill_off_baseline = true;
            } else {
                q_data_fill_off_baseline =
                    (1.0 - q_learning_inst_baseline_alpha) *
                        q_data_fill_off_baseline +
                    q_learning_inst_baseline_alpha * epochDataFills;
                q_total_fill_off_baseline =
                    (1.0 - q_learning_inst_baseline_alpha) *
                        q_total_fill_off_baseline +
                    q_learning_inst_baseline_alpha * epochTotalFills;
            }
            dataFillOffBaseline = q_data_fill_off_baseline;
            totalFillOffBaseline = q_total_fill_off_baseline;
            stats.qInstFillBaselineUpdates++;
        }

        // Rescue outcomes are delayed feedback applied directly to the
        // state-action pair that admitted the line: reuse earns a reward,
        // while eviction before reuse incurs a wasted-rescue penalty.
        const double reuseReward = 0.0;
        const double nonPreserveVictimReward = !scoreActiveAction ? 0.0 :
            q_reward_non_preserve_victim *
                (static_cast<double>(epoch_non_preserve_victims) / 10000.0);
        const double instFillPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_inst_fill *
                (static_cast<double>(epoch_inst_fills) / 1000.0);
        const double dataFillPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_data_fill *
                (static_cast<double>(epoch_data_fills_preserved_set) /
                    1000.0);
        const double preserveVictimPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_preserve_victim *
                (static_cast<double>(epoch_preserve_victims) / 1000.0);
        const double admittedPreservePenalty = !scoreActiveAction ? 0.0 :
            q_penalty_admitted_preserve * admittedPreserveK;
        const double admissionPressurePenalty = !scoreActiveAction ? 0.0 :
            q_penalty_admission_pressure * admissionAcceptPct;
        const double preserveClearPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_preserve_clear *
                (static_cast<double>(epoch_preserve_clears) / 1000.0);
        const double preserveOccupancyPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_preserve_occupancy *
                std::max(0.0,
                    preserveOccupancyPct - q_learning_target_occupancy);
        const double quotaPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_quota_exceeded *
                static_cast<double>(epoch_quota_exceeded_sets);
        const double saturationPenalty = !scoreActiveAction ? 0.0 :
            q_penalty_saturation *
                std::max(0.0, saturatedPct - q_learning_target_saturation);
        const double reward = !scoreActiveAction ? 0.0 :
            reuseReward + nonPreserveVictimReward +
                instFillReductionReward + totalFillReductionReward -
                instFillRegressionPenalty -
                dataFillRegressionPenalty - totalFillRegressionPenalty -
                instFillPenalty - dataFillPenalty -
                preserveVictimPenalty - admittedPreservePenalty -
                admissionPressurePenalty - preserveClearPenalty -
                preserveOccupancyPenalty - quotaPenalty - saturationPenalty;
        const int nextState =
            qState(saturatedPct, preserveOccupancyPct, preserveVictimPct,
                preserveReusePerAdmission, admissionAcceptPct);
        qUpdate(
            nextState, reward, saturatedPct, preserveOccupancyPct,
            preserveVictimPct, preserveReusePerAdmission, admissionAcceptPct,
            actionEffective,
            reuseReward, nonPreserveVictimReward,
            instFillReductionReward, instFillRegressionPenalty,
            instFillOffBaseline, instFillDelta,
            totalFillReductionReward,
            dataFillRegressionPenalty, totalFillRegressionPenalty,
            dataFillOffBaseline, dataFillDelta,
            totalFillOffBaseline, totalFillDelta,
            instFillPenalty, dataFillPenalty, preserveVictimPenalty,
            admittedPreservePenalty, admissionPressurePenalty,
            preserveClearPenalty, preserveOccupancyPenalty, quotaPenalty,
            saturationPenalty);
        resetEpochCounters();
    }

    if (adaptive_preserve && !q_learning_preserve && numSets > 0) {
        if (saturatedPct > adaptive_target_saturation &&
            effective_preserve_ways > adaptive_min_preserve_ways) {
            effective_preserve_ways--;
            stats.adaptiveTightens++;
        } else if (saturatedPct < adaptive_target_saturation / 2.0 &&
                   effective_preserve_ways < preserve_ways) {
            effective_preserve_ways++;
            stats.adaptiveRelaxes++;
        }
    }
}

double
LRUEmissary::qActionToAdmissionRate(int action) const
{
    if (q_actions.empty()) {
        return 100.0;
    }
    action = std::max(0, std::min(action,
        static_cast<int>(q_actions.size()) - 1));
    return q_actions[action].admissionRate;
}

int
LRUEmissary::qActionToPreserveWays(int action) const
{
    if (preserve_ways <= 0) {
        return 0;
    }
    if (q_actions.empty()) {
        return preserve_ways;
    }
    action = std::max(0, std::min(action,
        static_cast<int>(q_actions.size()) - 1));
    const int minWays = std::max(0,
        std::min(q_learning_min_preserve_ways, preserve_ways));
    return std::max(minWays,
        std::min(q_actions[action].preserveWays, preserve_ways));
}

int
LRUEmissary::countSetPreserves(CacheBlk *blk) const
{
    if (!blk || !indexingPolicy || numWays <= 0) {
        return 0;
    }

    const auto set = blk->getSet();
    int count = 0;
    for (int way = 0; way < numWays; way++) {
        auto *entry = indexingPolicy->getEntry(set, way);
        auto *candidate = static_cast<CacheBlk*>(entry);
        if (candidate->isPreserve()) {
            count++;
        }
    }
    return count;
}

void
LRUEmissary::qEnsureSetDataPollutionState()
{
    if (numSets <= 0) {
        return;
    }
    const auto sets = static_cast<std::size_t>(numSets);
    if (q_set_data_pollution_cooldowns.size() != sets) {
        q_set_data_pollution_cooldowns.assign(sets, 0);
    }
}

void
LRUEmissary::qMarkSetDataPolluted(CacheBlk *blk)
{
    if (!q_learning_set_data_pollution_filter ||
        q_learning_set_data_pollution_cooldown <= 0 || !blk) {
        return;
    }

    qEnsureSetDataPollutionState();
    const auto set = static_cast<std::size_t>(blk->getSet());
    if (set >= q_set_data_pollution_cooldowns.size()) {
        return;
    }

    q_set_data_pollution_cooldowns[set] = std::max(
        q_set_data_pollution_cooldowns[set],
        q_learning_set_data_pollution_cooldown);
    stats.qSetDataPollutionMarks++;
    epoch_set_data_pollution_marks++;
}

bool
LRUEmissary::qSetDataPollutionBlocked(CacheBlk *blk) const
{
    if (!q_learning_set_data_pollution_filter || !blk ||
        q_set_data_pollution_cooldowns.empty()) {
        return false;
    }

    const auto set = static_cast<std::size_t>(blk->getSet());
    return set < q_set_data_pollution_cooldowns.size() &&
        q_set_data_pollution_cooldowns[set] > 0;
}

bool
LRUEmissary::qApplyAdmission(
    const std::shared_ptr<ReplacementData>& replacement_data,
    const PacketPtr pkt) const
{
    if (!q_learning_preserve || !pkt || !pkt->isPreserve()) {
        return true;
    }

    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    if (repl_data->admissionAction >= 0) {
        pkt->setPreserve(false);
        if (repl_data->blk && !repl_data->protectedFromEviction) {
            repl_data->blk->clearPreserve();
        }
        return true;
    }

    if (effective_preserve_ways <= 0) {
        pkt->setPreserve(false);
        if (repl_data->blk && repl_data->blk->isPreserve()) {
            repl_data->blk->clearPreserve();
        }
        stats.qAdmissionRejects++;
        epoch_admission_rejects++;
        return false;
    }

    if (qSetDataPollutionBlocked(repl_data->blk)) {
        pkt->setPreserve(false);
        if (repl_data->blk && repl_data->blk->isPreserve()) {
            repl_data->blk->clearPreserve();
        }
        stats.qAdmissionRejects++;
        stats.qSetDataPollutionRejects++;
        epoch_admission_rejects++;
        epoch_set_data_pollution_rejects++;
        return false;
    }

    if (pkt->isEmissaryPreAdmitted()) {
        qRecordAdmission(repl_data);
        pkt->setPreserve(false);
        if (repl_data->blk) {
            repl_data->blk->clearPreserve();
        }
        stats.qAdmissionAccepts++;
        epoch_admission_accepts++;
        return true;
    }

    const double rate =
        qActionToAdmissionRate(q_has_last ? q_last_action : 0);
    const double sample =
        static_cast<double>(q_rng->random<uint32_t>(0, 9999)) / 100.0;
    if (sample < rate) {
        qRecordAdmission(repl_data);
        pkt->setPreserve(false);
        if (repl_data->blk) {
            repl_data->blk->clearPreserve();
        }
        stats.qAdmissionAccepts++;
        epoch_admission_accepts++;
        return true;
    }

    pkt->setPreserve(false);
    if (repl_data->blk && repl_data->blk->isPreserve()) {
        repl_data->blk->clearPreserve();
    }
    stats.qAdmissionRejects++;
    epoch_admission_rejects++;
    return false;
}

void
LRUEmissary::qClearLineTracking(
    const std::shared_ptr<LRUEmissaryReplData>& repl_data,
    bool countWastedProtection)
{
    if (countWastedProtection && repl_data->protectedFromEviction) {
        if (repl_data->admissionState >= 0 &&
            repl_data->admissionState < q_num_states &&
            repl_data->admissionAction > 0 &&
            repl_data->admissionAction < q_num_actions) {
            const int penaltyIndex =
                repl_data->admissionState * q_num_actions +
                repl_data->admissionAction;
            q_pending_wasted_penalties[penaltyIndex]++;
            qRecordRescueOutcome(repl_data->admissionAction, false);
        }
        stats.qWastedProtections++;
        epoch_wasted_protections++;
    }
    repl_data->demandUsedSinceFlush = false;
    repl_data->protectedFromEviction = false;
    repl_data->rescueConsumed = false;
    repl_data->preserveGraceEpochs = 0;
    repl_data->admissionState = -1;
    repl_data->admissionAction = -1;
}

void
LRUEmissary::qRecordRescueOutcome(int action, bool useful)
{
    if (action <= 0 ||
        action >= static_cast<int>(q_action_rescue_samples.size())) {
        return;
    }
    q_action_rescue_samples[action]++;
    q_global_rescue_samples++;
    if (useful) {
        q_action_rescue_successes[action]++;
        q_global_rescue_successes++;
    } else {
        q_action_rescue_wastes[action]++;
        q_global_rescue_wastes++;
    }
    stats.qLearningRescueOutcomeSamples++;

    if (q_learning_rescue_quality_gate &&
        q_learning_rescue_quality_cooldown > 0 &&
        qActionRescueQualityPoor(action)) {
        const bool wasBlocked = qActionRescueQualityBlocked(action);
        q_rescue_quality_cooldowns[action] = std::max(
            q_rescue_quality_cooldowns[action],
            q_learning_rescue_quality_cooldown);
        if (!wasBlocked) {
            stats.qRescueQualityBlocks++;
        }
    }

    if (q_learning_global_rescue_quality_gate &&
        q_learning_global_rescue_quality_cooldown > 0 &&
        qGlobalRescueQualityPoor()) {
        const bool wasBlocked = qGlobalRescueQualityBlocked();
        q_global_rescue_quality_cooldown = std::max(
            q_global_rescue_quality_cooldown,
            q_learning_global_rescue_quality_cooldown);
        if (!wasBlocked) {
            stats.qGlobalRescueQualityBlocks++;
        }
    }
}

void
LRUEmissary::qRecordAdmission(
    const std::shared_ptr<LRUEmissaryReplData>& repl_data) const
{
    repl_data->demandUsedSinceFlush = false;
    repl_data->protectedFromEviction = false;
    repl_data->rescueConsumed = false;
    repl_data->preserveGraceEpochs = q_learning_preserve_grace_epochs;
    repl_data->admissionState =
        q_has_last ? q_last_state : qState(0.0, 0.0, 0.0, 0.0, 0.0);
    repl_data->admissionAction =
        q_has_last ? q_last_action : q_learning_default_action;
}

double
LRUEmissary::qApplyDelayedRescueCredits()
{
    epoch_useful_credit_reward = 0.0;
    epoch_wasted_credit_penalty = 0.0;
    for (std::size_t index = 0;
         index < q_pending_useful_credits.size(); index++) {
        const uint64_t hits = q_pending_useful_credits[index];
        const uint64_t wasted = q_pending_wasted_penalties[index];
        if (hits == 0 && wasted == 0) {
            continue;
        }

        const double creditedHits = std::min(
            static_cast<double>(hits), q_learning_reuse_cap);
        const double penalizedWasted = std::min(
            static_cast<double>(wasted), q_learning_reuse_cap);
        const double creditReward =
            q_reward_preserve_hit * creditedHits;
        const double wastedPenalty =
            q_penalty_wasted_rescue * penalizedWasted;
        const double delayedReward = creditReward - wastedPenalty;
        double& oldValue = q_values[index];
        oldValue += q_learning_alpha * (delayedReward - oldValue);

        epoch_useful_credit_reward += creditReward;
        epoch_wasted_credit_penalty += wastedPenalty;
        if (hits > 0) {
            stats.qUsefulCreditUpdates++;
            stats.qUsefulCreditReward += creditReward;
        }
        if (wasted > 0) {
            stats.qWastedCreditUpdates++;
            stats.qWastedCreditPenalty += wastedPenalty;
        }
        q_pending_useful_credits[index] = 0;
        q_pending_wasted_penalties[index] = 0;
    }
    return epoch_useful_credit_reward - epoch_wasted_credit_penalty;
}

int
LRUEmissary::qState(
    double saturatedPct, double preserveOccupancyPct,
    double preserveVictimPct, double preserveReusePerAdmission,
    double admissionAcceptPct) const
{
    const int saturationBin =
        saturatedPct < 10.0 ? 0 : (saturatedPct < 50.0 ? 1 : 2);
    const double targetOccupancy =
        std::max(1.0, q_learning_target_occupancy);
    const int occupancyBin =
        preserveOccupancyPct < targetOccupancy ? 0 :
            (preserveOccupancyPct < targetOccupancy * 2.0 ? 1 : 2);
    const int victimBin = preserveVictimPct < 5.0 ? 0 : 1;
    const int reuseBin =
        preserveReusePerAdmission < 0.5 ? 0 :
            (preserveReusePerAdmission < 2.0 ? 1 : 2);
    const int admissionBin =
        admissionAcceptPct < 10.0 ? 0 : (admissionAcceptPct < 40.0 ? 1 : 2);
    return (((saturationBin * 3 + occupancyBin) * 2 + victimBin) * 3 +
        reuseBin) * 3 + admissionBin;
}

bool
LRUEmissary::qActionCoolingDown(int action) const
{
    return action > 0 &&
        action < static_cast<int>(q_action_cooldowns.size()) &&
        q_action_cooldowns[action] > 0;
}

bool
LRUEmissary::qActionQualityBlocked(int action) const
{
    return q_learning_action_quality_gate &&
        action > 0 &&
        action < static_cast<int>(q_action_quality.size()) &&
        q_action_quality[action] < q_learning_min_action_quality;
}

double
LRUEmissary::qActionRescueUsefulPct(int action) const
{
    if (action <= 0 ||
        action >= static_cast<int>(q_action_rescue_samples.size())) {
        return 0.0;
    }
    const uint64_t samples = q_action_rescue_samples[action];
    if (samples == 0) {
        return 0.0;
    }
    return 100.0 *
        (static_cast<double>(q_action_rescue_successes[action]) /
         static_cast<double>(samples));
}

bool
LRUEmissary::qActionRescueQualityPoor(int action) const
{
    if (!q_learning_rescue_quality_gate ||
        q_learning_rescue_quality_min_samples <= 0 ||
        q_learning_min_rescue_useful_pct <= 0.0 ||
        action <= 0 ||
        action >= static_cast<int>(q_action_rescue_samples.size())) {
        return false;
    }

    const uint64_t samples = q_action_rescue_samples[action];
    if (samples <
        static_cast<uint64_t>(q_learning_rescue_quality_min_samples)) {
        return false;
    }

    return qActionRescueUsefulPct(action) < q_learning_min_rescue_useful_pct;
}

bool
LRUEmissary::qActionRescueQualityBlocked(int action) const
{
    return q_learning_rescue_quality_gate &&
        action > 0 &&
        action < static_cast<int>(q_rescue_quality_cooldowns.size()) &&
        q_rescue_quality_cooldowns[action] > 0;
}

double
LRUEmissary::qGlobalRescueUsefulPct() const
{
    if (q_global_rescue_samples == 0) {
        return 0.0;
    }
    return 100.0 *
        (static_cast<double>(q_global_rescue_successes) /
         static_cast<double>(q_global_rescue_samples));
}

bool
LRUEmissary::qGlobalRescueQualityPoor() const
{
    if (!q_learning_global_rescue_quality_gate ||
        q_learning_global_rescue_quality_min_samples <= 0 ||
        q_learning_global_min_rescue_useful_pct <= 0.0) {
        return false;
    }

    if (q_global_rescue_samples <
        static_cast<uint64_t>(
            q_learning_global_rescue_quality_min_samples)) {
        return false;
    }

    return qGlobalRescueUsefulPct() <
        q_learning_global_min_rescue_useful_pct;
}

bool
LRUEmissary::qGlobalRescueQualityBlocked() const
{
    return q_learning_global_rescue_quality_gate &&
        q_global_rescue_quality_cooldown > 0;
}

void
LRUEmissary::qTickActionCooldowns()
{
    for (std::size_t action = 1; action < q_action_cooldowns.size();
         action++) {
        if (q_action_cooldowns[action] > 0) {
            q_action_cooldowns[action]--;
        }
    }
    for (std::size_t action = 1; action < q_rescue_quality_cooldowns.size();
         action++) {
        if (q_rescue_quality_cooldowns[action] > 0) {
            q_rescue_quality_cooldowns[action]--;
        }
    }
    if (q_global_rescue_quality_cooldown > 0) {
        q_global_rescue_quality_cooldown--;
    }
    for (auto& cooldown : q_set_data_pollution_cooldowns) {
        if (cooldown > 0) {
            cooldown--;
        }
    }
}

bool
LRUEmissary::qRecoverActionQuality()
{
    if (!q_learning_action_quality_gate ||
        q_learning_action_quality_recovery <= 0.0) {
        return false;
    }

    bool recovered = false;
    for (std::size_t action = 1; action < q_action_quality.size();
         action++) {
        if (q_action_quality[action] < 0.0) {
            q_action_quality[action] = std::min(
                0.0, q_action_quality[action] +
                    q_learning_action_quality_recovery);
            stats.qActionQualityRecoveries++;
            recovered = true;
        }
    }
    return recovered;
}

int
LRUEmissary::qChooseAction(int state)
{
    if (qGlobalRescueQualityBlocked()) {
        stats.qGlobalRescueQualitySkips++;
        return 0;
    }

    std::vector<int> candidates;
    candidates.reserve(q_num_actions);
    for (int action = 0; action < q_num_actions; action++) {
        if (qActionCoolingDown(action)) {
            stats.qCooldownActionSkips++;
            continue;
        }
        if (qActionQualityBlocked(action)) {
            stats.qQualityActionSkips++;
            continue;
        }
        if (qActionRescueQualityBlocked(action)) {
            stats.qRescueQualitySkips++;
            continue;
        }
        candidates.push_back(action);
    }
    if (candidates.empty()) {
        candidates.push_back(0);
    }

    std::vector<int> warmupCandidates;
    for (const int action : candidates) {
        if (action <= 0 ||
            q_learning_action_warmup_effective_epochs <= 0 ||
            q_learning_action_warmup_max_attempts <= 0) {
            continue;
        }
        if (q_action_rescue_samples[action] <
                static_cast<uint64_t>(
                    q_learning_action_warmup_effective_epochs) &&
            q_action_warmup_attempts[action] <
                static_cast<uint64_t>(
                    q_learning_action_warmup_max_attempts)) {
            warmupCandidates.push_back(action);
        }
    }
    if (!warmupCandidates.empty()) {
        const int index = q_rng->random<int>(
            0, static_cast<int>(warmupCandidates.size()) - 1);
        const int action = warmupCandidates[index];
        q_action_warmup_attempts[action]++;
        stats.qLearningWarmupSelections++;
        return action;
    }

    const double explore =
        static_cast<double>(q_rng->random<uint32_t>(0, 9999)) / 10000.0;
    if (explore < q_learning_epsilon) {
        stats.qLearningExplores++;
        const int index = q_rng->random<int>(
            0, static_cast<int>(candidates.size()) - 1);
        return candidates[index];
    }

    stats.qLearningExploits++;
    constexpr double tieEpsilon = 1.0e-12;
    double bestValue = -std::numeric_limits<double>::infinity();
    std::vector<int> bestActions;
    for (const int action : candidates) {
        const double value = q_values[state * q_num_actions + action];
        if (value > bestValue + tieEpsilon) {
            bestValue = value;
            bestActions.clear();
            bestActions.push_back(action);
        } else if (std::abs(value - bestValue) <= tieEpsilon) {
            bestActions.push_back(action);
        }
    }
    if (bestActions.size() > 1) {
        stats.qLearningTieBreaks++;
        for (const int action : bestActions) {
            if (qActionToAdmissionRate(action) <= 0.0 ||
                qActionToPreserveWays(action) <= 0) {
                return action;
            }
        }
    }
    const int bestIndex = q_rng->random<int>(
        0, static_cast<int>(bestActions.size()) - 1);
    return bestActions[bestIndex];
}

void
LRUEmissary::qUpdate(
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
    double saturationPenalty)
{
    const int activeAction =
        q_has_last ? q_last_action : q_learning_default_action;
    const bool activeOff =
        qActionToAdmissionRate(activeAction) <= 0.0 ||
        qActionToPreserveWays(activeAction) <= 0;
    const bool noEffectEpoch = !activeOff && !actionEffective;
    const bool totalFillRegressionGuarded =
        q_learning_fill_regression_guard && q_has_fill_off_baseline &&
        !activeOff && actionEffective && totalFillDelta < 0.0;
    const bool dataFillRegressionGuarded =
        q_learning_fill_regression_guard &&
        q_learning_data_regression_guard && q_has_fill_off_baseline &&
        !activeOff && actionEffective && dataFillDelta < 0.0;
    const bool fillRegressionGuarded =
        totalFillRegressionGuarded || dataFillRegressionGuarded;
    const bool dataPollutionGuarded =
        q_learning_data_pollution_guard && !activeOff && actionEffective &&
        epoch_data_fills_preserved_set >
            static_cast<uint64_t>(q_learning_data_pollution_threshold);
    const bool guardForced =
        fillRegressionGuarded || dataPollutionGuarded;
    const double dataPollutionQualityPenalty =
        q_learning_quality_preserved_data_weight *
            (static_cast<double>(epoch_data_fills_preserved_set) / 1000.0);

    int activeActionCooldown = 0;
    if (guardForced &&
        q_learning_bad_action_cooldown > 0 &&
        activeAction > 0 &&
        activeAction < static_cast<int>(q_action_cooldowns.size())) {
        q_action_cooldowns[activeAction] = std::max(
            q_action_cooldowns[activeAction],
            q_learning_bad_action_cooldown);
        activeActionCooldown = q_action_cooldowns[activeAction];
        stats.qBadActionCooldowns++;
    }

    double actionQualitySample = 0.0;
    double actionQualityBefore =
        activeAction < static_cast<int>(q_action_quality.size()) ?
            q_action_quality[activeAction] : 0.0;
    double actionQualityAfter = actionQualityBefore;
    bool actionQualityUpdated = false;
    bool actionQualityBlocked = false;
    bool actionQualityRecovered = false;

    if (activeOff) {
        actionQualityRecovered = qRecoverActionQuality();
        actionQualityAfter =
            activeAction < static_cast<int>(q_action_quality.size()) ?
                q_action_quality[activeAction] : actionQualityBefore;
    } else if (!noEffectEpoch && q_learning_action_quality_gate &&
               q_has_fill_off_baseline &&
               activeAction > 0 &&
               activeAction < static_cast<int>(q_action_quality.size())) {
        const bool wasBlocked =
            q_action_quality[activeAction] < q_learning_min_action_quality;
        const double fillQuality =
            q_learning_quality_data_weight *
                std::min(0.0, dataFillDelta / 1000.0) +
            q_learning_quality_total_weight *
                std::min(0.0, totalFillDelta / 1000.0);
        actionQualitySample =
            reward + fillQuality - dataPollutionQualityPenalty;
        if (q_learning_action_quality_sample_cap > 0.0) {
            actionQualitySample = std::max(
                -q_learning_action_quality_sample_cap,
                std::min(q_learning_action_quality_sample_cap,
                    actionQualitySample));
        }
        actionQualityAfter =
            (1.0 - q_learning_action_quality_alpha) *
                q_action_quality[activeAction] +
            q_learning_action_quality_alpha * actionQualitySample;
        q_action_quality[activeAction] = actionQualityAfter;
        actionQualityUpdated = true;
        actionQualityBlocked = qActionQualityBlocked(activeAction);
        stats.qActionQualityUpdates++;
        if (!wasBlocked && actionQualityBlocked) {
            stats.qQualityActionBlocks++;
        }
    }

    const bool rescueQualityBlocked =
        qActionRescueQualityBlocked(activeAction);
    const bool globalRescueQualityBlocked =
        qGlobalRescueQualityBlocked();

    bool qValueUpdated = false;
    if (q_has_last && !noEffectEpoch) {
        double nextBest = -std::numeric_limits<double>::infinity();
        for (int action = 0; action < q_num_actions; action++) {
            if (globalRescueQualityBlocked && action > 0) {
                continue;
            }
            if (qActionCoolingDown(action)) {
                continue;
            }
            if (qActionQualityBlocked(action)) {
                continue;
            }
            if (qActionRescueQualityBlocked(action)) {
                continue;
            }
            nextBest = std::max(
                nextBest, q_values[nextState * q_num_actions + action]);
        }
        if (nextBest == -std::numeric_limits<double>::infinity()) {
            nextBest = q_values[nextState * q_num_actions];
        }

        double& oldValue = q_values[q_last_state * q_num_actions + q_last_action];
        oldValue += q_learning_alpha *
            (reward + q_learning_gamma * nextBest - oldValue);
        stats.qLearningUpdates++;
        qValueUpdated = true;
    }

    int nextAction = 0;
    if (noEffectEpoch) {
        stats.qNoEffectEpochs++;
        stats.qNoEffectActionForces++;
    }
    if (fillRegressionGuarded) {
        stats.qFillRegressionGuardForces++;
    }
    if (dataPollutionGuarded) {
        stats.qDataPollutionGuardForces++;
    }
    if (actionQualityBlocked) {
        stats.qQualityActionForces++;
    }
    if (rescueQualityBlocked) {
        stats.qRescueQualityForces++;
    }
    if (globalRescueQualityBlocked) {
        stats.qGlobalRescueQualityForces++;
    }
    if (noEffectEpoch || guardForced || actionQualityBlocked ||
        rescueQualityBlocked || globalRescueQualityBlocked) {
        nextAction = 0;
    } else {
        nextAction = qChooseAction(nextState);
    }
    const int nextPreserveWays = qActionToPreserveWays(nextAction);
    qLogEpoch(
        nextState, activeAction, nextAction, reward, saturatedPct,
        preserveOccupancyPct,
        preserveVictimPct, preserveReusePerAdmission, admissionAcceptPct,
        actionEffective, noEffectEpoch, qValueUpdated,
        reuseReward, nonPreserveVictimReward,
        instFillReductionReward, instFillRegressionPenalty,
        instFillOffBaseline, instFillDelta,
        totalFillReductionReward,
        dataFillRegressionPenalty, totalFillRegressionPenalty,
        dataFillOffBaseline, dataFillDelta,
        totalFillOffBaseline, totalFillDelta,
        totalFillRegressionGuarded,
        dataFillRegressionGuarded,
        fillRegressionGuarded,
        dataPollutionGuarded,
        activeActionCooldown,
        dataPollutionQualityPenalty,
        actionQualitySample,
        actionQualityBefore,
        actionQualityAfter,
        actionQualityUpdated,
        actionQualityBlocked,
        actionQualityRecovered,
        instFillPenalty, dataFillPenalty, preserveVictimPenalty,
        admittedPreservePenalty, admissionPressurePenalty,
        preserveClearPenalty, preserveOccupancyPenalty, quotaPenalty,
        saturationPenalty);
    q_last_state = nextState;
    q_last_action = nextAction;
    q_has_last = true;
    effective_preserve_ways = nextPreserveWays;
    EmissaryRLControl::setAdmissionRate(
        qActionToAdmissionRate(nextAction));
    stats.qLearningActionSum +=
        static_cast<uint64_t>(qActionToAdmissionRate(nextAction) * 1000.0);
    stats.qLearningPreserveWaySum += nextPreserveWays;
    qTickActionCooldowns();
}

void
LRUEmissary::qLogEpoch(
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
    double saturationPenalty)
{
    std::ofstream qOut;
    qOut.open(simout.directory() + "/q_learning.csv", std::fstream::app);
    const bool activeActionInRange =
        activeAction >= 0 && activeAction < q_num_actions;
    const uint64_t activeActionProtectionEvents = activeActionInRange ?
        epoch_protection_events_by_action[activeAction] : 0;
    const uint64_t activeActionUsefulHits = activeActionInRange ?
        epoch_useful_hits_by_action[activeAction] : 0;
    const uint64_t activeActionRescueSamples = activeActionInRange ?
        q_action_rescue_samples[activeAction] : 0;
    const uint64_t activeActionRescueUseful = activeActionInRange ?
        q_action_rescue_successes[activeAction] : 0;
    const uint64_t activeActionRescueWasted = activeActionInRange ?
        q_action_rescue_wastes[activeAction] : 0;
    const double activeActionRescueUsefulPct = activeActionInRange ?
        qActionRescueUsefulPct(activeAction) : 0.0;
    const bool activeActionRescueQualityBlocked = activeActionInRange ?
        qActionRescueQualityBlocked(activeAction) : false;
    const int activeActionRescueQualityCooldown = activeActionInRange ?
        q_rescue_quality_cooldowns[activeAction] : 0;
    const bool globalRescueQualityBlocked = qGlobalRescueQualityBlocked();
    const double globalRescueUsefulPct = qGlobalRescueUsefulPct();
    const bool causalFillFeedback =
        activeActionProtectionEvents > 0 || activeActionUsefulHits > 0;
    if (!q_log_header_written) {
        qOut << "tick,state,action,admission_rate,effective_preserve_ways,"
             << "next_action,next_admission_rate,next_effective_preserve_ways,"
             << "reward,"
             << "saturated_pct,preserve_occupancy_pct,preserve_victim_pct,"
             << "preserve_victims,non_preserve_victims,preserve_hits,"
             << "demand_preserve_hits,useful_preserve_hits,"
             << "protection_events,wasted_protections,"
             << "grace_retentions,grace_expirations,"
             << "eligible_demand_hits,rescue_capacity_rejects,"
             << "one_shot_evictions,"
             << "useful_credit_reward,wasted_credit_penalty,"
             << "delayed_rescue_reward,"
             << "active_action_protection_events,"
             << "active_action_useful_hits,causal_fill_feedback,"
             << "inst_fills,data_fills,total_fills,data_fills_preserved_set,"
              << "admission_accepts,admission_rejects,admission_accept_pct,"
              << "action_effective,action_rescue_samples,"
              << "action_rescue_useful_samples,"
              << "action_rescue_wasted_samples,action_rescue_useful_pct,"
              << "action_rescue_quality_blocked,"
              << "action_rescue_quality_cooldown,"
              << "global_rescue_samples,global_rescue_useful_samples,"
              << "global_rescue_wasted_samples,global_rescue_useful_pct,"
              << "global_rescue_quality_blocked,"
              << "global_rescue_quality_cooldown,action_warmup_attempts,"
              << "no_effect_epoch,q_value_updated,"
              << "set_data_pollution_marks,set_data_pollution_rejects,"
             << "preserve_reuse_per_admission,preserve_clears,"
             << "quota_exceeded_sets,"
             << "reuse_reward,non_preserve_victim_reward,"
             << "inst_fill_reduction_reward,inst_fill_regression_penalty,"
             << "inst_fill_off_baseline,inst_fill_delta,"
             << "total_fill_reduction_reward,data_fill_regression_penalty,"
             << "total_fill_regression_penalty,"
             << "data_fill_off_baseline,data_fill_delta,"
             << "total_fill_off_baseline,total_fill_delta,"
             << "total_fill_regression_guard,"
             << "data_fill_regression_guard,"
             << "fill_regression_guard,data_pollution_guard,"
             << "active_action_cooldown,"
             << "data_pollution_quality_penalty,"
             << "action_quality_sample,action_quality_before,"
             << "action_quality_after,action_quality_updated,"
             << "action_quality_blocked,action_quality_recovered,"
             << "inst_fill_penalty,data_fill_penalty,"
             << "preserve_victim_penalty,admitted_preserve_penalty,"
             << "admission_pressure_penalty,preserve_clear_penalty,"
             << "preserve_occupancy_penalty,quota_penalty,"
             << "saturation_penalty\n";
        q_log_header_written = true;
    }
    qOut << curTick() << "," << state << "," << activeAction << ","
         << qActionToAdmissionRate(activeAction) << ","
         << qActionToPreserveWays(activeAction) << ","
         << nextAction << "," << qActionToAdmissionRate(nextAction) << ","
         << qActionToPreserveWays(nextAction) << "," << reward << ","
         << saturatedPct << "," << preserveOccupancyPct << ","
         << preserveVictimPct << "," << epoch_preserve_victims << ","
         << epoch_non_preserve_victims << "," << epoch_preserve_hits
         << "," << epoch_demand_preserve_hits
         << "," << epoch_useful_preserve_hits
         << "," << epoch_protection_events
         << "," << epoch_wasted_protections
         << "," << epoch_grace_retentions
         << "," << epoch_grace_expirations
         << "," << epoch_eligible_demand_hits
         << "," << epoch_rescue_capacity_rejects
         << "," << epoch_one_shot_evictions
         << "," << epoch_useful_credit_reward
         << "," << epoch_wasted_credit_penalty
         << ","
         << (epoch_useful_credit_reward - epoch_wasted_credit_penalty)
         << "," << activeActionProtectionEvents
         << "," << activeActionUsefulHits
         << "," << (causalFillFeedback ? 1 : 0)
         << "," << epoch_inst_fills << "," << epoch_data_fills
         << "," << (epoch_inst_fills + epoch_data_fills)
         << "," << epoch_data_fills_preserved_set
          << "," << epoch_admission_accepts << ","
          << epoch_admission_rejects << "," << admissionAcceptPct
          << "," << (actionEffective ? 1 : 0)
          << "," << activeActionRescueSamples
          << "," << activeActionRescueUseful
          << "," << activeActionRescueWasted
          << "," << activeActionRescueUsefulPct
          << "," << (activeActionRescueQualityBlocked ? 1 : 0)
          << "," << activeActionRescueQualityCooldown
          << "," << q_global_rescue_samples
          << "," << q_global_rescue_successes
          << "," << q_global_rescue_wastes
          << "," << globalRescueUsefulPct
          << "," << (globalRescueQualityBlocked ? 1 : 0)
          << "," << q_global_rescue_quality_cooldown
          << ","
          << (activeActionInRange ?
              q_action_warmup_attempts[activeAction] : 0)
          << "," << (noEffectEpoch ? 1 : 0)
          << "," << (qValueUpdated ? 1 : 0)
          << "," << epoch_set_data_pollution_marks
         << "," << epoch_set_data_pollution_rejects
         << "," << preserveReusePerAdmission << ","
         << epoch_preserve_clears << "," << epoch_quota_exceeded_sets
         << "," << reuseReward << ","
         << nonPreserveVictimReward << ","
         << instFillReductionReward << "," << instFillRegressionPenalty
         << "," << instFillOffBaseline << "," << instFillDelta << ","
         << totalFillReductionReward << "," << dataFillRegressionPenalty
         << "," << totalFillRegressionPenalty << ","
         << dataFillOffBaseline << "," << dataFillDelta << ","
         << totalFillOffBaseline << "," << totalFillDelta << ","
         << (totalFillRegressionGuarded ? 1 : 0) << ","
         << (dataFillRegressionGuarded ? 1 : 0) << ","
         << (fillRegressionGuarded ? 1 : 0) << ","
         << (dataPollutionGuarded ? 1 : 0) << ","
         << activeActionCooldown << ","
         << dataPollutionQualityPenalty << ","
         << actionQualitySample << "," << actionQualityBefore << ","
         << actionQualityAfter << ","
         << (actionQualityUpdated ? 1 : 0) << ","
         << (actionQualityBlocked ? 1 : 0) << ","
         << (actionQualityRecovered ? 1 : 0) << ","
         << instFillPenalty << ","
         << dataFillPenalty << "," << preserveVictimPenalty << ","
         << admittedPreservePenalty << "," << admissionPressurePenalty << ","
         << preserveClearPenalty << "," << preserveOccupancyPenalty << ","
         << quotaPenalty << "," << saturationPenalty << "\n";
}

void
LRUEmissary::resetEpochCounters()
{
    epoch_preserve_hits = 0;
    epoch_demand_preserve_hits = 0;
    epoch_useful_preserve_hits = 0;
    epoch_protection_events = 0;
    epoch_wasted_protections = 0;
    epoch_grace_retentions = 0;
    epoch_grace_expirations = 0;
    epoch_eligible_demand_hits = 0;
    epoch_rescue_capacity_rejects = 0;
    epoch_one_shot_evictions = 0;
    epoch_useful_credit_reward = 0.0;
    epoch_wasted_credit_penalty = 0.0;
    std::fill(
        epoch_protection_events_by_action.begin(),
        epoch_protection_events_by_action.end(), 0);
    std::fill(
        epoch_useful_hits_by_action.begin(),
        epoch_useful_hits_by_action.end(), 0);
    epoch_admission_accepts = 0;
    epoch_admission_rejects = 0;
    epoch_preserve_victims = 0;
    epoch_non_preserve_victims = 0;
    epoch_quota_exceeded_sets = 0;
    epoch_preserve_clears = 0;
    epoch_inst_fills = 0;
    epoch_data_fills = 0;
    epoch_data_fills_preserved_set = 0;
    epoch_set_data_pollution_marks = 0;
    epoch_set_data_pollution_rejects = 0;
}

LRUEmissary::LRUEmissaryStats::LRUEmissaryStats(statistics::Group* parent)
  : statistics::Group(parent),
    ADD_STAT(preserveVictims, statistics::units::Count::get(),
             "Number of victim selections from preserve lines"),
    ADD_STAT(nonPreserveVictims, statistics::units::Count::get(),
             "Number of victim selections from non-preserve lines"),
    ADD_STAT(quotaExceededSets, statistics::units::Count::get(),
             "Number of sets observed above the preserve-way quota"),
    ADD_STAT(preserveClears, statistics::units::Count::get(),
             "Number of preserve bits cleared by epoch flushing"),
    ADD_STAT(preserveFlushes, statistics::units::Count::get(),
             "Number of preserve epoch flushes"),
    ADD_STAT(adaptiveTightens, statistics::units::Count::get(),
             "Number of adaptive preserve quota decrements"),
    ADD_STAT(adaptiveRelaxes, statistics::units::Count::get(),
             "Number of adaptive preserve quota increments"),
    ADD_STAT(qLearningUpdates, statistics::units::Count::get(),
             "Number of Q-learning value updates"),
    ADD_STAT(qLearningExplores, statistics::units::Count::get(),
             "Number of Q-learning exploratory actions"),
    ADD_STAT(qLearningExploits, statistics::units::Count::get(),
             "Number of Q-learning greedy actions"),
    ADD_STAT(qLearningWarmupSelections, statistics::units::Count::get(),
             "Number of forced non-OFF action warm-up selections"),
    ADD_STAT(qLearningTieBreaks, statistics::units::Count::get(),
             "Number of greedy selections with tied Q-values"),
    ADD_STAT(qLearningRescueOutcomeSamples, statistics::units::Count::get(),
             "Number of completed rescue outcomes used as warm-up samples"),
    ADD_STAT(qRescueQualityBlocks, statistics::units::Count::get(),
             "Number of actions cooled down by poor rescue useful rate"),
    ADD_STAT(qRescueQualityForces, statistics::units::Count::get(),
             "Number of Q-learning actions forced to OFF by rescue quality"),
    ADD_STAT(qRescueQualitySkips, statistics::units::Count::get(),
             "Number of rescue-quality-blocked actions skipped during selection"),
    ADD_STAT(qGlobalRescueQualityBlocks, statistics::units::Count::get(),
             "Number of global rescue-quality cooldown activations"),
    ADD_STAT(qGlobalRescueQualityForces, statistics::units::Count::get(),
             "Number of epochs forced to OFF by global rescue quality"),
    ADD_STAT(qGlobalRescueQualitySkips, statistics::units::Count::get(),
             "Number of action selections skipped by global rescue quality"),
    ADD_STAT(qLearningActionSum, statistics::units::Count::get(),
             "Sum of Q-learning admission rates in milli-percent"),
    ADD_STAT(qLearningPreserveWaySum, statistics::units::Count::get(),
             "Sum of Q-learning selected effective preserve ways"),
    ADD_STAT(qAdmissionAccepts, statistics::units::Count::get(),
             "Number of Q-learning preserve admissions accepted"),
    ADD_STAT(qAdmissionRejects, statistics::units::Count::get(),
             "Number of Q-learning preserve admissions rejected"),
    ADD_STAT(qAdmissionGuardRejects, statistics::units::Count::get(),
             "Number of Q-learning preserve admissions rejected by set guard"),
    ADD_STAT(qInstFills, statistics::units::Count::get(),
             "Number of instruction-side L2 fills observed by Q-learning"),
    ADD_STAT(qDataFills, statistics::units::Count::get(),
             "Number of data-side L2 fills observed by Q-learning"),
    ADD_STAT(qDataFillsPreservedSet, statistics::units::Count::get(),
             "Number of data-side L2 fills in sets with preserved lines"),
    ADD_STAT(qInstFillBaselineUpdates, statistics::units::Count::get(),
             "Number of OFF-action epochs used to update I-fill baseline"),
    ADD_STAT(qFillRegressionGuardForces, statistics::units::Count::get(),
             "Number of Q-learning actions forced to OFF by fill regression guard"),
    ADD_STAT(qDataPollutionGuardForces, statistics::units::Count::get(),
             "Number of Q-learning actions forced to OFF by data pollution guard"),
    ADD_STAT(qSetDataPollutionMarks, statistics::units::Count::get(),
             "Number of sets placed on preserve-admission cooldown after data pollution"),
    ADD_STAT(qSetDataPollutionRejects, statistics::units::Count::get(),
             "Number of preserve admissions rejected by set data-pollution filter"),
    ADD_STAT(qBadActionCooldowns, statistics::units::Count::get(),
             "Number of Q-learning actions placed on regression cooldown"),
    ADD_STAT(qCooldownActionSkips, statistics::units::Count::get(),
             "Number of cooled-down Q-learning actions skipped during selection"),
    ADD_STAT(qActionQualityUpdates, statistics::units::Count::get(),
             "Number of Q-learning action quality updates"),
    ADD_STAT(qActionQualityRecoveries, statistics::units::Count::get(),
             "Number of Q-learning action quality recovery steps"),
    ADD_STAT(qQualityActionBlocks, statistics::units::Count::get(),
             "Number of actions newly blocked by the quality gate"),
    ADD_STAT(qQualityActionForces, statistics::units::Count::get(),
             "Number of Q-learning actions forced to OFF by the quality gate"),
    ADD_STAT(qQualityActionSkips, statistics::units::Count::get(),
             "Number of quality-blocked Q-learning actions skipped during selection"),
    ADD_STAT(qNoEffectEpochs, statistics::units::Count::get(),
             "Number of non-OFF epochs with no observable preserve effect"),
    ADD_STAT(qNoEffectActionForces, statistics::units::Count::get(),
             "Number of no-effect epochs that forced the next action to OFF"),
    ADD_STAT(preserveHits, statistics::units::Count::get(),
             "Number of cache hits on preserved lines"),
    ADD_STAT(qDemandPreserveHits, statistics::units::Count::get(),
             "Number of demand instruction hits on preserved lines"),
    ADD_STAT(qProtectionEvents, statistics::units::Count::get(),
             "Number of preserved lines retained instead of the ordinary LRU victim"),
    ADD_STAT(qUsefulPreserveHits, statistics::units::Count::get(),
             "Number of demand hits after preserve avoided an LRU eviction"),
    ADD_STAT(qWastedProtections, statistics::units::Count::get(),
             "Number of protected lines cleared or invalidated before demand reuse"),
    ADD_STAT(qGraceRetentions, statistics::units::Count::get(),
             "Number of flushes that retained an unused line during admission grace"),
    ADD_STAT(qGraceExpirations, statistics::units::Count::get(),
             "Number of admitted lines cleared after admission grace expired"),
    ADD_STAT(qEligibleDemandHits, statistics::units::Count::get(),
             "Number of demand hits on eligible lines before victim-time rescue"),
    ADD_STAT(qRescueCapacityRejects, statistics::units::Count::get(),
             "Number of eligible LRU victims not rescued because the set rescue quota was full"),
    ADD_STAT(qOneShotEvictions, statistics::units::Count::get(),
             "Number of rescued lines evicted on their next LRU victim attempt"),
    ADD_STAT(qUsefulCreditUpdates, statistics::units::Count::get(),
             "Number of delayed useful-hit state-action credit updates"),
    ADD_STAT(qUsefulCreditReward, statistics::units::Count::get(),
             "Total delayed reward attributed to useful preserve hits"),
    ADD_STAT(qWastedCreditUpdates, statistics::units::Count::get(),
             "Number of original state-action pairs charged for wasted rescues"),
    ADD_STAT(qWastedCreditPenalty, statistics::units::Count::get(),
             "Total delayed penalty attributed to wasted rescues"),
    ADD_STAT(qAuxiliaryTouchSuppressions, statistics::units::Count::get(),
             "Number of auxiliary EMISSARY hits prevented from refreshing LRU recency")
{
}

} // namespace replacement_policy
} // namespace gem5
