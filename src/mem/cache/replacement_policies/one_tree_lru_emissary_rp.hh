/**
 * @file
 * Declaration of the paper one-tree PLRU EMISSARY replacement policy.
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_ONE_TREE_LRU_EMISSARY_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_ONE_TREE_LRU_EMISSARY_RP_HH__

#include <cstdint>
#include <memory>
#include <vector>

#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"

namespace gem5
{

struct OneTreeLRUEmissaryRPParams;

namespace replacement_policy
{

class OneTreeLRUEmissary : public Base
{
  public:
    using PLRUTree = std::vector<bool>;

    struct OneTreeLRUEmissaryReplData : ReplacementData
    {
        CacheBlk *blk;
        const uint64_t index;
        std::shared_ptr<PLRUTree> tree;

        OneTreeLRUEmissaryReplData(CacheBlk *blk, const uint64_t index,
            std::shared_ptr<PLRUTree> tree)
          : blk(blk), index(index), tree(tree)
        {}
    };

  private:
    const uint64_t numLeaves;
    uint64_t count;
    std::shared_ptr<PLRUTree> treeInstance;

  public:
    using Params = OneTreeLRUEmissaryRPParams;

    int lru_ways;
    int preserve_ways;
    uint64_t last_tick;
    int numSets;
    int numWays;
    uint64_t flush_freq_in_cycles;
    TaggedIndexingPolicy *indexingPolicy;

    explicit OneTreeLRUEmissary(const Params &p);
    ~OneTreeLRUEmissary() = default;

    void invalidate(
        const std::shared_ptr<ReplacementData>& replacement_data) override;
    void touch(
        const std::shared_ptr<ReplacementData>& replacement_data) const
        override;
    void reset(
        const std::shared_ptr<ReplacementData>& replacement_data) const
        override;
    ReplaceableEntry* getVictim(
        const ReplacementCandidates& candidates) const override;

    std::shared_ptr<ReplacementData> instantiateEntry() override;
    std::shared_ptr<ReplacementData> instantiateEntry(CacheBlk *blk);

    void promote(
        const std::shared_ptr<ReplacementData>& replacement_data) const;
    void updateTree(
        const std::shared_ptr<ReplacementData>& replacement_data) const;
    void touchTree(uint64_t touch_index, PLRUTree &tree) const;
    void dumpPreserveHist();
    void checkToFlushPreserveBits();
};

} // namespace replacement_policy
} // namespace gem5

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_ONE_TREE_LRU_EMISSARY_RP_HH__
