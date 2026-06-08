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
      q_learning_target_saturation(p.q_learning_target_saturation),
      q_learning_target_occupancy(p.q_learning_target_occupancy),
      q_learning_min_preserve_ways(p.q_learning_min_preserve_ways),
      q_learning_default_action(p.q_learning_default_action),
      q_learning_reuse_cap(p.q_learning_reuse_cap),
      q_learning_inst_baseline_alpha(p.q_learning_inst_baseline_alpha),
      q_reward_non_preserve_victim(p.q_reward_non_preserve_victim),
      q_penalty_preserve_victim(p.q_penalty_preserve_victim),
      q_penalty_quota_exceeded(p.q_penalty_quota_exceeded),
      q_penalty_saturation(p.q_penalty_saturation),
      q_reward_preserve_hit(p.q_reward_preserve_hit),
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
      q_rng(q_learning_seed ?
            Random::genRandom(static_cast<uint32_t>(q_learning_seed)) :
            Random::genRandom()),
      epoch_preserve_hits(0),
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
            {0.390625, 1},
            {0.78125, 1},
            {1.5625, 1},
            {3.125, 2},
        };
    }
    q_num_actions = static_cast<int>(q_actions.size());
    q_learning_default_action = std::max(
        0, std::min(q_learning_default_action, q_num_actions - 1));
    q_values.assign(q_num_states * q_num_actions, 0.0);
    q_action_cooldowns.assign(q_num_actions, 0);
    q_action_quality.assign(q_num_actions, 0.0);
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
    std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data)->lastTouchTick = Tick(0);
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
    if (pkt && pkt->isRead() && repl_data->blk &&
        repl_data->blk->isPreserve()) {
        stats.preserveHits++;
        epoch_preserve_hits++;
    }
    if (!qApplyAdmission(replacement_data, pkt)) {
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
    if (q_learning_preserve && pkt && pkt->isRead() && pkt->req) {
        auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
            replacement_data);
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
    if (!qApplyAdmission(replacement_data, pkt)) {
        return;
    }
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

    ReplaceableEntry *victim_not_preserved = candidates[0];
    ReplaceableEntry *preserved_victim = candidates[0];
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

    if (num_preserved > effective_preserve_ways) {
        stats.preserveVictims++;
        epoch_preserve_victims++;
        return preserved_victim;
    }
    stats.nonPreserveVictims++;
    epoch_non_preserve_victims++;
    return victim_not_preserved;
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
            if (blk->isPreserve()) {
                numPreserved++;
            }
            if (blk->isPreserve() && !blk->isUsed()) {
                stats.preserveClears++;
                epoch_preserve_clears++;
                blk->clearPreserve();
            }
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
        static_cast<double>(epoch_preserve_hits) /
            static_cast<double>(epoch_admission_accepts) : 0.0;
    const double cappedPreserveReuse =
        std::min(preserveReusePerAdmission, q_learning_reuse_cap);
    const double admittedPreserveK =
        static_cast<double>(epoch_admission_accepts) / 1000.0;

    if (q_learning_preserve && numSets > 0) {
        const int activeAction = epochAction;
        const bool activeOff =
            qActionToAdmissionRate(activeAction) <= 0.0 ||
            qActionToPreserveWays(activeAction) <= 0;
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

        if (!activeOff && q_has_inst_fill_off_baseline) {
            instFillDelta = q_inst_fill_off_baseline -
                epochInstFills;
            instFillReductionReward =
                q_reward_inst_fill_reduction *
                (std::max(0.0, instFillDelta) / 1000.0);
            instFillRegressionPenalty =
                q_penalty_inst_fill_regression *
                (std::max(0.0, -instFillDelta) / 1000.0);
        }

        if (!activeOff && q_has_fill_off_baseline) {
            dataFillDelta = q_data_fill_off_baseline - epochDataFills;
            totalFillDelta = q_total_fill_off_baseline - epochTotalFills;
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

        const double reuseReward =
            q_reward_preserve_hit * cappedPreserveReuse;
        const double nonPreserveVictimReward =
            q_reward_non_preserve_victim *
                (static_cast<double>(epoch_non_preserve_victims) / 10000.0);
        const double instFillPenalty =
            q_penalty_inst_fill *
                (static_cast<double>(epoch_inst_fills) / 1000.0);
        const double dataFillPenalty =
            q_penalty_data_fill *
                (static_cast<double>(epoch_data_fills_preserved_set) /
                    1000.0);
        const double preserveVictimPenalty =
            q_penalty_preserve_victim *
                (static_cast<double>(epoch_preserve_victims) / 1000.0);
        const double admittedPreservePenalty =
            q_penalty_admitted_preserve * admittedPreserveK;
        const double admissionPressurePenalty =
            q_penalty_admission_pressure * admissionAcceptPct;
        const double preserveClearPenalty =
            q_penalty_preserve_clear *
                (static_cast<double>(epoch_preserve_clears) / 1000.0);
        const double preserveOccupancyPenalty =
            q_penalty_preserve_occupancy *
                std::max(0.0,
                    preserveOccupancyPct - q_learning_target_occupancy);
        const double quotaPenalty =
            q_penalty_quota_exceeded *
                static_cast<double>(epoch_quota_exceeded_sets);
        const double saturationPenalty =
            q_penalty_saturation *
                std::max(0.0, saturatedPct - q_learning_target_saturation);
        const double reward =
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
    if (repl_data->blk && repl_data->blk->isPreserve()) {
        return true;
    }

    if (effective_preserve_ways <= 0) {
        pkt->setPreserve(false);
        stats.qAdmissionRejects++;
        epoch_admission_rejects++;
        return false;
    }

    if (q_learning_set_guard && repl_data->blk &&
        countSetPreserves(repl_data->blk) >= effective_preserve_ways) {
        pkt->setPreserve(false);
        stats.qAdmissionRejects++;
        stats.qAdmissionGuardRejects++;
        epoch_admission_rejects++;
        return false;
    }

    if (qSetDataPollutionBlocked(repl_data->blk)) {
        pkt->setPreserve(false);
        stats.qAdmissionRejects++;
        stats.qSetDataPollutionRejects++;
        epoch_admission_rejects++;
        epoch_set_data_pollution_rejects++;
        return false;
    }

    if (pkt->isEmissaryPreAdmitted()) {
        stats.qAdmissionAccepts++;
        epoch_admission_accepts++;
        return true;
    }

    const double rate =
        qActionToAdmissionRate(q_has_last ? q_last_action : 0);
    const double sample =
        static_cast<double>(q_rng->random<uint32_t>(0, 9999)) / 100.0;
    if (sample < rate) {
        stats.qAdmissionAccepts++;
        epoch_admission_accepts++;
        return true;
    }

    pkt->setPreserve(false);
    stats.qAdmissionRejects++;
    epoch_admission_rejects++;
    return false;
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

void
LRUEmissary::qTickActionCooldowns()
{
    for (std::size_t action = 1; action < q_action_cooldowns.size();
         action++) {
        if (q_action_cooldowns[action] > 0) {
            q_action_cooldowns[action]--;
        }
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
        candidates.push_back(action);
    }
    if (candidates.empty()) {
        candidates.push_back(0);
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
    int bestAction = candidates.front();
    for (const int action : candidates) {
        if (action == q_learning_default_action) {
            bestAction = action;
            break;
        }
    }
    double bestValue = q_values[state * q_num_actions + bestAction];
    for (const int action : candidates) {
        const double value = q_values[state * q_num_actions + action];
        if (value > bestValue) {
            bestValue = value;
            bestAction = action;
        }
    }
    return bestAction;
}

void
LRUEmissary::qUpdate(
    int nextState, double reward, double saturatedPct,
    double preserveOccupancyPct, double preserveVictimPct,
    double preserveReusePerAdmission, double admissionAcceptPct,
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
    const bool totalFillRegressionGuarded =
        q_learning_fill_regression_guard && q_has_fill_off_baseline &&
        !activeOff && totalFillDelta < 0.0;
    const bool dataFillRegressionGuarded =
        q_learning_fill_regression_guard &&
        q_learning_data_regression_guard && q_has_fill_off_baseline &&
        !activeOff && dataFillDelta < 0.0;
    const bool fillRegressionGuarded =
        totalFillRegressionGuarded || dataFillRegressionGuarded;
    const bool dataPollutionGuarded =
        q_learning_data_pollution_guard && !activeOff &&
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
    } else if (q_learning_action_quality_gate &&
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

    if (q_has_last) {
        double nextBest = -std::numeric_limits<double>::infinity();
        for (int action = 0; action < q_num_actions; action++) {
            if (qActionCoolingDown(action)) {
                continue;
            }
            if (qActionQualityBlocked(action)) {
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
    }

    int nextAction = 0;
    if (fillRegressionGuarded) {
        stats.qFillRegressionGuardForces++;
    }
    if (dataPollutionGuarded) {
        stats.qDataPollutionGuardForces++;
    }
    if (actionQualityBlocked) {
        stats.qQualityActionForces++;
    }
    if (guardForced || actionQualityBlocked) {
        nextAction = 0;
    } else {
        nextAction = qChooseAction(nextState);
    }
    const int nextPreserveWays = qActionToPreserveWays(nextAction);
    qLogEpoch(
        nextState, activeAction, nextAction, reward, saturatedPct,
        preserveOccupancyPct,
        preserveVictimPct, preserveReusePerAdmission, admissionAcceptPct,
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
    if (!q_log_header_written) {
        qOut << "tick,state,action,admission_rate,effective_preserve_ways,"
             << "next_action,next_admission_rate,next_effective_preserve_ways,"
             << "reward,"
             << "saturated_pct,preserve_occupancy_pct,preserve_victim_pct,"
             << "preserve_victims,non_preserve_victims,preserve_hits,"
             << "inst_fills,data_fills,total_fills,data_fills_preserved_set,"
             << "admission_accepts,admission_rejects,admission_accept_pct,"
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
         << "," << epoch_inst_fills << "," << epoch_data_fills
         << "," << (epoch_inst_fills + epoch_data_fills)
         << "," << epoch_data_fills_preserved_set
         << "," << epoch_admission_accepts << ","
         << epoch_admission_rejects << "," << admissionAcceptPct
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
    ADD_STAT(preserveHits, statistics::units::Count::get(),
             "Number of cache hits on preserved lines")
{
}

} // namespace replacement_policy
} // namespace gem5
