use IntranetMacStats;
go

begin transaction;
	delete from Machine;

	insert into Machine (macName) values (N'dummyMachine');

	delete from SvcAccessCredential;

	-- this is necessary for unit tests:
	insert into SvcAccessCredential (machine, idKey) values (N'dummyMachine', N'dummyIdKey'); 

	-- this is necessary for integration tests on my machine:
	insert into SvcAccessCredential (machine, idKey) values (N'CASE', N'Entschuldigung');

	-- this is necessary for integration tests on YOUR machine:
	--insert into SvcAccessCredential (machine, idKey) values (N'YOUR_PC_NAME_RUNNING_UNIT_TESTS', N'Entschuldigung');
commit transaction;

select * from SvcAccessCredential;

select * from Machine;

select * from Statistic;

-- Tests InsertIntoStatsFloat32Proc:

begin transaction;
	declare @time bigint;
	set @time = datediff_big(MILLISECOND, cast('1970-01-01 00:00:00' as datetime), SYSDATETIME());

	insert into StagingStatsValFloat32 (batchId, macName, statName, instant, statVal, quality)
		values (1, N'HAL9000', N'cpu_usage_percentage', @time, 21.6, 0);

	set @time = @time + 400;
	insert into StagingStatsValFloat32 (batchId, macName, statName, instant, statVal, quality)
		values (1, N'HAL9000', N'cpu_usage_percentage', @time, 58.7, 0);

	set @time = @time + 400;
	insert into StagingStatsValFloat32 (batchId, macName, statName, instant, statVal, quality)
		values (1, N'HAL9000', N'cpu_usage_percentage', @time, 2.94, 0);

	exec InsertIntoStatsFloat32Proc 1;
commit transaction;

select * from StatsValFloat32;

-- Show historic data inserted:
select mach.macName
      ,stat.statName
      ,dateadd(SECOND, instant / 1000,
               dateadd(MILLISECOND, instant % 1000, cast('1970-01-01' as datetime))) as [time]
      ,statVal
      ,quality
    from StatsValFloat32 dat
        inner join Machine mach
            on dat.macId = mach.macId
        inner join Statistic stat
            on dat.statId = stat.statId
    order by instant;

-- Tests InsertIntoStatsInt32Proc:

begin transaction;
	declare @time bigint;
	set @time = datediff_big(MILLISECOND, cast('1970-01-01 00:00:00' as datetime), SYSDATETIME());

	insert into StagingStatsValInt32 (batchId, macName, statName, instant, statVal, quality)
		values (2, N'HAL9000', N'process_count', @time, 459, 0);

	set @time = @time + 400;
	insert into StagingStatsValInt32 (batchId, macName, statName, instant, statVal, quality)
		values (2, N'HAL9000', N'process_count', @time, 123, 0);

	set @time = @time + 400;
	insert into StagingStatsValInt32 (batchId, macName, statName, instant, statVal, quality)
		values (2, N'HAL9000', N'process_count', @time, 696, 0);

	exec InsertIntoStatsInt32Proc 2;
commit transaction;

select * from StatsValInt32 order by instant;

-- Show historic data inserted:
select mach.macName
      ,stat.statName
      ,dateadd(SECOND, instant / 1000,
               dateadd(MILLISECOND, instant % 1000, cast('1970-01-01' as datetime))) as [time]
      ,statVal
      ,quality
    from StatsValInt32 dat
        inner join Machine mach
            on dat.macId = mach.macId
        inner join Statistic stat
            on dat.statId = stat.statId
    order by instant;
