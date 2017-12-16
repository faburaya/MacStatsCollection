#include "stdafx.h"
#include <3FD\runtime.h>
#include <3FD\callstacktracer.h>
#include "WebService.h"
#include "Utilities.h"
#include <map>
#include <string>
#include <chrono>


namespace unit_tests
{
    using namespace _3fd;
    using namespace _3fd::core;
    using namespace _3fd::web;


    void HandleException();


    // Contains the expected serialized data for the SendStatsSample request
    struct ExpectedRequest
    {
        int64_t time;
        std::wstring machine;
        std::wstring key;

        struct ExpectedSampleFloat
        {
            float value;
            int8_t quality;
        };

        std::map<uint32_t, ExpectedSampleFloat> samplesFloatByCode;
        std::map<std::wstring, ExpectedSampleFloat> samplesFloatByName;

        struct ExpectedSampleInt
        {
            int value;
            int8_t quality;
        };

        std::map<uint32_t, ExpectedSampleInt> samplesIntByCode;
        std::map<std::wstring, ExpectedSampleInt> samplesIntByName;

        static ExpectedRequest data;

        // Creates the data that will be expected in tests of request content and transformation
        void Initialize()
        {
            static bool done(false);

            if (done)
                return;

            using namespace application;

            time = 42;
            machine = GetLocalHostName();
            key = L"ExcuseMe";

            auto code = PerfCounterCode::CpuUsage;
            samplesFloatByName[ToStatName(code)] =
                samplesFloatByCode[static_cast<uint32_t> (code)] =
                    ExpectedSampleFloat{ 12.0F, static_cast<int8_t> (Quality::Good) };

            code = PerfCounterCode::MemAvailable;
            samplesFloatByName[ToStatName(code)] =
                samplesFloatByCode[static_cast<uint32_t> (code)] =
                    ExpectedSampleFloat{ 23.0F, static_cast<int8_t> (Quality::Invalid) };
            
            code = PerfCounterCode::DiskRead;
            samplesFloatByName[ToStatName(code)] =
                samplesFloatByCode[static_cast<uint32_t> (code)] =
                    ExpectedSampleFloat{ 34.0F, static_cast<int8_t> (Quality::Error) };
            
            code = PerfCounterCode::DiskWrite;
            samplesFloatByName[ToStatName(code)] =
                samplesFloatByCode[static_cast<uint32_t> (code)] =
                    ExpectedSampleFloat{ 45.0F, static_cast<int8_t> (Quality::Unknown) };
            
            
            code = PerfCounterCode::ProcessCount;
            samplesIntByName[ToStatName(code)] =
                samplesIntByCode[static_cast<uint32_t> (code)] =
                    ExpectedSampleInt{ 56, static_cast<int8_t> (Quality::Good) };
            
            
            code = PerfCounterCode::ThreadCount;
            samplesIntByName[ToStatName(code)] =
                samplesIntByCode[static_cast<uint32_t> (code)] =
                ExpectedSampleInt{ 67, static_cast<int8_t> (Quality::Unknown) };

            done = true;
        }
    };


    ExpectedRequest ExpectedRequest::data;


    // Uses a test implementation to check whether transport from client to server did okay
    HRESULT CALLBACK SendStatsSample_ServerImpl(
        _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
        _In_z_ WCHAR *key,
        _In_ SendStatsSampleRequest *payload,
        _Out_ BOOL *status,
        _In_ const WS_ASYNC_CONTEXT *wsAsyncContext,
        _In_ WS_ERROR *wsErrorHandle)
    {
        CALL_STACK_TRACE;

        *status = static_cast<BOOL> (STATUS_OKAY);

        bool fail(false);

        // Check whether raw payload is correct:

        EXPECT_EQ(ExpectedRequest::data.time, payload->time);
        EXPECT_EQ(ExpectedRequest::data.machine, payload->machine);
        EXPECT_EQ(ExpectedRequest::data.key, key);

        EXPECT_EQ(ExpectedRequest::data.samplesFloatByName.size(), payload->statsFloat32Count);
        
        for (int idx = 0; idx < payload->statsFloat32Count; ++idx)
        {
            auto iter = ExpectedRequest::data.samplesFloatByName.find(payload->statsFloat32[idx].statName);
            
            EXPECT_TRUE(ExpectedRequest::data.samplesFloatByName.end() != iter)
                << "stat name in request is " << payload->statsFloat32[idx].statName;

            if (ExpectedRequest::data.samplesFloatByName.end() == iter)
            {
                fail = true;
                continue;
            }

            auto &expectedSample = iter->second;

            EXPECT_EQ(expectedSample.value, payload->statsFloat32[idx].statValue);
            EXPECT_EQ(expectedSample.quality, payload->statsFloat32[idx].quality);
        }
        
        EXPECT_EQ(ExpectedRequest::data.samplesIntByName.size(), payload->statsInt32Count);

        for (int idx = 0; idx < payload->statsInt32Count; ++idx)
        {
            auto iter = ExpectedRequest::data.samplesIntByName.find(payload->statsInt32[idx].statName);

            EXPECT_TRUE(ExpectedRequest::data.samplesIntByName.end() != iter)
                << "stat name in request is " << payload->statsInt32[idx].statName;

            if (ExpectedRequest::data.samplesIntByName.end() == iter)
            {
                fail = true;
                continue;
            }

            auto &expectedSample = iter->second;

            EXPECT_EQ(expectedSample.value, payload->statsInt32[idx].statValue);
            EXPECT_EQ(expectedSample.quality, payload->statsInt32[idx].quality);
        }

        if (fail)
            return S_OK;

        // Now check whether transformation of types is correct:

        auto statsPackage = application::ExtractStatsDataFrom(*payload);

        EXPECT_EQ(ExpectedRequest::data.time, statsPackage->timeSinceEpochInMillisecs);
        EXPECT_EQ(ExpectedRequest::data.machine, statsPackage->machine);
        
        EXPECT_EQ(ExpectedRequest::data.samplesFloatByName.size(), statsPackage->statSamplesFloat32.size());

        for (auto &sample : statsPackage->statSamplesFloat32)
        {
            auto iter = ExpectedRequest::data.samplesFloatByName.find(sample.statName.c_str());

            EXPECT_TRUE(ExpectedRequest::data.samplesFloatByName.end() != iter)
                << "stat name in request is " << sample.statName;

            if (ExpectedRequest::data.samplesFloatByName.end() == iter)
            {
                fail = true;
                continue;
            }

            auto &expectedSample = iter->second;

            EXPECT_EQ(expectedSample.value, sample.value);
            EXPECT_EQ(expectedSample.quality, static_cast<int8_t> (sample.quality));
        }

        EXPECT_EQ(ExpectedRequest::data.samplesIntByName.size(), statsPackage->statSamplesInt32.size());

        for (auto &sample : statsPackage->statSamplesInt32)
        {
            auto iter = ExpectedRequest::data.samplesIntByName.find(sample.statName.c_str());

            EXPECT_TRUE(ExpectedRequest::data.samplesIntByName.end() != iter)
                << "stat name in request is " << sample.statName;

            if (ExpectedRequest::data.samplesIntByName.end() == iter)
            {
                fail = true;
                continue;
            }

            auto &expectedSample = iter->second;

            EXPECT_EQ(expectedSample.value, sample.value);
            EXPECT_EQ(expectedSample.quality, static_cast<int8_t> (sample.quality));
        }
        
        return S_OK;
    }


    /// <summary>
    /// Tests the <see cref="application::MacStatsCollectionClient"/> class: setup
    /// </summary>
    TEST(TestCase_WebService, TestMacStatsCollectionClient_Setup)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Create the HTTP client:
            wws::SvcProxyConfig proxyCfg;
            application::MacStatsCollectionClient client(proxyCfg);
            client.Open();

            EXPECT_TRUE(client.Close());
        }
        catch (...)
        {
            HandleException();
        }
    }


    // Initialized some performance counter data for testing
    static void Initialize(application::PerfCountersValues &pcvals)
    {
        ExpectedRequest::data.Initialize();

        using namespace std::chrono;
        using namespace application;

        pcvals.time = system_clock().from_time_t(0) + milliseconds(ExpectedRequest::data.time);

        ExpectedRequest::ExpectedSampleFloat expectedSampleFloat;
        expectedSampleFloat = ExpectedRequest::data.samplesFloatByCode[static_cast<uint32_t> (PerfCounterCode::CpuUsage)];
        pcvals.cpuTotalUsage.value = expectedSampleFloat.value;
        pcvals.cpuTotalUsage.quality = static_cast<Quality> (expectedSampleFloat.quality);

        expectedSampleFloat = ExpectedRequest::data.samplesFloatByCode[static_cast<uint32_t> (PerfCounterCode::MemAvailable)];
        pcvals.memAvailableMBytes.value = expectedSampleFloat.value;
        pcvals.memAvailableMBytes.quality = static_cast<Quality> (expectedSampleFloat.quality);

        expectedSampleFloat = ExpectedRequest::data.samplesFloatByCode[static_cast<uint32_t> (PerfCounterCode::DiskRead)];
        pcvals.diskReadBytesPerSec.value = expectedSampleFloat.value;
        pcvals.diskReadBytesPerSec.quality = static_cast<Quality> (expectedSampleFloat.quality);

        expectedSampleFloat = ExpectedRequest::data.samplesFloatByCode[static_cast<uint32_t> (PerfCounterCode::DiskWrite)];
        pcvals.diskWriteBytesPerSec.value = expectedSampleFloat.value;
        pcvals.diskWriteBytesPerSec.quality = static_cast<Quality> (expectedSampleFloat.quality);

        ExpectedRequest::ExpectedSampleInt expectedSampleInt;
        expectedSampleInt = ExpectedRequest::data.samplesIntByCode[static_cast<uint32_t> (PerfCounterCode::ProcessCount)];
        pcvals.processCount.value = expectedSampleInt.value;
        pcvals.processCount.quality = static_cast<Quality> (expectedSampleInt.quality);

        expectedSampleInt = ExpectedRequest::data.samplesIntByCode[static_cast<uint32_t> (PerfCounterCode::ThreadCount)];
        pcvals.threadCount.value = expectedSampleInt.value;
        pcvals.threadCount.quality = static_cast<Quality> (expectedSampleInt.quality);
    }


    /// <summary>
    /// Tests creation of request payload in client side.
    /// </summary>
    TEST(TestCase_WebService, TestClient_CreateRequestPayload)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Generate performance counters data:
            application::PerfCountersValues pcvals;
            Initialize(pcvals);

            // Create the payload for the HTTP request:
            wws::WSHeap heap(256);
            auto payload = CreateRequestFrom(pcvals, heap);

            // Check whether payload is correct:

            EXPECT_EQ(ExpectedRequest::data.time, payload->time);
            EXPECT_EQ(ExpectedRequest::data.machine, payload->machine);

            EXPECT_EQ(ExpectedRequest::data.samplesFloatByName.size(), payload->statsFloat32Count);

            for (int idx = 0; idx < payload->statsFloat32Count; ++idx)
            {
                auto iter = ExpectedRequest::data.samplesFloatByName.find(payload->statsFloat32[idx].statName);

                EXPECT_TRUE(ExpectedRequest::data.samplesFloatByName.end() != iter)
                    << "stat name in request is " << payload->statsFloat32[idx].statName;

                if (ExpectedRequest::data.samplesFloatByName.end() == iter)
                    continue;

                auto &expectedSample = iter->second;

                EXPECT_EQ(expectedSample.value, payload->statsFloat32[idx].statValue);
                EXPECT_EQ(expectedSample.quality, payload->statsFloat32[idx].quality);
            }

            EXPECT_EQ(ExpectedRequest::data.samplesIntByName.size(), payload->statsInt32Count);

            for (int idx = 0; idx < payload->statsInt32Count; ++idx)
            {
                auto iter = ExpectedRequest::data.samplesIntByName.find(payload->statsInt32[idx].statName);

                EXPECT_TRUE(ExpectedRequest::data.samplesIntByName.end() != iter)
                    << "stat name in request is " << payload->statsInt32[idx].statName;

                if (ExpectedRequest::data.samplesIntByName.end() == iter)
                    continue;

                auto &expectedSample = iter->second;

                EXPECT_EQ(expectedSample.value, payload->statsInt32[idx].statValue);
                EXPECT_EQ(expectedSample.quality, payload->statsInt32[idx].quality);
            }
        }
        catch (...)
        {
            HandleException();
        }
    }


    /// <summary>
    /// Tests web server setup.
    /// </summary>
    TEST(TestCase_WebService, TestServer_Setup)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Function tables contains the service implementation:
            MacStatsCollectionBindingFunctionTable funcTableSvc = {
                &SendStatsSample_ServerImpl,
                &application::CloseService_ServerImpl
            };

            // Create the web service host with default configurations:
            wws::SvcEndpointsConfig hostCfg;

            wws::ServiceBindings bindings;

            /* Map the binding used for the endpoint to
            the corresponding implementations: */
            bindings.MapBinding(
                "MacStatsCollectionBinding",
                &funcTableSvc,
                &wws::CreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE, MacStatsCollectionBindingFunctionTable, MacStatsCollectionBinding_CreateServiceEndpoint>
            );

            // Create the service host:
            wws::WebServiceHost host(2048);
            host.Setup("MacStatsCollection.wsdl", hostCfg, bindings, nullptr, false);
            host.Open(); // start listening
        }
        catch (...)
        {
            HandleException();
        }
    }


    /// <summary>
    /// Tests web server being closed by client.
    /// </summary>
    TEST(TestCase_WebService, TestServerClosure)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            // Function tables contains the service implementation:
            MacStatsCollectionBindingFunctionTable funcTableSvc = {
                &SendStatsSample_ServerImpl,
                &application::CloseService_ServerImpl
            };

            // Create the web service host with default configurations:
            wws::SvcEndpointsConfig hostCfg;

            wws::ServiceBindings bindings;

            /* Map the binding used for the endpoint to
            the corresponding implementations: */
            bindings.MapBinding(
                "MacStatsCollectionBinding",
                &funcTableSvc,
                &wws::CreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE, MacStatsCollectionBindingFunctionTable, MacStatsCollectionBinding_CreateServiceEndpoint>
            );

            // Create the service host:
            wws::WebServiceHost host(2048);
            host.Setup("MacStatsCollection.wsdl", hostCfg, bindings, nullptr, false);
            host.Open(); // start listening

            // Create the HTTP client:
            wws::SvcProxyConfig proxyCfg;
            application::MacStatsCollectionClient client(proxyCfg);
            client.Open();

            // Request closure of server
            EXPECT_TRUE(client.CloseService());

            // Wait for the host closure requested by client
            EXPECT_TRUE(
                application::ServiceCloser::GetInstance().WaitForCloseRequest(3000, host)
            );

            EXPECT_TRUE(client.Close());

            application::ServiceCloser::Finalize();
        }
        catch (...)
        {
            HandleException();
        }
    }


    /// <summary>
    /// Tests data SOAP over HTTP transport.
    /// </summary>
    TEST(TestCase_WebService, TestSoapHttpTransport)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            ExpectedRequest::data.Initialize();

            // Function tables contains the service implementation:
            MacStatsCollectionBindingFunctionTable funcTableSvc = {
                &SendStatsSample_ServerImpl,
                &application::CloseService_ServerImpl
            };

            // Create the web service host with default configurations:
            wws::SvcEndpointsConfig hostCfg;

            wws::ServiceBindings bindings;

            /* Map the binding used for the endpoint to
            the corresponding implementations: */
            bindings.MapBinding(
                "MacStatsCollectionBinding",
                &funcTableSvc,
                &wws::CreateServiceEndpoint<WS_HTTP_BINDING_TEMPLATE, MacStatsCollectionBindingFunctionTable, MacStatsCollectionBinding_CreateServiceEndpoint>
            );

            // Create the service host:
            wws::WebServiceHost host(2048);
            host.Setup("MacStatsCollection.wsdl", hostCfg, bindings, nullptr, false);
            host.Open(); // start listening

            // Create the HTTP client:
            wws::SvcProxyConfig proxyCfg;
            application::MacStatsCollectionClient client(proxyCfg);
            client.Open();

            // Generate performance counters data:
            application::PerfCountersValues pcvals;
            Initialize(pcvals);

            EXPECT_EQ(static_cast<bool> (STATUS_OKAY),
                client.SendStatsSample(ExpectedRequest::data.key.c_str(), pcvals)
            );

            // Request closure of server
            EXPECT_TRUE(client.CloseService());

            // Wait for the host closure requested by client
            EXPECT_TRUE(
                application::ServiceCloser::GetInstance().WaitForCloseRequest(3000, host)
            );

            EXPECT_TRUE(client.Close());

            application::ServiceCloser::Finalize();
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace unit_tests