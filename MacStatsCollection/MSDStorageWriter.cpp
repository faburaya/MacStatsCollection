#include "stdafx.h"
#include "MSDStorageWriter.h"
#include <3FD\callstacktracer.h>
#include <3FD\exceptions.h>
#include <3FD\logger.h>

namespace Poco {
namespace Data {

    using namespace application;

    /// <summary>
    /// A type handler for a given type allows Poco::Data
    /// to attempt bulk operations on complex types.
    /// </summary>
    template <typename ValType>
    class TypeHandler<RowStat<ValType>>
    {
    public:

        static size_t size() { return 6; }

        static void bind(size_t pos, const RowStat<ValType> &obj, AbstractBinder::Ptr pBinder, AbstractBinder::Direction dir)
        {
            poco_assert_dbg(!pBinder.isNull());

            TypeHandler<int16_t>::bind(pos++, obj.batchId, pBinder, dir);
            TypeHandler<std::wstring>::bind(pos++, obj.macName, pBinder, dir);
            TypeHandler<std::wstring>::bind(pos++, obj.statName, pBinder, dir);
            TypeHandler<int64_t>::bind(pos++, obj.instant, pBinder, dir);
            TypeHandler<ValType>::bind(pos++, obj.statVal, pBinder, dir);
            TypeHandler<int8_t>::bind(pos++, obj.quality, pBinder, dir);
        }

        static void prepare(size_t pos, RowStat<ValType> &obj, AbstractPreparator::Ptr pPrepare)
        {
            poco_assert_dbg(!pPrepare.isNull());

            TypeHandler<int16_t>::prepare(pos++, obj.batchId, pPrepare);
            TypeHandler<std::wstring>::prepare(pos++, obj.macName, pPrepare);
            TypeHandler<std::wstring>::prepare(pos++, obj.statName, pPrepare);
            TypeHandler<int64_t>::prepare(pos++, obj.instant, pPrepare);
            TypeHandler<ValType>::prepare(pos++, obj.statVal, pPrepare);
            TypeHandler<int8_t>::prepare(pos++, obj.quality, pPrepare);
        }

        static void extract(size_t pos, RowStat<ValType> &obj, const RowStat<ValType> &defVal, AbstractExtractor::Ptr pExt)
        {
            poco_assert_dbg(!pExt.isNull());

            int16_t batchId;
            std::wstring macName, statName;
            int64_t instant;
            ValType statVal;
            int8_t quality;

            TypeHandler<int16_t>::extract(pos++, batchId, defVal.batchId, pExt);
            TypeHandler<std::wstring>::extract(pos++, macName, defVal.macName, pExt);
            TypeHandler<std::wstring>::extract(pos++, statName, defVal.statName, pExt);
            TypeHandler<int64_t>::extract(pos++, instant, defVal.instant, pExt);
            TypeHandler<ValType>::extract(pos++, statVal, defVal.statVal, pExt);
            TypeHandler<int8_t>::extract(pos++, quality, defVal.quality, pExt);

            obj.batchId = batchId;
            obj.macName = std::move(macName);
            obj.statName = std::move(statName);
            obj.instant = instant;
            obj.statVal = statVal;
            obj.quality = quality;
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
    using namespace _3fd::core;


    /// <summary>
    /// Initializes a new instance of the <see cref="MSDStorageWriter"/> class.
    /// </summary>
    /// <param name="connString">The backend connection string.</param>
    MSDStorageWriter::MSDStorageWriter(const string &connString)
    try :
        m_dbSession("ODBC", connString)
    {
        CALL_STACK_TRACE;

        Logger::Write(
            "Stats data writer has successfully connected to storage database via ODBC",
            connString,
            Logger::PRIO_INFORMATION
        );

        m_dbSession.setFeature("autoCommit", false);
    }
    catch (Poco::Data::DataException &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create database writer. POCO C++ reported a data access error: " << ex.name();
        throw AppException<std::runtime_error>(oss.str(), ex.message());
    }
    catch (Poco::Exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create database writer. POCO C++ reported a generic error - " << ex.name();

        if (!ex.message().empty())
            oss << ": " << ex.message();

        throw AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure prevented creation of database writer: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }


    /// <summary>
    /// Gathers several tasks of database writing carrying packages
    /// of "machine stats" samples, combines them in a few batches
    /// then bulk insert them into database.
    /// </summary>
    /// <param name="tasks">The database writing tasks.</param>
    void MSDStorageWriter::WriteStats(std::vector<std::unique_ptr<StorageWriteTask>> &tasks)
    {
        if (tasks.empty())
            return;

        CALL_STACK_TRACE;

        try
        {
            if (!m_dbSession.isConnected())
                m_dbSession.reconnect();

            m_rowsInt32DataBind.clear();
            m_rowsFloat32DataBind.clear();

            auto batchIdInt32 = GenerateBatchId();
            auto batchIdFloat32 = GenerateBatchId();
            
            // Combine the data of all tasks in a single batch:
            for (auto &task : tasks)
            {
                // Prepares the rows with samples float32 for insertion:
                for (auto &sample : task->statSamplesFloat32)
                {
                    m_rowsFloat32DataBind.emplace_back();
                    auto &row = m_rowsFloat32DataBind.back();
                    
                    row.batchId = batchIdFloat32;
                    row.instant = task->timeSinceEpochInMillisecs;
                    row.macName = task->machine;
                    row.statName = std::move(sample.statName);
                    row.statVal = sample.value;
                    row.quality = static_cast<int8_t> (sample.quality);
                }

                // Prepares the rows with samples int32 for insertion:
                for (auto &sample : task->statSamplesInt32)
                {
                    m_rowsInt32DataBind.emplace_back();
                    auto &row = m_rowsInt32DataBind.back();

                    row.batchId = batchIdInt32;
                    row.instant = task->timeSinceEpochInMillisecs;
                    row.macName = task->machine;
                    row.statName = std::move(sample.statName);
                    row.statVal = sample.value;
                    row.quality = static_cast<int8_t> (sample.quality);
                }
            }

            tasks.clear(); // release some memory

            /* The tables actually holding historical data belong to a normalized data model, where
            foreign keys refer to the machine and stats names in other tables. That collaborates to
            more efficient use of storage, but then insertion implies in previous conversion of names
            to ID's, which is done more efficiently in the database side via stored procedures.
            Because ODBC implementation on POCO cannot reliably/efficiently pass parameters to issue
            massive calls to stored procedures (that would lead to several isolated calls leading to a
            growing overhead of data round-trips), the strategy adopted here is to attempt bulk-insertion
            of parameters for all calls at once into "staging tables". Once in the database, one call to
            the stored procedures will read all that data already there and process it. */

            using namespace Poco::Data;
            using namespace Poco::Data::Keywords;

            // begin transaction
            m_dbSession.begin();

            m_dbSession << R"(
	            insert into StagingStatsValInt32 (batchId, macName, statName, instant, statVal, quality)
	                values (?, ?, ?, ?, ?, ?);
                )"
                , use(m_rowsInt32DataBind)
                , now;

            m_dbSession << R"(
	            insert into StagingStatsValFloat32 (batchId, macName, statName, instant, statVal, quality)
	                values (?, ?, ?, ?, ?, ?);
                )"
                , use(m_rowsFloat32DataBind)
                , now;

            m_dbSession << "exec InsertIntoStatsFloat32Proc %hd;", batchIdFloat32, now;

            m_dbSession << "exec InsertIntoStatsInt32Proc %hd;", batchIdInt32, now;

            // commit transaction
            m_dbSession.commit();
        }
        catch (Poco::Data::DataException &ex)
        {
            if (m_dbSession.isConnected() && m_dbSession.isTransaction())
                m_dbSession.rollback();

            std::ostringstream oss;
            oss << "Failed to write samples into tables of historic data. "
                   "POCO C++ reported a data access error: " << ex.name();

            throw AppException<std::runtime_error>(oss.str(), ex.message());
        }
        catch (Poco::Exception &ex)
        {
            if (m_dbSession.isConnected() && m_dbSession.isTransaction())
                m_dbSession.rollback();

            std::ostringstream oss;
            oss << "Failed to write samples into tables of historic data. "
                   "POCO C++ reported a generic error - " << ex.name();

            if (!ex.message().empty())
                oss << ": " << ex.message();

            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            if (m_dbSession.isConnected() && m_dbSession.isTransaction())
                m_dbSession.rollback();

            std::ostringstream oss;
            oss << "Generic failure prevented writing samples into tables of historic data: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace application
