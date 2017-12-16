#ifndef __Authenticator_h__ // header guard
#define __Authenticator_h__

#include "Utilities.h"
#include <POCO\Tuple.h>
#include <POCO\Data\Session.h>
#include <shared_mutex>
#include <memory>
#include <string>
#include <vector>


namespace application
{
    using std::string;


    /// <summary>
    /// Represents a credential composed of machine & identification key.
    /// </summary>
    struct Credential
    {
        std::wstring machine;
        std::wstring idKey;

        const std::wstring &GetKey() const { return machine; }
    };

    /// <summary>
    /// Provides fast authentication of requests by keeping
    /// in cache the credentials read from database.
    /// </summary>
    class Authenticator : OdbcClient
    {
    private:

        Poco::Data::Session m_dbSession;

        std::unique_ptr<Poco::Data::Statement> m_selectAllCredentials;

        std::vector<Credential> m_cachedCredentials;

        /// <summary>
        /// Access to the cache of credentials will be controlled
        /// by this "multiple readers, single writer" lock.
        /// </summary>
        mutable std::shared_mutex m_cacheAccessSharedMutex;

        static std::unique_ptr<Authenticator> singleton;

        static std::mutex singletonCreationMutex;

        Authenticator(const string &connString);

    public:
        
        Authenticator(const Authenticator &) = delete;

        static Authenticator &GetInstance();

        static void Finalize();

        bool IsAuthentic(const wchar_t *machine, const wchar_t *idKey) const;

        void LoadCredentials();
    };

}// end of namespace application

#endif // end of header guard
