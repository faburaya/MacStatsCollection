// MSCServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <3FD\runtime.h>
#include <3FD\configuration.h>
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>
#include <3FD\logger.h>
#include "WebService.h"
#include "TasksQueue.h"
#include "Authenticator.h"
#include "MSDStorageWriter.h"
#include <iostream>
#include <iomanip>
#include <chrono>


namespace application
{
    using namespace _3fd::core;


    ////////////////////////////////
    // Web service implementation
    ////////////////////////////////


    /* Implements handling of received 'SendStatsSample' requests.
       Requests that fail to authenticate do not get processed, but do not fail either (no SOAP fault). */
    HRESULT CALLBACK SendStatsSample_ServerImpl(
        _In_ const WS_OPERATION_CONTEXT *wsContextHandle,
        _In_z_ WCHAR *key,
        _In_ SendStatsSampleRequest *payload,
        _Out_ BOOL *status,
        _In_ const WS_ASYNC_CONTEXT *wsAsyncContext,
        _In_ WS_ERROR *wsErrorHandle)
    {
        CALL_STACK_TRACE;

        try
        {
            /* The actual processing of the request does not happen here, but actually in the main
            thread. Here the request is received, the pair machine & key is used for authentication,
            and if all goes well, a task in enqueued in a lock-free queue for later processing. This
            way the server can provide quick servicing for all requests. */

            if (Authenticator::GetInstance().IsAuthentic(payload->machine, key))
            {
                *status = TRUE; // authenticated: accept request

                TasksQueue::GetInstance().Enqueue(
                    ExtractStatsDataFrom(*payload)
                );
            }
            else
                *status = FALSE; // NOT authenticated: reject request

            return S_OK;
        }
        catch (IAppException &ex)
        {
            Logger::Write(ex, Logger::PRIO_CRITICAL);
            wws::SetSoapFault(ex, "SendStatsSample", wsContextHandle, wsErrorHandle);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when processing service request: " << ex.what();

            AppException<std::runtime_error> appEx(oss.str());
            Logger::Write(appEx, Logger::PRIO_CRITICAL);
            wws::SetSoapFault(appEx, "SendStatsSample", wsContextHandle, wsErrorHandle);
        }

        return E_FAIL;
    }

}// end of namespace application


 /////////////////////
 // Entry Point
 /////////////////////

int main(int argc, char *argv[])
{
    using namespace application;
    using namespace _3fd::core;
    using namespace std::chrono;

    FrameworkInstance _framework;

    CALL_STACK_TRACE;

    int rc = EXIT_SUCCESS;

    try
    {
        // Before starting the service, prepare the authenticator
        Authenticator::GetInstance().LoadCredentials();

        MSDStorageWriter dbWriter(
            AppConfig::GetSettings().application.GetString("dbConnString", "NOT SET")
        );

        // Function tables contains the service implementation:
        MacStatsCollectionBindingFunctionTable funcTableSvc = {
            &application::SendStatsSample_ServerImpl,
            &application::CloseService_ServerImpl
        };

        // Create the web service host with default configurations
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

        Logger::Write("HTTP server is ready", Logger::PRIO_INFORMATION);

        std::cout << "The application will now enter the processing loop" << std::endl;

        std::vector<std::unique_ptr<StatsPackage>> tasks;

        // This is the main processing loop:
        bool running(true);
        while (running)
        {
            // Interrupt service when a request for shutdown arrives
            running = !ServiceCloser::GetInstance().WaitForCloseRequest(
                AppConfig::GetSettings().application.GetUInt("srvDbFlushCycleTimeSecs", 5) * 1000,
                host
            );

            // Retrieve the tasks enqueued in parallel by client requests
            TasksQueue::GetInstance().Dequeue(tasks);

            if (!tasks.empty())
            {
                std::cout << "Flushing to database a batch of " << tasks.size() << " package(s) of samples" << std::endl;

                // Process the tasks
                dbWriter.WriteStats(tasks);
            }

            /* Here I would place an implementation for processing the samples looking for
            a configured alert, but unfortunately I had not time for that. Sorry :( */

            /* Refresh the credentials cached in the authenticator, for when new
            credentials are included in the database while the service is up */
            Authenticator::GetInstance().LoadCredentials();
        }
    }
    catch (IAppException &ex)
    {
        rc = EXIT_FAILURE;
        std::cout << "Error! See the log for details. Application is exiting now..." << std::endl;
        Logger::Write(ex, Logger::PRIO_FATAL);
    }
    catch (std::exception &ex)
    {
        rc = EXIT_FAILURE;
        std::cout << "Error! See the log for details. Application is exiting now..." << std::endl;
        std::ostringstream oss;
        oss << "Generic failure: " << ex.what();
        Logger::Write(oss.str(), Logger::PRIO_FATAL);
    }

    ServiceCloser::Finalize();
    Authenticator::Finalize();

    return rc;
}

