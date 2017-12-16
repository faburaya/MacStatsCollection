========================================================================
                 MacStatsCollection Solution Overview
========================================================================

This solution was developed and tested using Microsoft Visual Studio 2015
Update 3 Community Edition, running on top of a Windows 10 Pro x64 machine,
build 1607.14393 (annivesary update). Only x64 binaries have been tested.

The solution depends on 3FD v2.4+ (installed in a location defined by an
enviroment variable to be called "_3FD_HOME"), Boost and POCO C++. All of
them can be installed by building 3FD (https://github.com/faburaya/3fd).

After that, it should build fine. Google test framework source code in
included and dependencies are properly set already.

Please refer to the specific Readme.txt files inside each of the projects that
compose this solution. All the source is based on 3FD, which is a framework of
mine that I have been developing over the years to prevent me from rewriting
boiler plate code, but it also has several other things, like the helpers and
wrappers that I employ to quickly create services with SOAP over HTTP transport.

Client, Server, and tests are separated in dedicated directories, but most of
the source is concentrated in MacStatsCollection static library, from which
all of them import classes.


========================================================================
                            Database Setup
========================================================================

I am using Microsoft SQL Server 2016 Express. Previous versions are compatible,
but not without errors during database creation. It should also work with
Microsoft SQL Server Local DB, but that would also require changes in the
script for database creation. Please use 2016 express just to be safe.

In the solution root directory you will find the scripts for database setup.
The steps to create the database environment are:

1) Edit CreateDatabase.sql to use a valid path to place the database storage
file (at line 4 and 6). Then run the script.

2) Run DbSchemaSetup.sql. Whenever you want to reset the schema (erasing all
data in tables), just run it again.

For the sake of simplicity of deployment, connect to SQL Server using integrated
security (log on with OS user), and make sure you run the demo and tests
elevating the processes with administrator privilege.


========================================================================
                           Environment Setup
========================================================================

Make sure you run everything as administrator, as doing otherwise might cause
"access denied" errors when the executables try to create the web server.

Running the executables:

Build in VC++2015 using the IDE, which is pretty straightforward. In the
output directory, along with the executable UnitTests.exe, you will also
find the XML configuration file UnitTests.exe.3fd.config. I will summarize
here what you will have to edit:

(Just under configuration node)

<application>
    <!-- This is used by all executables. It contains the
         connection string ODBC uses to connect to MSSQL -->
    <entry key="dbConnString"
           value"Driver={SQL Server Native Client 11.0};Server=CASE\SQLEXPRESS;Database=IntranetMacStats;Trusted_Connection=yes;"/>

    <!-- This is used by the server application. It sets how often (in seconds) the server must
         dequeue tasks enqueued by client requests, process them and persist in database. -->
    <entry key="srvDbFlushCycleTimeSecs" value="10"/>
    
    <!-- ATTENTION! This is used by client application. It sets
         the endpoint of the server. DO NOT USE "localhost". -->
    <entry key="webSvcHostEndpoint" value="http://CASE:81/macstatscollection"/>

    <!-- This is used by client application. It is the key used for authentication
         that requests issued by the client will carry to the server. The value here
         must be registered as a credential in the SvcAccessCredential table. -->
    <entry key="webClientAuthKey" value="Entschuldigung"/>
</application>


This should be all :)