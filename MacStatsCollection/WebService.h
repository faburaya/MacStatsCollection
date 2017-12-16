#ifndef __WebService_h__ // header guard
#define __WebService_h__

#include <3FD\web_wws_webserviceproxy.h>
#include <3FD\web_wws_webservicehost.h>
#include <3FD\utils.h>
#include "CommonDataExchange.h"
#include "MacStatsCollection.wsdl.h"
#include <memory>
#include <vector>

namespace application
{
    using namespace _3fd;
    using namespace _3fd::web;


    /////////////////////
    // HTTP Transport
    /////////////////////

    std::unique_ptr<StatsPackage> ExtractStatsDataFrom(const SendStatsSampleRequest &request);

    SendStatsSampleRequest *CreateRequestFrom(const PerfCountersValues &sample, wws::WSHeap &heap);


    ///////////////////
    // Client Side
    ///////////////////


    /// <summary>
    /// Implements the web client based in Microsoft WWS API.
    /// </summary>
    /// <seealso cref="wws::WebServiceProxy" />
    class MacStatsCollectionClient : public wws::WebServiceProxy
    {
    private:

        wws::WSHeap m_heap;

    public:

        MacStatsCollectionClient(const wws::SvcProxyConfig &config);

        bool SendStatsSample(const wchar_t *authKey, const PerfCountersValues &sample);

        bool CloseService();
    };


    ///////////////////
    // Server Side
    ///////////////////


    HRESULT CALLBACK CloseService_ServerImpl(
        _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
        _Out_ BOOL *status,
        _In_ const WS_ASYNC_CONTEXT *asyncContext,
        _In_ WS_ERROR *wsErrorHandle
    );


    /// <summary>
    /// This singleton has the only purpose of helping
    /// close the web service via client request.
    /// This implementation is NOT THREAD SAFE!!!
    /// </summary>
    class ServiceCloser
    {
    private:

        utils::Event m_closeServiceEvent;

        static std::unique_ptr<ServiceCloser> singleton;

        ServiceCloser() {}

    public:

        static ServiceCloser &GetInstance();

        static void Finalize();

        void RequestClosure();

        bool WaitForCloseRequest(unsigned long millisecs, wws::WebServiceHost &host);
    };

}// end of namespace application

#endif // end of header guard
