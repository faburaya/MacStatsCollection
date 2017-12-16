// MSCClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <3FD\runtime.h>
#include <3FD\configuration.h>
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>
#include <3FD\logger.h>
#include <3FD\cmdline.h>
#include <iostream>
#include <iomanip>


namespace application
{
    using namespace _3fd::core;

    //////////////////////////////
    // Command line arguments
    //////////////////////////////

    struct CmdLineParams
    {
        uint32_t collectCycleTimeSecs;
        uint32_t expirationInSecs;
        bool shutdownServer;
        bool showHelp;
    };

    // Parse arguments from command line
    static bool ParseCommandLineArgs(int argc, const char *argv[], CmdLineParams &params)
    {
        CALL_STACK_TRACE;

        CommandLineArguments cmdLineArgs(80, CommandLineArguments::ArgValSeparator::Colon, true, false);

        enum { ArgValCollectCycleTime, ArgValExpiration, ArgOptShutdownServer, ArgOptShowHelp };

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValCollectCycleTime,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::RangeInteger,
            't', "cycle",
            "The amount of time (in seconds) the cycle must wait between collections"
        }, { 300LL, 2LL, 86400LL });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgValExpiration,
            CommandLineArguments::ArgType::OptionWithReqValue,
            CommandLineArguments::ArgValType::RangeInteger,
            'x', "expires",
            "The expiration time (in seconds) after which this client quits the collection time and closes"
        }, { 0LL, 0LL, 999999999LL });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgOptShutdownServer,
            CommandLineArguments::ArgType::OptionSwitch,
            CommandLineArguments::ArgValType::None,
            0, "shutdown",
            "Issues a request to shutdown the server"
        });

        cmdLineArgs.AddExpectedArgument(CommandLineArguments::ArgDeclaration{
            ArgOptShowHelp,
            CommandLineArguments::ArgType::OptionSwitch,
            CommandLineArguments::ArgValType::None,
            'h', "help",
            "Displays this help"
        });

        const char *usageMessage("\nUsage:\n\n MSCClient [/shutdown] [/t:collection_cycle_time_in_seconds] [/x:expiration_time_in_seconds]\n\n");

        if (cmdLineArgs.Parse(argc, argv) == STATUS_FAIL)
        {
            std::cerr << usageMessage;
            cmdLineArgs.PrintArgsInfo();
            return STATUS_FAIL;
        }

        params.showHelp = cmdLineArgs.GetArgSwitchOptionValue(ArgOptShowHelp);

        if (params.showHelp)
        {
            std::cerr << usageMessage;
            cmdLineArgs.PrintArgsInfo();
        }

        params.shutdownServer = cmdLineArgs.GetArgSwitchOptionValue(ArgOptShutdownServer);

        if (params.shutdownServer)
        {
            std::cout << "This client will request shutdown of server" << std::endl;
            return STATUS_OKAY;
        }

        bool isPresent;
        auto paramValCCT = cmdLineArgs.GetArgValueInteger(ArgValCollectCycleTime, isPresent);
        params.collectCycleTimeSecs = static_cast<uint32_t> (paramValCCT);

        std::cout << "Collection cycle interval will be " << params.collectCycleTimeSecs
                  << (isPresent ? " seconds \n" : " seconds (default)\n");

        auto paramValExpiration = cmdLineArgs.GetArgValueInteger(ArgValExpiration, isPresent);
        params.expirationInSecs = static_cast<uint32_t> (paramValExpiration);

        if (isPresent && params.expirationInSecs > 0)
            std::cout << "This client will automatically quit after " << params.expirationInSecs << " seconds";
        else
            std::cout << "This client will NOT quit automatically";

        std::cout << std::endl;

        return STATUS_OKAY;
    }

}// end of namespace application


#include "WebService.h"
#include "PerfCountersReader.h"
#include <thread>
#include <chrono>
#include <codecvt>


/////////////////////
// Entry Point
/////////////////////

int main(int argc, const char *argv[])
{
    using namespace application;
    using namespace _3fd::core;
    using namespace std::chrono;

    FrameworkInstance _framework;

    CALL_STACK_TRACE;

    try
    {
        CmdLineParams params;
        if (ParseCommandLineArgs(argc, argv, params) == STATUS_FAIL)
            return EXIT_FAILURE;

        // Create the HTTP client:
        wws::SvcProxyConfig proxyCfg; // use standard configuration
        application::MacStatsCollectionClient client(proxyCfg);
        client.Open();

        Logger::Write("HTTP client is ready", Logger::PRIO_INFORMATION);

        if (params.shutdownServer)
        {
            if (client.CloseService())
                std::cout << "The server accepted the shutdown order!" << std::endl;
            else
                std::cout << "The server DID NOT accept the shutdown order!" << std::endl;

            return EXIT_SUCCESS;
        }

        // Retrieve the key for authentication from the XML configuration file
        auto authKey = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(
            AppConfig::GetSettings().application.GetString("webClientAuthKey", "NOT SET")
        );

        // Setup performance counters reader
        PerfCountersReader statsReader;

        // Collection cycle:
        do
        {
            auto t1 = system_clock().now();

            auto statsNow = statsReader.GetCurrentValues();

            client.SendStatsSample(
                authKey.c_str(),
                statsNow
            );

            auto t2 = system_clock().now();

            auto remainingTime = seconds(params.collectCycleTimeSecs) - (t2 - t1);
            if (remainingTime.count() > 0)
                std::this_thread::sleep_for(remainingTime);

        } while (params.expirationInSecs <= 0
                 || params.expirationInSecs >= clock() / CLOCKS_PER_SEC);

        std::cout << "Application running time has expired. Exiting now..." << std::endl;
    }
    catch (IAppException &ex)
    {
        std::cout << "Error! See the log for details. Application is exiting now..." << std::endl;
        Logger::Write(ex, Logger::PRIO_FATAL);
        return EXIT_FAILURE;
    }
    catch (std::exception &ex)
    {
        std::cout << "Error! See the log for details. Application is exiting now..." << std::endl;
        std::ostringstream oss;
        oss << "Generic failure: " << ex.what();
        Logger::Write(oss.str(), Logger::PRIO_FATAL);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
