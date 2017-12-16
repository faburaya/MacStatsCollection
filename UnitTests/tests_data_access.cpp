#include "stdafx.h"
#include <3FD\runtime.h>
#include <3FD\configuration.h>
#include <3FD\callstacktracer.h>
#include "Authenticator.h"
#include "MSDStorageWriter.h"
#include <codecvt>
#include <array>

namespace unit_tests
{
    using namespace _3fd;
    using namespace _3fd::core;


    void HandleException();


    /// <summary>
    /// Tests the <see cref="application::Authenticator"/> class.
    /// </summary>
    TEST(TestCase_DataAccess, TestAuthenticator)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            application::Authenticator::GetInstance().LoadCredentials();

            // this should have been inserted on database setup:
            EXPECT_TRUE(
                application::Authenticator::GetInstance().IsAuthentic(L"dummyMachine", L"dummyIdKey")
            );

            std::wstring xMachine(L"dryCatDoesNot");
            std::wstring xIdKey(L"digInTheDesert");

            // whereas this has not:
            EXPECT_FALSE(
                application::Authenticator::GetInstance().IsAuthentic(xMachine.c_str(), xIdKey.c_str())
            );

            using namespace Poco::Data;
            using namespace Poco::Data::Keywords;

            Session dbSession("ODBC",
                AppConfig::GetSettings().application.GetString("dbConnString", "NOT SET")
            );

            dbSession << "insert into SvcAccessCredential (machine, idKey) values (?, ?);"
                , use(xMachine)
                , use(xIdKey)
                , now;

            // cache is not update, so this should still fail:
            EXPECT_FALSE(
                application::Authenticator::GetInstance().IsAuthentic(xMachine.c_str(), xIdKey.c_str())
            );

            application::Authenticator::GetInstance().LoadCredentials();

            // but now it must succeed:
            EXPECT_TRUE(
                application::Authenticator::GetInstance().IsAuthentic(xMachine.c_str(), xIdKey.c_str())
            );

            EXPECT_TRUE(
                application::Authenticator::GetInstance().IsAuthentic(L"dummyMachine", L"dummyIdKey")
            );

            dbSession << R"(
                delete from SvcAccessCredential
                    where machine = cast(? as nvarchar);
                )"
                , use(xMachine)
                , now;

            application::Authenticator::Finalize();
        }
        catch (...)
        {
            HandleException();
        }
    }


    /// <summary>
    /// Tests the <see cref="application::MSDStorageWriter"/> class.
    /// </summary>
    TEST(TestCase_DataAccess, TestMSDStorageWriter)
    {
        FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace application;

            // Generate some dummy stats data:

            auto theTime = time(nullptr) * 1000;

            std::wstring macNamePrefix(L"joeTheCrazyFrog");

            // must keep ASC order here!
            std::array<std::wstring, 2> macNames = { macNamePrefix + L'1', macNamePrefix + L'2' };

            std::wstring statFloatNamePrefix(L"dummy_stat_float_");

            // must keep ASC order here!
            std::array<std::wstring, 3> statFloatNames =
            {
                statFloatNamePrefix + L'0',
                statFloatNamePrefix + L'1',
                statFloatNamePrefix + L'2'
            };

            std::wstring statIntNamePrefix(L"dummy_stat_int_");

            // must keep ASC order here!
            std::array<std::wstring, 3> statIntNames =
            {
                statIntNamePrefix + L'0',
                statIntNamePrefix + L'1',
                statIntNamePrefix + L'2'
            };

            decltype(StorageWriteTask::statSamplesFloat32) expStatSamplesFloat;
            expStatSamplesFloat.reserve(3);
            expStatSamplesFloat.emplace_back(statFloatNames[0], 60.6F, Quality::Good);
            expStatSamplesFloat.emplace_back(statFloatNames[1], 9.0F, Quality::Invalid);
            expStatSamplesFloat.emplace_back(statFloatNames[2], 69.6F, Quality::Good);

            std::wstring statNameIntPrefix(L"dummy_stat_int_");

            decltype(StorageWriteTask::statSamplesInt32) expStatSamplesInt;
            expStatSamplesInt.reserve(3);
            expStatSamplesInt.emplace_back(statIntNames[0], 606, Quality::Good);
            expStatSamplesInt.emplace_back(statIntNames[1], 9, Quality::Unknown);
            expStatSamplesInt.emplace_back(statIntNames[2], 696, Quality::Good);

            std::vector<std::unique_ptr<StorageWriteTask>> tasks(2);

            tasks[0].reset(new StorageWriteTask());
            tasks[0]->timeSinceEpochInMillisecs = theTime;
            tasks[0]->machine = macNames[0];
            tasks[0]->statSamplesFloat32 = expStatSamplesFloat;
            tasks[0]->statSamplesInt32 = expStatSamplesInt;

            tasks[1].reset(new StorageWriteTask());
            tasks[1]->timeSinceEpochInMillisecs = theTime;
            tasks[1]->machine = macNames[1];
            tasks[1]->statSamplesFloat32 = expStatSamplesFloat;
            tasks[1]->statSamplesInt32 = expStatSamplesInt;

            // Write it to database:

            MSDStorageWriter dbWriter(
                AppConfig::GetSettings().application.GetString("dbConnString", "NOT SET")
            );

            dbWriter.WriteStats(tasks);

            // And check what was written:

            using namespace Poco::Data;
            using namespace Poco::Data::Keywords;

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            Session dbSession("ODBC",
                AppConfig::GetSettings().application.GetString("dbConnString", "NOT SET")
            );

            std::vector<int16_t> macIds;
            
            dbSession << "select macId from Machine where macName like N'%s%%' order by macName asc"
                , transcoder.to_bytes(macNamePrefix) // does not support std::wstring here
                , into(macIds)
                , now;

            ASSERT_EQ(macNames.size(), macIds.size());

            std::vector<int16_t> statFloatIds;

            dbSession << "select statId from Statistic where statName like N'%s%%' order by statName asc"
                , transcoder.to_bytes(statFloatNamePrefix) // does not support std::wstring here
                , into(statFloatIds)
                , now;

            ASSERT_EQ(statFloatNames.size(), statFloatIds.size());

            std::vector<int16_t> statIntIds;

            dbSession << "select statId from Statistic where statName like N'%s%%' order by statName asc"
                , transcoder.to_bytes(statIntNamePrefix) // does not support std::wstring here
                , into(statIntIds)
                , now;

            ASSERT_EQ(statIntNames.size(), statIntIds.size());

            float statValFloat;
            int statValInt;
            int16_t macId;
            int16_t statId;
            int8_t quality;

            // prepare the query:
            auto queryFloat = (dbSession << R"(
                select statVal
                    from StatsValFloat32
                    where instant = %Ld
                        and macId = ?
                        and statId = ?
                        and quality = ?;
                )"
                // bind by reference:
                , theTime
                , use(macId)
                , use(statId)
                , use(quality)
                , into(statValFloat)
            );

            // prepare the query:
            auto queryInt = (dbSession << R"(
                select statVal
                    from StatsValInt32
                    where instant = %Ld
                        and macId = ?
                        and statId = ?
                        and quality = ?;
                )"
                // bind by reference:
                , theTime
                , use(macId)
                , use(statId)
                , use(quality)
                , into(statValInt)
            );

            for (int idxMachine = 0; idxMachine < macNames.size(); ++idxMachine)
            {
                macId = macIds[idxMachine];

                // check stats whose value are float 32-bits type:
                for (int idxStat = 0; idxStat < statFloatNames.size(); ++idxStat)
                {
                    statId = statFloatIds[idxStat];
                    quality = static_cast<int8_t> (expStatSamplesFloat[idxStat].quality);
                    queryFloat.execute();
                    EXPECT_EQ(expStatSamplesFloat[idxStat].value, statValFloat);
                }

                // check stats whose value are integer 32-bits type:
                for (int idxStat = 0; idxStat < statIntNames.size(); ++idxStat)
                {
                    statId = statIntIds[idxStat];
                    quality = static_cast<int8_t> (expStatSamplesInt[idxStat].quality);
                    queryInt.execute();
                    EXPECT_EQ(expStatSamplesInt[idxStat].value, statValInt);
                }
            }
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace unit_tests