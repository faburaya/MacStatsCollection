#ifndef __PerfCountersReader_h__ // header guard
#define __PerfCountersReader_h__

#include "CommonDataExchange.h"
#include <cinttypes>
#include <string>
#include <vector>
#include <Pdh.h>

namespace application
{
    /// <summary>
    /// Reads system performance counters.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class PerfCountersReader
    {
    private:

        DWORD_PTR m_queryUserData;
        PDH_HQUERY m_pdhQuery;

        struct PerfCounter
        {
            std::wstring fullPath;
            PDH_HCOUNTER handle;
            DWORD_PTR userData;
        };

        std::vector<PerfCounter> m_perfCounters;

        void SetValueIn(PerfCountersValues &object, PerfCounterCode perfCounterCode) const;

    public:

        PerfCountersReader();

        PerfCountersReader(const PerfCountersReader &) = delete;

        ~PerfCountersReader();

        PerfCountersValues GetCurrentValues() const;
    };

}// end of namespace application

#endif // end of header guard