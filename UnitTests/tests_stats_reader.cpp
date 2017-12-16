#include "stdafx.h"
#include <3FD\runtime.h>
#include <3FD\callstacktracer.h>
#include <3FD\utils_io.h>
#include "PerfCountersReader.h"

#define format utils::FormatArg

namespace unit_tests
{
    using namespace _3fd;
    using namespace _3fd::core;


    void HandleException();


    /// <summary>
    /// Tests the <see cref="application::PerfCountersReader"/> class.
    /// </summary>
    TEST(TestCase_DataAccess, TestPerfCountersReader)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace application;
            using namespace std::chrono;

            PerfCountersReader pcReader;

            for (int idx = 0; idx < 2; ++idx)
            {
                auto pcVals = pcReader.GetCurrentValues();

                auto timeSinceEpochInMillisecs = duration_cast<milliseconds>(pcVals.time - system_clock().from_time_t(0)).count();
                time_t timeSinceEpochInSecs = timeSinceEpochInMillisecs / 1000;

                std::array<char, 21> timestamp;
                strftime(timestamp.data(), timestamp.size(), "%Y-%b-%d %H:%M:%S", localtime(&timeSinceEpochInSecs));

                auto remainingMillisecs = static_cast<int16_t> (timeSinceEpochInMillisecs - timeSinceEpochInSecs * 1000);

                utils::SerializeTo<char>(stdout, "\nStats at ", timestamp.data(), " + ", remainingMillisecs, " ms\n");

                utils::SerializeTo<char>(stdout,
                    format(ToStatName(PerfCounterCode::CpuUsage)).width(24),
                    ": value = ", pcVals.cpuTotalUsage.value,
                    " / quality = ", static_cast<int8_t> (pcVals.cpuTotalUsage.quality), '\n');

                utils::SerializeTo<char>(stdout,
                    format(ToStatName(PerfCounterCode::MemAvailable)).width(24),
                    ": value = ", pcVals.memAvailableMBytes.value,
                    " / quality = ", static_cast<int8_t> (pcVals.memAvailableMBytes.quality), '\n');

                utils::SerializeTo<char>(stdout,
                    format(ToStatName(PerfCounterCode::DiskRead)).width(24),
                    ": value = ", pcVals.diskReadBytesPerSec.value,
                    " / quality = ", static_cast<int8_t> (pcVals.diskReadBytesPerSec.quality), '\n');

                utils::SerializeTo<char>(stdout,
                    format(ToStatName(PerfCounterCode::DiskWrite)).width(24),
                    ": value = ", pcVals.diskWriteBytesPerSec.value,
                    " / quality = ", static_cast<int8_t> (pcVals.diskWriteBytesPerSec.quality), '\n');

                utils::SerializeTo<char>(stdout,
                    format(ToStatName(PerfCounterCode::ProcessCount)).width(24),
                    ": value = ", pcVals.processCount.value,
                    " / quality = ", static_cast<int8_t> (pcVals.processCount.quality), '\n');

                utils::SerializeTo<char>(stdout,
                    format(ToStatName(PerfCounterCode::ThreadCount)).width(24),
                    ": value = ", pcVals.threadCount.value,
                    " / quality = ", static_cast<int8_t> (pcVals.threadCount.quality), '\n');
            }
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace unit_tests