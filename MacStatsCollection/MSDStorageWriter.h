#ifndef __MSDStorageWriter_h__ // header guard
#define __MSDStorageWriter_h__

#include "Utilities.h"
#include "CommonDataExchange.h"
#include <Poco/Data/Session.h>
#include <vector>
#include <string>
#include <memory>

namespace application
{
    using std::string;


    /// <summary>
    /// Base class for rows to bulk-insert.
    /// </summary>
    struct RowStatBase
    {
        int64_t instant; // time in milliseconds past epoch (1970-01-01)
        std::wstring macName;
        std::wstring statName;
        int16_t batchId;
        int8_t quality;
    };


    /// <summary>
    /// Packages all data to insert into a table of samples.
    /// </summary>
    template <typename ValType>
    struct RowStat : public RowStatBase
    {
        ValType statVal;
    };


    /// <summary>
    /// Commits to database the samples of machine stats.
    /// </summary>
    /// <seealso cref="OdbcClient" />
    /// <seealso cref="notcopiable" />
    class MSDStorageWriter : OdbcClient
    {
    private:

        // Query placeholders will bind to the members of this object
        std::vector<RowStat<float>> m_rowsFloat32DataBind;

        // Query placeholders will bind to the members of this object
        std::vector<RowStat<int>> m_rowsInt32DataBind;

        Poco::Data::Session m_dbSession;

    public:

        MSDStorageWriter(const string &connString);

        MSDStorageWriter(const MSDStorageWriter &) = delete;

        void WriteStats(std::vector<std::unique_ptr<StorageWriteTask>> &tasks);
    };

}// end of namespace application

#endif // end of header guard
