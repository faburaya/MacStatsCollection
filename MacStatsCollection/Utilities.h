#ifndef __Utilities_h__ // header guard
#define __Utilities_h__

#include <cinttypes>

namespace application
{
    const wchar_t *GetLocalHostName();

    int16_t GenerateBatchId();

    /// <summary>
    /// Implementation needs this base class upon queue reader/writer
    /// initialization before using POCO Data ODBC connector.
    /// </summary>
    class OdbcClient
    {
    public:

        OdbcClient();
    };

}

#endif // end of header guard
