// UnitTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <3FD\exceptions.h>
#include <3FD\logger.h>
#include <POCO\Exception.h>
#include <iostream>
//#include <vld.h>


namespace unit_tests
{
    using namespace _3fd::core;


    /// <summary>
    /// Handles several types of exception.
    /// </summary>
    void HandleException()
    {
        try
        {
            throw;
        }
        catch (IAppException &appEx)
        {
            //std::cerr << appEx.ToString() << std::endl;
            Logger::Write(appEx, Logger::PRIO_ERROR);
        }
        catch (Poco::Exception &ex)
        {
            std::cerr << ex.name() << ": " << ex.message();
        }
        catch (std::exception &stdEx)
        {
            std::cerr << stdEx.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "An unexpected exception has been caught." << std::endl;
        }

        FAIL();
    }
}// end of namespace unit_tests


int main(int argc, char *argv[])
{
    std::cout << "Running main() from UnitTests.cpp\n";
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
