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
#include <fstream>
#include <limits>
#include <map>

#include "base/output.hh"
#include "base/trace.hh"
#include "debug/EMISSARY.hh"
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
      q_learning_min_preserve_ways(p.q_learning_min_preserve_ways),
      q_reward_non_preserve_victim(p.q_reward_non_preserve_victim),
      q_penalty_preserve_victim(p.q_penalty_preserve_victim),
      q_penalty_quota_exceeded(p.q_penalty_quota_exceeded),
      q_penalty_saturation(p.q_penalty_saturation),
      q_reward_preserve_hit(p.q_reward_preserve_hit),
      q_penalty_admitted_preserve(p.q_penalty_admitted_preserve),
      q_learning_set_guard(p.q_learning_set_guard),
      q_learning_seed(p.q_learning_seed),
      q_num_actions(0),
      q_num_states(18),
      q_last_state(0),
      q_last_action(0),
      q_has_last(false),
      q_log_header_written(false),
      q_rng(q_learning_seed ?
            Random::genRandom(static_cast<uint32_t>(q_learning_seed)) :
            Random::genRandom()),
      epoch_preserve_hits(0),
      epoch_admission_accepts(0),
      epoch_admission_rejects(0),
      epoch_preserve_victims(0),
      epoch_non_preserve_victims(0),
      epoch_quota_exceeded_sets(0),
      indexingPolicy(nullptr),
      stats(this)
{
    if (adaptive_min_preserve_ways > preserve_ways) {
        adaptive_min_preserve_ways = preserve_ways;
    }
    if (q_learning_min_preserve_ways > preserve_ways) {
        q_learning_min_preserve_ways = preserve_ways;
    }
    if (q_learning_min_preserve_ways < 1) {
        q_learning_min_preserve_ways = 1;
    }
    q_admission_rates = {1.5625, 3.125, 6.25, 12.5};
    q_num_actions = static_cast<int>(q_admission_rates.size());
    q_values.assign(q_num_states * q_num_actions, 0.0);
    if (q_learning_preserve) {
        q_last_state = qState(0.0, 0.0, 0.0);
        q_last_action = 1;
        q_has_last = true;
        effective_preserve_ways = preserve_ways;
    }
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
    qApplyAdmission(replacement_data, pkt);
    touch(replacement_data);
}

void
LRUEmissary::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    const_cast<LRUEmissary*>(this)->checkToFlushPreserveBits();
    std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data)->lastTouchTick = max_age;
}

void
LRUEmissary::reset(
    const std::shared_ptr<ReplacementData>& replacement_data,
    const PacketPtr pkt)
{
    qApplyAdmission(replacement_data, pkt);
    reset(replacement_data);
}

void
LRUEmissary::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto *non_const_this = const_cast<LRUEmissary*>(this);
    non_const_this->checkToFlushPreserveBits();
    checkLRU(replacement_data);
    std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data)->lastTouchTick = max_age;
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
    int fullSaturatedSets = 0;
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
                blk->clearPreserve();
            }
            blk->clearUsed();
        }

        if (numPreserved >= effective_preserve_ways) {
            saturatedSets++;
        }
        if (numPreserved > effective_preserve_ways) {
            stats.quotaExceededSets++;
            epoch_quota_exceeded_sets++;
        }
        if (numPreserved >= preserve_ways) {
            fullSaturatedSets++;
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

    const double fullSaturatedPct = 100.0 * fullSaturatedSets / numSets;
    const uint64_t victimTotal =
        epoch_preserve_victims + epoch_non_preserve_victims;
    const double preserveVictimPct = victimTotal ?
        100.0 * epoch_preserve_victims / victimTotal : 0.0;
    const uint64_t admissionTotal =
        epoch_admission_accepts + epoch_admission_rejects;
    const double admissionAcceptPct = admissionTotal ?
        100.0 * epoch_admission_accepts / admissionTotal : 0.0;
    const double preserveReusePerAdmission = epoch_admission_accepts ?
        static_cast<double>(epoch_preserve_hits) /
            static_cast<double>(epoch_admission_accepts) : 0.0;
    const double cappedPreserveReuse =
        std::min(preserveReusePerAdmission, 8.0);
    const double admittedPreserveK =
        static_cast<double>(epoch_admission_accepts) / 1000.0;

    if (q_learning_preserve && numSets > 0) {
        const double reward =
            q_reward_preserve_hit *
                cappedPreserveReuse +
            q_reward_non_preserve_victim *
                (static_cast<double>(epoch_non_preserve_victims) / 10000.0) -
            q_penalty_preserve_victim *
                (static_cast<double>(epoch_preserve_victims) / 1000.0) -
            q_penalty_admitted_preserve *
                admittedPreserveK -
            q_penalty_quota_exceeded *
                static_cast<double>(epoch_quota_exceeded_sets) -
            q_penalty_saturation *
                std::max(0.0, fullSaturatedPct - q_learning_target_saturation);
        const int nextState =
            qState(fullSaturatedPct, preserveVictimPct,
                preserveReusePerAdmission);
        qUpdate(
            nextState, reward, fullSaturatedPct, preserveVictimPct,
            preserveReusePerAdmission, admissionAcceptPct);
        resetEpochCounters();
    }

    if (adaptive_preserve && !q_learning_preserve && numSets > 0) {
        const double saturatedPct = 100.0 * saturatedSets / numSets;
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
    if (q_admission_rates.empty()) {
        return 100.0;
    }
    action = std::max(0, std::min(action,
        static_cast<int>(q_admission_rates.size()) - 1));
    return q_admission_rates[action];
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
LRUEmissary::qApplyAdmission(
    const std::shared_ptr<ReplacementData>& replacement_data,
    const PacketPtr pkt) const
{
    if (!q_learning_preserve || !pkt || !pkt->isPreserve()) {
        return;
    }

    auto repl_data = std::static_pointer_cast<LRUEmissaryReplData>(
        replacement_data);
    if (repl_data->blk && repl_data->blk->isPreserve()) {
        return;
    }

    if (q_learning_set_guard && repl_data->blk &&
        countSetPreserves(repl_data->blk) >= preserve_ways) {
        pkt->setPreserve(false);
        stats.qAdmissionRejects++;
        stats.qAdmissionGuardRejects++;
        epoch_admission_rejects++;
        return;
    }

    const double rate =
        qActionToAdmissionRate(q_has_last ? q_last_action : 0);
    const double sample =
        static_cast<double>(q_rng->random<uint32_t>(0, 9999)) / 100.0;
    if (sample < rate) {
        stats.qAdmissionAccepts++;
        epoch_admission_accepts++;
    } else {
        pkt->setPreserve(false);
        stats.qAdmissionRejects++;
        epoch_admission_rejects++;
    }
}

int
LRUEmissary::qState(
    double saturatedPct, double preserveVictimPct,
    double preserveReusePerAdmission) const
{
    const int saturationBin =
        saturatedPct < 10.0 ? 0 : (saturatedPct < 50.0 ? 1 : 2);
    const int victimBin = preserveVictimPct < 5.0 ? 0 : 1;
    const int reuseBin =
        preserveReusePerAdmission < 0.5 ? 0 :
            (preserveReusePerAdmission < 2.0 ? 1 : 2);
    return (saturationBin * 2 + victimBin) * 3 + reuseBin;
}

int
LRUEmissary::qChooseAction(int state)
{
    const double explore =
        static_cast<double>(q_rng->random<uint32_t>(0, 9999)) / 10000.0;
    if (explore < q_learning_epsilon) {
        stats.qLearningExplores++;
        return q_rng->random<int>(0, q_num_actions - 1);
    }

    stats.qLearningExploits++;
    int bestAction = std::min(1, q_num_actions - 1);
    double bestValue = q_values[state * q_num_actions + bestAction];
    for (int action = 0; action < q_num_actions; action++) {
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
    double preserveVictimPct, double preserveReusePerAdmission,
    double admissionAcceptPct)
{
    if (q_has_last) {
        double nextBest = -std::numeric_limits<double>::infinity();
        for (int action = 0; action < q_num_actions; action++) {
            nextBest = std::max(
                nextBest, q_values[nextState * q_num_actions + action]);
        }

        double& oldValue = q_values[q_last_state * q_num_actions + q_last_action];
        oldValue += q_learning_alpha *
            (reward + q_learning_gamma * nextBest - oldValue);
        stats.qLearningUpdates++;
    }

    const int nextAction = qChooseAction(nextState);
    qLogEpoch(
        nextState, nextAction, reward, saturatedPct, preserveVictimPct,
        preserveReusePerAdmission, admissionAcceptPct);
    q_last_state = nextState;
    q_last_action = nextAction;
    q_has_last = true;
    effective_preserve_ways = preserve_ways;
    stats.qLearningActionSum +=
        static_cast<uint64_t>(qActionToAdmissionRate(nextAction) * 1000.0);
}

void
LRUEmissary::qLogEpoch(
    int state, int action, double reward, double saturatedPct,
    double preserveVictimPct, double preserveReusePerAdmission,
    double admissionAcceptPct)
{
    std::ofstream qOut;
    qOut.open(simout.directory() + "/q_learning.csv", std::fstream::app);
    if (!q_log_header_written) {
        qOut << "tick,state,action,admission_rate,reward,"
             << "saturated_pct,preserve_victim_pct,preserve_victims,"
             << "non_preserve_victims,preserve_hits,admission_accepts,"
             << "admission_rejects,admission_accept_pct,"
             << "preserve_reuse_per_admission,quota_exceeded_sets\n";
        q_log_header_written = true;
    }
    qOut << curTick() << "," << state << "," << action << ","
         << qActionToAdmissionRate(action) << "," << reward << ","
         << saturatedPct << "," << preserveVictimPct << ","
         << epoch_preserve_victims << "," << epoch_non_preserve_victims
         << "," << epoch_preserve_hits << "," << epoch_admission_accepts
         << "," << epoch_admission_rejects << "," << admissionAcceptPct
         << "," << preserveReusePerAdmission << ","
         << epoch_quota_exceeded_sets << "\n";
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
    ADD_STAT(qAdmissionAccepts, statistics::units::Count::get(),
             "Number of Q-learning preserve admissions accepted"),
    ADD_STAT(qAdmissionRejects, statistics::units::Count::get(),
             "Number of Q-learning preserve admissions rejected"),
    ADD_STAT(qAdmissionGuardRejects, statistics::units::Count::get(),
             "Number of Q-learning preserve admissions rejected by set guard"),
    ADD_STAT(preserveHits, statistics::units::Count::get(),
             "Number of cache hits on preserved lines")
{
}

} // namespace replacement_policy
} // namespace gem5
