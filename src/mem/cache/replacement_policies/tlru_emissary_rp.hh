/**
 * @file
 * Declaration of the paper TLRU EMISSARY replacement policy.
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_TLRU_EMISSARY_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_TLRU_EMISSARY_RP_HH__

#include <cstdint>
#include <memory>

#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"

namespace gem5
{

struct TLRUEmissaryRPParams;

namespace replacement_policy
{

class TLRUEmissary : public Base
{
  protected:
    struct TLRUEmissaryReplData : ReplacementData
    {
        Tick lastTouchTick;

        TLRUEmissaryReplData() : lastTouchTick(0) {}
    };

  public:
    using Params = TLRUEmissaryRPParams;

    int lru_ways;
    int preserve_ways;
    uint64_t last_tick;
    int numSets;
    int numWays;
    uint64_t flush_freq_in_cycles;
    TaggedIndexingPolicy *indexingPolicy;

    explicit TLRUEmissary(const Params &p);
    ~TLRUEmissary() = default;

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

    void dumpPreserveHist();
    void checkToFlushPreserveBits();
};

} // namespace replacement_policy
} // namespace gem5

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_TLRU_EMISSARY_RP_HH__
