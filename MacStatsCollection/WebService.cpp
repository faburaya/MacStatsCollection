#include "stdafx.h"
#include "WebService.h"
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>
#include <3FD\logger.h>
#include <3FD\configuration.h>
#include "Utilities.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>

namespace application
{
    using namespace _3fd::core;


    /////////////////////
    // HTTP Transport
    /////////////////////


    /// <summary>
    /// Extracts the data from an HTTP request that carries "machine stats",
    /// and whose memory is allocated from WWS API heap
    /// </summary>
    /// <param name="request">The payload of the HTTP request.</param>
    /// <returns>A <see cref="StatsPackage"/> object for processing, containing the extracted data.</returns>
    std::unique_ptr<StatsPackage> ExtractStatsDataFrom(const SendStatsSampleRequest &request)
    {
        _ASSERTE(request.statsFloat32Count != 0 && request.statsInt32Count != 0);

        CALL_STACK_TRACE;
        
        try
        {
            auto package = std::make_unique<StatsPackage>();
            package->machine.clear();
            package->statSamplesFloat32.clear();
            package->statSamplesInt32.clear();
            
            package->timeSinceEpochInMillisecs = request.time;
            
            package->machine = request.machine;
            package->statSamplesInt32.reserve(request.statsInt32Count);
            package->statSamplesFloat32.reserve(request.statsFloat32Count);

            for (uint32_t idx = 0; idx < request.statsFloat32Count; ++idx)
            {
                auto &entry = request.statsFloat32[idx];
                package->statSamplesFloat32.emplace_back(entry.statName,
                                                         entry.statValue,
                                                         static_cast<Quality> (entry.quality));
            }

            for (uint32_t idx = 0; idx < request.statsInt32Count; ++idx)
            {
                auto &entry = request.statsInt32[idx];
                package->statSamplesInt32.emplace_back(entry.statName,
                                                       entry.statValue,
                                                       static_cast<Quality> (entry.quality));
            }

            return package;
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when copying data from service request: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Creates the request from.
    /// </summary>
    /// <param name="sample">The sample.</param>
    /// <param name="heap">The heap.</param>
    /// <returns></returns>
    SendStatsSampleRequest *CreateRequestFrom(const PerfCountersValues &sample, wws::WSHeap &heap)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace std::chrono;

            static const auto epoch = system_clock().from_time_t(0);

            auto request = heap.Alloc<SendStatsSampleRequest>();

            request->time = duration_cast<milliseconds>(sample.time - epoch).count();
            request->machine = const_cast<wchar_t *> (GetLocalHostName());

            // Stats whose value type is float 32 bits:

            request->statsFloat32Count = 4;
            request->statsFloat32 = heap.Alloc<listOfStatsFloat32_entry>(request->statsFloat32Count);

            short idx(0);
            request->statsFloat32[idx].statName = const_cast<wchar_t *> (ToStatName(PerfCounterCode::CpuUsage));
            request->statsFloat32[idx].statValue = sample.cpuTotalUsage.value;
            request->statsFloat32[idx].quality = static_cast<char> (sample.cpuTotalUsage.quality);
            ++idx;

            request->statsFloat32[idx].statName = const_cast<wchar_t *> (ToStatName(PerfCounterCode::DiskRead));
            request->statsFloat32[idx].statValue = sample.diskReadBytesPerSec.value;
            request->statsFloat32[idx].quality = static_cast<char> (sample.diskReadBytesPerSec.quality);
            ++idx;

            request->statsFloat32[idx].statName = const_cast<wchar_t *> (ToStatName(PerfCounterCode::DiskWrite));
            request->statsFloat32[idx].statValue = sample.diskWriteBytesPerSec.value;
            request->statsFloat32[idx].quality = static_cast<char> (sample.diskWriteBytesPerSec.quality);
            ++idx;

            request->statsFloat32[idx].statName = const_cast<wchar_t *> (ToStatName(PerfCounterCode::MemAvailable));
            request->statsFloat32[idx].statValue = sample.memAvailableMBytes.value;
            request->statsFloat32[idx].quality = static_cast<char> (sample.memAvailableMBytes.quality);
            ++idx;

            _ASSERTE(idx == request->statsFloat32Count);

            // Stats whose value type is integer 32 bits:

            request->statsInt32Count = 2;
            request->statsInt32 = heap.Alloc<listOfStatsInt32_entry>(request->statsInt32Count);

            idx = 0;
            request->statsInt32[idx].statName = const_cast<wchar_t *> (ToStatName(PerfCounterCode::ProcessCount));
            request->statsInt32[idx].statValue = sample.processCount.value;
            request->statsInt32[idx].quality = static_cast<char> (sample.processCount.quality);
            ++idx;

            request->statsInt32[idx].statName = const_cast<wchar_t *> (ToStatName(PerfCounterCode::ThreadCount));
            request->statsInt32[idx].statValue = sample.threadCount.value;
            request->statsInt32[idx].quality = static_cast<char> (sample.threadCount.quality);
            ++idx;

            _ASSERTE(idx == request->statsInt32Count);

            return request;
        }
        catch (IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when creating payload of service request: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    ///////////////////
    // Client Side
    ///////////////////


    /// <summary>
    /// The amount in bytes of memory to reserve in the
    /// heap of each request issued by the proxy.
    /// </summary>
    static constexpr ULONG proxyOperBaseHeapSize(512);


    /// <summary>
    /// Initializes a new instance of the <see cref="MacStatsCollectionClient"/> class.
    /// </summary>
    /// <param name="config">The proxy configuration.</param>
    MacStatsCollectionClient::MacStatsCollectionClient(const wws::SvcProxyConfig &config)
    try :
        WebServiceProxy(
            AppConfig::GetSettings().application.GetString("webSvcHostEndpoint", "NOT SET"),
            config,
            &wws::CreateWSProxy<WS_HTTP_BINDING_TEMPLATE, MacStatsCollectionBinding_CreateServiceProxy>
        ),
        m_heap(512)
    {
    }
    catch (IAppException &ex)
    {
        CALL_STACK_TRACE;
        throw AppException<std::runtime_error>("Failed to create HTTP client", ex);
    }
    catch (...)
    {
        CALL_STACK_TRACE;
        throw AppException<std::runtime_error>("Unexpected exception has been caught upon creation of HTTP client!");
    }

    /// <summary>
    /// Sends "machine stats" to the server.
    /// </summary>
    /// <param name="authKey">The authentication key.</param>
    /// <param name="sample">The sample of "machine stats" data.</param>
    /// <returns>
    /// Whether processing of the sample was available in the server.
    /// </returns>
    bool MacStatsCollectionClient::SendStatsSample(const wchar_t *authKey, const PerfCountersValues &sample)
    {
        CALL_STACK_TRACE;

        m_heap.Reset(); // reset the heap to make room for this request

        HRESULT hr;
        BOOL result;
        wws::WSError err;

        hr = MacStatsCollectionBinding_SendStatsSample(
            GetHandle(),
            const_cast<wchar_t *> (authKey),
            CreateRequestFrom(sample, m_heap),
            &result,
            m_heap.GetHandle(),
            nullptr, 0,
            nullptr,
            err.GetHandle()
        );

        err.RaiseExClientNotOK(hr, "Machine stats collection service returned an error", m_heap);

        return static_cast<bool> (result);
    }

    /// <summary>
    /// Requests closure of the web server.
    /// </summary>
    /// <returns>Whether the server accepted the request.</returns>
    bool MacStatsCollectionClient::CloseService()
    {
        CALL_STACK_TRACE;

        HRESULT hr;
        BOOL status;
        wws::WSError err;

        hr = MacStatsCollectionBinding_CloseService(
            GetHandle(),
            &status,
            m_heap.GetHandle(),
            nullptr, 0,
            nullptr,
            err.GetHandle()
        );

        err.RaiseExClientNotOK(hr, "Machine stats collection service returned an error", m_heap);

        return status == TRUE;
    }


    ///////////////////
    // Server Side
    ///////////////////


    // Implements "CloseService" in the server side
    HRESULT CALLBACK CloseService_ServerImpl(
        _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
        _Out_ BOOL *status,
        _In_ const WS_ASYNC_CONTEXT *asyncContext,
        _In_ WS_ERROR *wsErrorHandle)
    {
        CALL_STACK_TRACE;

        try
        {
            *status = TRUE; // always accept
            ServiceCloser::GetInstance().RequestClosure();
            return S_OK;
        }
        catch (IAppException &ex)
        {
            Logger::Write(ex, Logger::PRIO_CRITICAL);
            wws::SetSoapFault(ex, "CloseService", wsContextHandle, wsErrorHandle);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when creating payload of service request: " << ex.what();
            
            AppException<std::runtime_error> appEx(oss.str());
            Logger::Write(appEx, Logger::PRIO_CRITICAL);
            wws::SetSoapFault(appEx, "CloseService", wsContextHandle, wsErrorHandle);
        }

        return E_FAIL;
    }


    std::unique_ptr<ServiceCloser> ServiceCloser::singleton;


    /// <summary>
    /// Provides access to this singleton.
    /// This implementation is NOT THREAD SAFE!!!
    /// </summary>
    /// <returns>A reference to this singleton.</returns>
    ServiceCloser & ServiceCloser::GetInstance()
    {
        CALL_STACK_TRACE;

        try
        {
            if (!singleton)
                singleton.reset(new ServiceCloser());

            return *singleton;
        }
        catch (IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when instantiating service closer: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Finalizes the singleton.
    /// </summary>
    void ServiceCloser::Finalize()
    {
        singleton.reset(nullptr);
    }


    /// <summary>
    /// Requests the closure of the web server.
    /// This is meant to be called in a client request callback (worker thread).
    /// </summary>
    void ServiceCloser::RequestClosure()
    {
        CALL_STACK_TRACE;
        m_closeServiceEvent.Signalize();
    }


    /// <summary>
    /// Waits for close request.
    /// This is meant to stall the execution in the main thread.
    /// </summary>
    /// <param name="millisecs">The timeout in milliseconds before resuming execution.</param>
    /// <param name="host">The service host to close when the closure request arrives.</param>
    /// <returns>
    /// Whether the close request has arrived, instead of timing out.
    /// </returns>
    bool ServiceCloser::WaitForCloseRequest(unsigned long millisecs, wws::WebServiceHost &host)
    {
        CALL_STACK_TRACE;

        if (m_closeServiceEvent.WaitFor(millisecs))
        {
            std::cout << "The server received a shutdown request and will exit now..." << std::endl;
            
            // stall a little before exiting, so as to give time to the client to disconnect
            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            host.Close();
            return true;
        }

        return false;
    }

}// end of namespace application