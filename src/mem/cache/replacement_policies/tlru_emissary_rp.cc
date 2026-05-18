/**
 * @file
 * Implementation of the paper TLRU EMISSARY replacement policy.
 */

#include "mem/cache/replacement_policies/tlru_emissary_rp.hh"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>

#include "base/output.hh"
#include "params/TLRUEmissaryRP.hh"
#include "sim/core.hh"
#include "sim/cur_tick.hh"

namespace gem5
{

namespace replacement_policy
{

TLRUEmissary::TLRUEmissary(const Params &p)
    : Base(p),
      lru_ways(p.lru_ways),
      preserve_ways(p.preserve_ways),
      last_tick(0),
      numSets(0),
      numWays(0),
      flush_freq_in_cycles(p.flush_freq_in_cycles),
      indexingPolicy(nullptr)
{
    registerExitCallback([this]() { dumpPreserveHist(); });
}

void
TLRUEmissary::invalidate(
    const std::shared_ptr<ReplacementData>& replacement_data)
{
    std::static_pointer_cast<TLRUEmissaryReplData>(
        replacement_data)->lastTouchTick = Tick(0);
}

void
TLRUEmissary::touch(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    const_cast<TLRUEmissary*>(this)->checkToFlushPreserveBits();
    std::static_pointer_cast<TLRUEmissaryReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
TLRUEmissary::reset(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    const_cast<TLRUEmissary*>(this)->checkToFlushPreserveBits();
    std::static_pointer_cast<TLRUEmissaryReplData>(
        replacement_data)->lastTouchTick = curTick();
}

ReplaceableEntry*
TLRUEmissary::getVictim(const ReplacementCandidates& candidates) const
{
    assert(!candidates.empty());
    const_cast<TLRUEmissary*>(this)->checkToFlushPreserveBits();

    ReplaceableEntry *lru_entry = candidates[0];
    ReplaceableEntry *victim_not_preserved = nullptr;
    ReplaceableEntry *preserved_entry = nullptr;
    int num_preserved = 0;

    for (const auto& candidate : candidates) {
        auto candidate_repl_data =
            std::static_pointer_cast<TLRUEmissaryReplData>(
                candidate->replacementData);
        auto lru_repl_data =
            std::static_pointer_cast<TLRUEmissaryReplData>(
                lru_entry->replacementData);
        if (candidate_repl_data->lastTouchTick < lru_repl_data->lastTouchTick) {
            lru_entry = candidate;
        }

        auto *blk = static_cast<CacheBlk*>(candidate);
        if (blk->isPreserve()) {
            num_preserved++;
            if (!preserved_entry) {
                preserved_entry = candidate;
                continue;
            }

            auto preserved_repl_data =
                std::static_pointer_cast<TLRUEmissaryReplData>(
                    preserved_entry->replacementData);
            const uint64_t candidate_cost = candidate_repl_data->lastTouchTick;
            const uint64_t preserved_cost = preserved_repl_data->lastTouchTick;
            auto *preserved_blk = static_cast<CacheBlk*>(preserved_entry);

            if ((candidate_cost < preserved_cost &&
                 curTick() - candidate_cost > 500 * 500) ||
                blk->getRefCount() < preserved_blk->getRefCount()) {
                preserved_entry = candidate;
            }
        } else if (!victim_not_preserved) {
            victim_not_preserved = candidate;
        } else {
            auto victim_repl_data =
                std::static_pointer_cast<TLRUEmissaryReplData>(
                    victim_not_preserved->replacementData);
            if (candidate_repl_data->lastTouchTick <
                victim_repl_data->lastTouchTick) {
                victim_not_preserved = candidate;
            }
        }
    }

    if (num_preserved > preserve_ways) {
        return preserved_entry ? preserved_entry : lru_entry;
    }

    return victim_not_preserved ? victim_not_preserved : lru_entry;
}

std::shared_ptr<ReplacementData>
TLRUEmissary::instantiateEntry()
{
    return std::make_shared<TLRUEmissaryReplData>();
}

void
TLRUEmissary::checkToFlushPreserveBits()
{
    if (!flush_freq_in_cycles || !indexingPolicy) {
        return;
    }

    const uint64_t cur_tick = curTick();
    if (((cur_tick - last_tick) / 500) >= flush_freq_in_cycles) {
        dumpPreserveHist();
        last_tick = cur_tick;
    }
}

void
TLRUEmissary::dumpPreserveHist()
{
    if (!indexingPolicy || numSets <= 0 || numWays <= 0) {
        return;
    }

    std::ofstream hist_out;
    hist_out.open(simout.directory() + "/set_hist.csv", std::fstream::app);
    hist_out << curTick() / 500 << ",";

    std::map<int, int> preserve_count_hist;
    for (int i = 0; i < numWays; i++) {
        preserve_count_hist[i] = 0;
    }

    for (int set = 0; set < numSets; set++) {
        int num_preserved = 0;
        for (int way = 0; way < numWays; way++) {
            auto *entry = indexingPolicy->getEntry(set, way);
            auto *blk = static_cast<CacheBlk*>(entry);
            if (blk->isPreserve()) {
                num_preserved++;
            }
            if (!blk->isUsed()) {
                blk->clearPreserve();
            }
            blk->clearUsed();
        }

        preserve_count_hist[
            num_preserved >= preserve_ways ? preserve_ways : num_preserved]++;
    }

    for (int i = 0; i < numWays; i++) {
        hist_out << preserve_count_hist[i] << ",";
    }
    hist_out << "\n";
}

} // namespace replacement_policy
} // namespace gem5
