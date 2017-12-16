#ifndef __CommonDataExchange_h__ // header guard
#define __CommonDataExchange_h__

#include <cinttypes>
#include <string>
#include <vector>
#include <chrono>

namespace application
{
    using std::string;


    /// <summary>
    /// Enumerates possible states for quality, with a hardcoded numeric code for each one.
    /// Such codes are the same that are recorded into the database, so DO NOT CHANGE THEM!!!!
    /// </summary>
    enum class Quality : int8_t
    {
        Good = 0, // okay!
        Invalid = 4, // client is fine, but data is not valid
        Error = 8, // some implementation or runtime error took place, see the client log
        Unknown = 12 // unknown (or unexpected) status
    };


    ///////////////////
    // Client Side
    ///////////////////

    template <typename ValType>
    struct ValueWithQuality
    {
        ValType value;
        Quality quality;
    };

    /// <summary>
    /// Holds all the performance counters values retrieved
    /// by <see cref="PerfCountersReader"/> in a single call.
    /// </summary>
    struct PerfCountersValues
    {
        std::chrono::time_point<std::chrono::system_clock> time;

        ValueWithQuality<float> cpuTotalUsage;
        ValueWithQuality<float> memAvailableMBytes;
        ValueWithQuality<float> diskReadBytesPerSec;
        ValueWithQuality<float> diskWriteBytesPerSec;
        ValueWithQuality<uint16_t> processCount;
        ValueWithQuality<uint16_t> threadCount;
    };


    /// <summary>
    /// Enumerates codes that represent the available performance counters
    /// (they must match the internal array indexes: DO NOT CHANGE THESE CODES!!!).
    /// </summary>
    enum class PerfCounterCode : uint32_t
    {
        CpuUsage = 0,
        MemAvailable,
        DiskRead,
        DiskWrite,
        ProcessCount,
        ThreadCount
    };

    const wchar_t *ToStatName(PerfCounterCode code);


    ///////////////////
    // Server Side
    ///////////////////

    /// <summary>
    /// Holds data for a single sample of statistic value.
    /// </summary>
    template <typename ValType>
    struct StatSampleValue
    {
        std::wstring statName;
        ValType value;
        Quality quality;

        StatSampleValue(const wchar_t *p_statName, ValType p_value, Quality p_quality)
            : statName(p_statName), value(p_value), quality(p_quality) {}

        StatSampleValue(std::wstring &&p_statName, ValType p_value, Quality p_quality)
            : statName(std::move(p_statName)), value(p_value), quality(p_quality) {}

        StatSampleValue(const std::wstring &p_statName, ValType p_value, Quality p_quality)
            : statName(p_statName), value(p_value), quality(p_quality) {}
    };


    /// <summary>
    /// Packages all useful data that comes in <see cref="_SendStatsSampleRequest"/>,
    /// which will also be used to write into storage.
    /// </summary>
    struct StatsPackage
    {
        int64_t timeSinceEpochInMillisecs;
        std::wstring machine;
        std::vector<StatSampleValue<float>> statSamplesFloat32;
        std::vector<StatSampleValue<int>> statSamplesInt32;

        StatsPackage() {};

        StatsPackage(StatsPackage &&ob)
            : machine(std::move(ob.machine))
            , statSamplesFloat32(std::move(ob.statSamplesFloat32))
            , statSamplesInt32(std::move(ob.statSamplesInt32))
        {}
    };

    typedef StatsPackage StorageWriteTask;

}// end of namespace application

#endif // end of header guard
