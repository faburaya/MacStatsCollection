#include "stdafx.h"
#include "Authenticator.h"
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>
#include <3FD\logger.h>
#include <3FD\configuration.h>
#include <3FD\utils_algorithms.h>
#include <codecvt>
#include <sstream>

namespace Poco {
namespace Data {

    template <>
    class TypeHandler<application::Credential>
    {
    public:

        static void bind(std::size_t pos, const application::Credential &obj, AbstractBinder::Ptr pBinder, AbstractBinder::Direction dir)
        {
            poco_assert_dbg(!pBinder.isNull());
            TypeHandler<std::wstring>::bind(pos++, obj.machine, pBinder, dir);
            TypeHandler<std::wstring>::bind(pos++, obj.idKey, pBinder, dir);
        }

        static std::size_t size() { return 2; }

        static void prepare(std::size_t pos, application::Credential &obj, AbstractPreparator::Ptr pPrepare)
        {
            poco_assert_dbg(!pPrepare.isNull());
            TypeHandler<std::wstring>::prepare(pos++, obj.machine, pPrepare);
            TypeHandler<std::wstring>::prepare(pos++, obj.idKey, pPrepare);
        }

        static void extract(std::size_t pos, application::Credential &obj, const application::Credential &defVal, AbstractExtractor::Ptr pExt)
        {
            poco_assert_dbg(!pExt.isNull());

            std::wstring machine, idKey;

            TypeHandler<std::wstring>::extract(pos++, machine, defVal.machine, pExt);
            TypeHandler<std::wstring>::extract(pos++, idKey, defVal.idKey, pExt);

            obj.machine = std::move(machine);
            obj.idKey = std::move(idKey);
        }

    private:

        TypeHandler() {}
        ~TypeHandler() {}

        TypeHandler(const TypeHandler &) {}
        TypeHandler &operator=(const TypeHandler &) {}
    };

}// end of namespace Data
}// end of namespace Poco

namespace application
{
    using namespace _3fd;
    using namespace _3fd::core;


    std::unique_ptr<Authenticator> Authenticator::singleton;

    std::mutex Authenticator::singletonCreationMutex;


    /// <summary>
    /// Provides access to the singleton.
    /// </summary>
    /// <returns>A reference to the singleton.</returns>
    Authenticator & Authenticator::GetInstance()
    {
        if (singleton)
            return *singleton;

        CALL_STACK_TRACE;

        try
        {
            std::lock_guard<std::mutex> lock(singletonCreationMutex);

            singleton.reset(
                // use connection string defined in the main configuration file
                new Authenticator(
                    AppConfig::GetSettings().application.GetString("dbConnString", "NOT SET")
                )
            );

            return *singleton;
        }
        catch (IAppException &)
        {
            throw; // just forward already prepared application exceptions
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when instantiating authenticator: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="Authenticator"/> class.
    /// </summary>
    /// <param name="connString">The database connection string.</param>
    Authenticator::Authenticator(const std::string &connString)
    try :
        m_dbSession("ODBC", connString),
        m_cacheAccessSharedMutex()
    {
        CALL_STACK_TRACE;

        m_selectAllCredentials.reset(new Poco::Data::Statement(m_dbSession));

        Logger::Write(
            "Authenticator has succesfully connected to database via ODBC",
            connString,
            Logger::PRIO_INFORMATION
        );

        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        /* Please notice that selecting like this works great as long as the amount
        of machines is not too big. For some hundreds of machine we are okay, though. */

        // for now, just prepare the query:
        *m_selectAllCredentials
             << "select machine, idKey "
                "from SvcAccessCredential "
                "order by machine asc;"
                ,into(m_cachedCredentials);
    }
    catch (Poco::Data::DataException &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create authenticator. POCO C++ reported a data access error: " << ex.name();
        throw AppException<std::runtime_error>(oss.str(), ex.message());
    }
    catch (Poco::Exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create authenticator. POCO C++ reported a generic error - " << ex.name();

        if (!ex.message().empty())
            oss << ": " << ex.message();

        throw AppException<std::runtime_error>(oss.str());
    }
    catch (std::system_error &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "System error prevented creation of authenticator: " << StdLibExt::GetDetailsFromSystemError(ex);
        throw AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure prevented creation of authenticator: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }


    /// <summary>
    /// Finalizes the singleton.
    /// </summary>
    void Authenticator::Finalize()
    {
        singleton.reset(nullptr);
    }


    /// <summary>
    /// Determines whether the given credential is authentic.
    /// </summary>
    /// <param name="machine">The machine ID.</param>
    /// <param name="idKey">The verification key.</param>
    /// <returns>
    ///   <c>true</c> if the given credential is authentic, otherwise, <c>false</c>.
    /// </returns>
    bool Authenticator::IsAuthentic(const wchar_t *machine, const wchar_t *idKey) const
    {
        CALL_STACK_TRACE;

        try
        {
            // Shared lock for read access
            std::shared_lock<std::shared_mutex> lock(m_cacheAccessSharedMutex);

            auto iter = utils::BinarySearch(
                std::wstring(machine),
                m_cachedCredentials.begin(),
                m_cachedCredentials.end()
            );

            return (m_cachedCredentials.end() != iter);
        }
        catch (std::system_error &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "System error during authentication: " << StdLibExt::GetDetailsFromSystemError(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Generic failure prevented authentication: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }


    /// <summary>
    /// Read all credentials from database and load them into cache.
    /// </summary>
    void Authenticator::LoadCredentials()
    {
        CALL_STACK_TRACE;

        try
        {
            // Exclusive lock for write access
            std::unique_lock<std::shared_mutex> lock(m_cacheAccessSharedMutex);

            m_cachedCredentials.clear(); // clear the cache

            if (!m_dbSession.isConnected())
                m_dbSession.reconnect();

            m_selectAllCredentials->execute(); // load
        }
        catch (Poco::Data::DataException &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Failed to load credentials into authenticator cache. "
                   "POCO C++ reported a data access error: " << ex.name();

            throw AppException<std::runtime_error>(oss.str(), ex.message());
        }
        catch (Poco::Exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Failed to load credentials into authenticator cache. "
                   "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::system_error &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "System error prevented loading of credentials into authenticator cache: "
                << StdLibExt::GetDetailsFromSystemError(ex);

            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Generic failure prevented loading of credentials into authenticator cache: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace application
