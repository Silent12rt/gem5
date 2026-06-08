/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_EMISSARY_RL_CONTROL_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_EMISSARY_RL_CONTROL_HH__

#include <algorithm>

namespace gem5
{

/**
 * Process-local control channel from the RL L2 policy to the fetch stage.
 *
 * The current experiments use one CPU and one RL-controlled L2. Publishing
 * the active admission rate here lets fetch suppress auxiliary requests
 * before rejected requests can perturb the cache hierarchy.
 */
class EmissaryRLControl
{
  public:
    static void
    configure(bool enabled, double admission_rate)
    {
        _enabled = enabled;
        _admissionRate = std::clamp(admission_rate, 0.0, 100.0);
    }

    static void
    setAdmissionRate(double admission_rate)
    {
        _admissionRate = std::clamp(admission_rate, 0.0, 100.0);
    }

    static bool enabled() { return _enabled; }
    static double admissionRate() { return _admissionRate; }

  private:
    inline static bool _enabled = false;
    inline static double _admissionRate = 0.0;
};

} // namespace gem5

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_EMISSARY_RL_CONTROL_HH__
