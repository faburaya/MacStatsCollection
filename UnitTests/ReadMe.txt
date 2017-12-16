========================================================================
    CONSOLE APPLICATION : UnitTests Project Overview
========================================================================

application.config

    This is the main XML configuration file for both client and server
    applications. Because the tests play the role of both inside a same
    process, all of them follow the same layout.
    It comes from the infrastructure provided by my framework, but it
    was extended to receive some parameters exclusive to this solution.
    During build process, this file is copied to output directory and
    renamed to have the same name of the executable, plus ".3fd.config".

tests_data_access.cpp

    Tests for data access components. They are the Authenticator and
    MSDStorageWriter classes. All data access in the solution rellies on
    ODBC via Poco C++. It is no different here.

tests_stats_reader.cpp

    Tests the collection of machine stats (performance counters) by
    class PerfCoutersReader.

tests_web_service.cpp

    Tests transformation of data from data access to web service and back,
    including whether SOAP serialization is correct, or whether infrastructure
    for setup and closing of services is all right.

UnitTests.cpp

    This is the main application source file. The entry point just invokes
    Google test framework that then drives the implemented tests.

/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named UnitTests.pch and a precompiled types file named StdAfx.obj.

/////////////////////////////////////////////////////////////////////////////
