#include "stdafx.h"
#include "PerfCountersReader.h"
#include "Utilities.h"
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>
#include <3FD\logger.h>
#include <3FD\utils_io.h>
#include <cstdlib>
#include <sstream>
#include <array>
#include <thread>
#include <PdhMsg.h>

namespace application
{
    static constexpr uint32_t numSupPerfCounters = 6;


    /// <summary>
    /// This array gathers the suffixes to compose the full path of the performance counters.
    /// The entries are ordered to match the indexes that are listed in the enumeration
    /// <see cref"PerfCounterCode"/>, so: DO NOT CHANGE THE ORDER!!!
    /// </summary>
    static const std::array<const wchar_t *, numSupPerfCounters> perfCounterPathSuffixes =
    {
        L"\\Processor(_Total)\\% Processor Time",
        L"\\Memory\\Available MBytes",
        L"\\LogicalDisk(_Total)\\Disk Read Bytes/sec",
        L"\\LogicalDisk(_Total)\\Disk Write Bytes/sec",
        L"\\System\\Processes",
        L"\\System\\Threads"
    };

    /// <summary>
    /// This array gathers formats for values of the performance counters.
    /// The entries are ordered to match the indexes that are listed in the enumeration
    /// <see cref"PerfCounterCode"/>, so: DO NOT CHANGE THE ORDER!!!
    /// </summary>
    static const std::array<DWORD, numSupPerfCounters> perfCounterValFormats =
    {
        PDH_FMT_DOUBLE,
        PDH_FMT_DOUBLE,
        PDH_FMT_DOUBLE,
        PDH_FMT_DOUBLE,
        PDH_FMT_LONG,
        PDH_FMT_LONG
    };

    /// <summary>
    /// This array gathers the labels for the supported performance counters.
    /// The entries are ordered to match the indexes that are listed in the enumeration
    /// <see cref"PerfCounterCode"/>, so: DO NOT CHANGE THE ORDER!!!
    /// </summary>
    static const std::array<const wchar_t *, numSupPerfCounters> perfCounterLabels =
    {
        L"cpu_usage_percentage",
        L"available_memory_mbytes",
        L"disk_read_bps",
        L"disk_write_bps",
        L"process_count",
        L"thread_count"
    };


    /// <summary>
    /// Converts an enumerated code for performance counter into a name for statistic.
    /// </summary>
    /// <param name="code">The <see cref="PerfCounterCode"/> code.</param>
    /// <returns>The correponding name os statistic.</returns>
    const wchar_t *ToStatName(PerfCounterCode code)
    {
        return perfCounterLabels[static_cast<uint32_t> (code)];
    }


    /// <summary>
    /// Translates a value status reported by PDH API to its corresponding quality.
    /// </summary>
    /// <param name="valStatus">The value status.</param>
    /// <returns>The quality of the value as enumerated by <see cref="Quality"/>.</returns>
    static Quality ToQuality(DWORD valStatus)
    {
        switch (valStatus)
        {
        case PDH_CSTATUS_VALID_DATA:
        case PDH_CSTATUS_NEW_DATA:
            return Quality::Good;

        case PDH_CSTATUS_INVALID_DATA:
        case PDH_CSTATUS_NO_OBJECT:
        case PDH_CSTATUS_NO_INSTANCE:
        case PDH_CSTATUS_NO_COUNTER:
        case PDH_CALC_NEGATIVE_DENOMINATOR:
        case PDH_CALC_NEGATIVE_TIMEBASE:
        case PDH_CALC_NEGATIVE_VALUE:
            return Quality::Invalid;

        case PDH_CSTATUS_NO_MACHINE:
        case PDH_CSTATUS_NO_COUNTERNAME:
        case PDH_CSTATUS_BAD_COUNTERNAME:
        case PDH_MORE_DATA:
            return Quality::Error;

        case PDH_CSTATUS_ITEM_NOT_VALIDATED:
            return Quality::Unknown;

        default:
            _ASSERTE(false);
            return Quality::Unknown;
        }
    }


    using namespace _3fd;
    using namespace _3fd::core;


    /// <summary>
    /// Checks the status returned by a PDH API call,
    /// throwing an exception whenever an error takes place.
    /// </summary>
    /// <param name="status">The returned status.</param>
    /// <param name="message">The main message in case of error.</param>
    /// <param name="pdhApiFuncName">Name of the PDH API function.</param>
    /// <param name="justLog">if set to <c>true</c> just log the error, do not throw exception.</param>
    static void CheckStatus(PDH_STATUS status,
                            const char *message,
                            const char *pdhApiFuncName,
                            bool justLog = false,
                            Logger::Priority logPrio = Logger::PRIO_CRITICAL)
    {
        if (status == ERROR_SUCCESS)
            return;

        static HMODULE pdhLibHandle = LoadLibraryW(L"pdh.dll");

        std::ostringstream oss;
        oss << message << " - ";
        WWAPI::AppendDWordErrorMessage(status, pdhApiFuncName, oss, pdhLibHandle);

        if (!justLog)
            throw AppException<std::runtime_error>(oss.str());
        else
            Logger::Write(oss.str(), logPrio);
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="PerfCountersReader"/> class.
    /// </summary>
    PerfCountersReader::PerfCountersReader()
    {
        CALL_STACK_TRACE;

        try
        {
            /* Use random numbers for user data just in case this class has more
            than 1 instance and collision of codes could cause some issue with
            PDH API. This starts the random number generator: */
            srand((unsigned int)clock());

            m_queryUserData = rand();

            // Create a PDH query to get performance counters data:

            ScopedLogWrite logScope(
                "Setting up performance counters... ",
                Logger::PRIO_INFORMATION, "done!",
                Logger::PRIO_ERROR, "FAILED!"
            );

            PDH_STATUS status;
            status = PdhOpenQueryW(nullptr, m_queryUserData, &m_pdhQuery);
            CheckStatus(status, "Failed to create PDH query", "PdhOpenQuery");

            // Add some performance counters to the PDH query:

            m_perfCounters.reserve(perfCounterPathSuffixes.size());

            for (auto pathSuffix : perfCounterPathSuffixes)
            {
                m_perfCounters.emplace_back();
                auto &perfCounter = m_perfCounters.back();

                perfCounter.userData = rand();

                utils::SerializeTo(perfCounter.fullPath, L"\\\\", GetLocalHostName(), pathSuffix);

                status = PdhAddEnglishCounterW(m_pdhQuery,
                                               perfCounter.fullPath.c_str(),
                                               perfCounter.userData,
                                               &perfCounter.handle);

                CheckStatus(status, "Failed to performance counter to PDH query", "PdhAddCounter");
            }

            logScope.LogSuccess();
        }
        catch (IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when creating reader for performance counters: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Finalizes an instance of the <see cref="PerfCountersReader"/> class.
    /// </summary>
    PerfCountersReader::~PerfCountersReader()
    {
        CALL_STACK_TRACE;
        PDH_STATUS status;
        status = PdhCloseQuery(m_pdhQuery);
        CheckStatus(status, "Failed to release resources associated with PDH query", "PdhCloseQuery", true);
    }


    /// <summary>
    /// Sets the value of a given performance counter in a struct <see cref="PerfCountersValues" />.
    /// </summary>
    /// <param name="object">The struct whose value will be set.</param>
    /// <param name="perfCounterCode">The code of the performance counter value to set.</param>
    void PerfCountersReader::SetValueIn(PerfCountersValues &object, PerfCounterCode perfCounterCode) const
    {
        CALL_STACK_TRACE;

        PDH_STATUS status;
        PDH_FMT_COUNTERVALUE pdhFormatVal;
        auto pcIndex = static_cast<uint32_t> (perfCounterCode);
        
        status = PdhGetFormattedCounterValue(m_perfCounters[pcIndex].handle,
                                             perfCounterValFormats[pcIndex],
                                             nullptr,
                                             &pdhFormatVal);

        if (status != ERROR_SUCCESS)
        {
            std::array<char, 128> message;
            utils::SerializeTo(message, "Failed to get formatted value for performance counter ", ToStatName(perfCounterCode));
            CheckStatus(status, message.data(), "PdhGetFormattedCounterValue");
        }

        auto quality = ToQuality(pdhFormatVal.CStatus);

        switch (perfCounterCode)
        {
        case PerfCounterCode::CpuUsage:
            object.cpuTotalUsage.value = static_cast<float> (pdhFormatVal.doubleValue);
            object.cpuTotalUsage.quality = quality;
            break;

        case PerfCounterCode::MemAvailable:
            object.memAvailableMBytes.value = static_cast<float> (pdhFormatVal.doubleValue);
            object.memAvailableMBytes.quality = quality;
            break;

        case PerfCounterCode::DiskRead:
            object.diskReadBytesPerSec.value = static_cast<float> (pdhFormatVal.doubleValue);
            object.diskReadBytesPerSec.quality = quality;
            break;

        case PerfCounterCode::DiskWrite:
            object.diskWriteBytesPerSec.value = static_cast<float> (pdhFormatVal.doubleValue);
            object.diskWriteBytesPerSec.quality = quality;
            break;

        case PerfCounterCode::ProcessCount:
            object.processCount.value = static_cast<uint16_t> (pdhFormatVal.longValue);
            object.processCount.quality = quality;
            break;

        case PerfCounterCode::ThreadCount:
            object.threadCount.value = static_cast<uint16_t> (pdhFormatVal.longValue);
            object.threadCount.quality = quality;
            break;

        default:
            _ASSERTE(false);
            break;
        }
        
        // In case quality for counter is not good, report to log:
        if (quality != Quality::Good)
        {
            std::array<char, 256> message;
            utils::SerializeTo(message, "Quality of performance counter '", m_perfCounters[pcIndex].fullPath, "' is NOT GOOD");
            CheckStatus(pdhFormatVal.CStatus, message.data(), "PdhGetFormattedCounterValue", true, Logger::PRIO_WARNING);
        }
    }


    /// <summary>
    /// Gets the current values for the performance counters.
    /// This call blocks for at least 1 second, in order to
    /// calculate rates given 2 sequential samples.
    /// </summary>
    /// <returns>The values for the set of monitored system performance counters.</returns>
    PerfCountersValues PerfCountersReader::GetCurrentValues() const
    {
        CALL_STACK_TRACE;

        // Take first sample:

        PDH_STATUS status;
        status = PdhCollectQueryData(m_pdhQuery);
        CheckStatus(status, "Failed to collect data for performance counters", "PdhCollectQueryData");

        std::this_thread::sleep_for(std::chrono::seconds(1));

        PerfCountersValues values;
        values.time = std::chrono::system_clock().now();

        // Take second sample:

        status = PdhCollectQueryData(m_pdhQuery);
        CheckStatus(status, "Failed to collect data for performance counters", "PdhCollectQueryData");

        // Get the calculated values for the performance counters:
        for (int idx = 0; idx < numSupPerfCounters; ++idx)
            SetValueIn(values, static_cast<PerfCounterCode> (idx));

        return values;
    }

}// end of namespace application
