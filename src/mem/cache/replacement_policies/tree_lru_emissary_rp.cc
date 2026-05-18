/**
 * @file
 * Implementation of the paper two-tree PLRU EMISSARY replacement policy.
 */

#include "mem/cache/replacement_policies/tree_lru_emissary_rp.hh"

#include <cassert>
#include <cstddef>
#include <fstream>
#include <map>

#include "base/logging.hh"
#include "base/output.hh"
#include "debug/EMISSARY.hh"
#include "params/TreeLRUEmissaryRP.hh"
#include "sim/core.hh"
#include "sim/cur_tick.hh"

namespace gem5
{

namespace replacement_policy
{

static uint64_t
parentIndex(const uint64_t index)
{
    return (index - 1) / 2;
}

static uint64_t
leftSubtreeIndex(const uint64_t index)
{
    return 2 * index + 1;
}

static uint64_t
rightSubtreeIndex(const uint64_t index)
{
    return 2 * index + 2;
}

static bool
isRightSubtree(const uint64_t index)
{
    return index % 2 == 0;
}

static void
setTreeDirection(TreeLRUEmissary::TreeLRUEmissaryReplData *repl_data,
    TreeLRUEmissary::PLRUTree &tree, bool point_to_entry)
{
    uint64_t tree_index = repl_data->index;
    do {
        const bool right = isRightSubtree(tree_index);
        tree_index = parentIndex(tree_index);
        tree.at(tree_index) = point_to_entry ? right : !right;
    } while (tree_index != 0);
}

TreeLRUEmissary::TreeLRUEmissary(const Params &p)
    : Base(p),
      numLeaves(p.num_leaves),
      count(0),
      treeInstance(nullptr),
      preserveTreeInstance(nullptr),
      lru_ways(p.lru_ways),
      preserve_ways(p.preserve_ways),
      last_tick(0),
      numSets(0),
      numWays(0),
      flush_freq_in_cycles(p.flush_freq_in_cycles),
      indexingPolicy(nullptr)
{
    fatal_if(numLeaves < 1, "numLeaves should never be 0");
    registerExitCallback([this]() { dumpPreserveHist(); });
}

void
TreeLRUEmissary::invalidate(
    const std::shared_ptr<ReplacementData>& replacement_data)
{
    auto repl_data =
        std::static_pointer_cast<TreeLRUEmissaryReplData>(replacement_data);
    setTreeDirection(repl_data.get(), *repl_data->tree, true);
    setTreeDirection(repl_data.get(), *repl_data->preserveTree, true);
}

void
TreeLRUEmissary::promote(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto repl_data =
        std::static_pointer_cast<TreeLRUEmissaryReplData>(replacement_data);
    setTreeDirection(repl_data.get(), *repl_data->preserveTree, false);
}

void
TreeLRUEmissary::updateTree(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    auto repl_data =
        std::static_pointer_cast<TreeLRUEmissaryReplData>(replacement_data);
    setTreeDirection(repl_data.get(), *repl_data->tree, false);
}

void
TreeLRUEmissary::touch(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    const_cast<TreeLRUEmissary*>(this)->checkToFlushPreserveBits();
    updateTree(replacement_data);
}

void
TreeLRUEmissary::reset(
    const std::shared_ptr<ReplacementData>& replacement_data) const
{
    touch(replacement_data);
}

ReplaceableEntry*
TreeLRUEmissary::getVictim(const ReplacementCandidates& candidates) const
{
    assert(!candidates.empty());
    const_cast<TreeLRUEmissary*>(this)->checkToFlushPreserveBits();

    const auto *repl_data =
        std::static_pointer_cast<TreeLRUEmissaryReplData>(
            candidates[0]->replacementData).get();
    const PLRUTree *tree = repl_data->tree.get();
    const PLRUTree *preserve_tree = repl_data->preserveTree.get();

    size_t num_preserved = 0;
    for (const auto& candidate : candidates) {
        auto *blk = static_cast<CacheBlk*>(candidate);
        if (blk->isPreserve()) {
            num_preserved++;
        }
    }

    if (num_preserved == 0 || num_preserved == candidates.size()) {
        uint64_t tree_index = 0;
        while (tree_index < tree->size()) {
            tree_index = tree->at(tree_index) ?
                rightSubtreeIndex(tree_index) :
                leftSubtreeIndex(tree_index);
        }
        return candidates.at(tree_index - (numLeaves - 1));
    }

    const bool need_preserve_victim =
        num_preserved > static_cast<size_t>(preserve_ways);
    for (size_t attempt = 0; attempt < candidates.size(); attempt++) {
        uint64_t tree_index = 0;
        const PLRUTree *chosen_tree =
            need_preserve_victim ? preserve_tree : tree;
        while (tree_index < chosen_tree->size()) {
            tree_index = chosen_tree->at(tree_index) ?
                rightSubtreeIndex(tree_index) :
                leftSubtreeIndex(tree_index);
        }

        ReplaceableEntry *victim = candidates.at(tree_index - (numLeaves - 1));
        auto *victim_blk = static_cast<CacheBlk*>(victim);
        if (victim_blk->isPreserve() == need_preserve_victim) {
            const int way = static_cast<int>(tree_index - (numLeaves - 1));
            DPRINTF(EMISSARY, "TreeLRUEmissary victim set %d way %d P:%d\n",
                    victim_blk->getSet(), way, victim_blk->isPreserve());
            return victim;
        }

        if (need_preserve_victim) {
            promote(victim->replacementData);
        } else {
            updateTree(victim->replacementData);
        }
    }

    for (const auto& candidate : candidates) {
        auto *blk = static_cast<CacheBlk*>(candidate);
        if (blk->isPreserve() == need_preserve_victim) {
            return candidate;
        }
    }

    return candidates[0];
}

std::shared_ptr<ReplacementData>
TreeLRUEmissary::instantiateEntry()
{
    return instantiateEntry(nullptr);
}

std::shared_ptr<ReplacementData>
TreeLRUEmissary::instantiateEntry(CacheBlk *blk)
{
    if (count % numLeaves == 0) {
        treeInstance = std::make_shared<PLRUTree>(numLeaves - 1, false);
        preserveTreeInstance =
            std::make_shared<PLRUTree>(numLeaves - 1, false);
    }

    auto repl_data = std::make_shared<TreeLRUEmissaryReplData>(
        blk, (count % numLeaves) + numLeaves - 1, treeInstance,
        preserveTreeInstance);
    count++;
    return repl_data;
}

void
TreeLRUEmissary::checkToFlushPreserveBits()
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
TreeLRUEmissary::dumpPreserveHist()
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
