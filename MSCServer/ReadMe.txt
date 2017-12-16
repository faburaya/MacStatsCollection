========================================================================
    CONSOLE APPLICATION : MSCServer Project Overview
========================================================================

In the lines below I will summarize what you are going to find, but please look
inside them for more information details regarding the implementation decisions.

application.config

    This is the main XML configuration file for the server application.
    It comes from the infrastructure provided by my framework, but it
    was extended to receive some parameters exclusive to this solution.
    During build process, this file is copied to output directory and
    renamed to have the same name of the executable, plus ".3fd.config".

MSCServer.cpp

    This is the main application source file. It has the main loop for processing
    of tasks enqueued by client requests.

/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named MSCServer.pch and a precompiled types file named StdAfx.obj.

/////////////////////////////////////////////////////////////////////////////
