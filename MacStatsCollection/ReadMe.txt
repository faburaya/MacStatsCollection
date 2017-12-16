========================================================================
    STATIC LIBRARY : MacStatsCollection Project Overview
========================================================================

This static library gathers most of the code used by both client and server applications.
They were combined here to ease writing unit tests given the restricted time frame. The
client and server apps link to this library in order to use the main components for data
access and web servicing.

In the lines below I will summarize what you are going to find, but please look inside them
for more information details regarding the implementation decisions.

Authenticator.cpp
Authenticator.h

    This class connects to the database via ODBC and caches the credentials for server
    access in memory, thus providing a faster way for the server to authenticate each
    request coming from the client. All data access in the solution relies on ODBC via
    Poco C++.

CommonDataExchange.h

    Common structures used for data exchange between components.

WebService.cpp
WebService.h
MacStatsCollection.wsdl
MacStatsCollection.wsdl.c
MacStatsCollection.wsdl.h

    WSDL file defines the HTTP service (uses SOAP). Implementation is automatically generated
    by wsutil.exe tool provided in Windows SDK. This solution uses Windows Web Services API
    as foundation for its web service. Infrastructure (wrappers and helpers) come from 3FD,
    which is a framework of mine available in https://github.com/faburaya/3fd.

MSDStorageWriter.cpp
MSDStorageWriter.h

    This class gets several packages of stats that came from clients, combine them in a batch
    and bulk insert it into database. All data access in the solution relies on ODBC via Poco C++.

PerfCountersReader.cpp
PerfCountersReader.h

    This class uses Win32 PDH API to read machine stats (performance counters).

TasksQueue.cpp
TasksQueue.h

    This class is a lock-free queue that receives tasks for later processing. The many requests
    arriving from clients are enqueued, then dequeued by main thread for persistent storage
    into database.

Utilities.cpp
Utilities.h

    Common utilities reused around different parts of this solution.

/////////////////////////////////////////////////////////////////////////////

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named MacStatsCollection.pch and a precompiled types file named StdAfx.obj.

/////////////////////////////////////////////////////////////////////////////
