use IntranetMacStats;
go

/* These stored procedures ensure consistency of inserted data
   across all the tables in database, plus adds some level of
   independence from the tables design. */

if object_id(N'InsertIntoStatsFloat32Proc', N'P') is not null
begin
	drop procedure InsertIntoStatsFloat32Proc;
end;
go

/* Because ODBC implementation on the application side cannot reliably/efficiently
   pass parameters to issue massive calls to stored procedures (that would lead to
   several isolated calls leading to a growing overhead of data round-trips), the
   strategy adopted here is to bulk-insert the parameters for all calls at once into
   a "staging table", then calling this procedure that process all those calls. */

create procedure InsertIntoStatsFloat32Proc (@batchId smallint) as
begin

	declare @timeSinceEpochInMillisecs bigint;
	declare @macName nvarchar(50);
	declare @statName nvarchar(50);
	declare @statValue float(24);
	declare @quality tinyint;

	declare stagingCursor cursor for (
		select instant
              ,macName
			  ,statName
			  ,statVal
			  ,quality
			from StagingStatsValFloat32
			where batchId = @batchId
	);

	open stagingCursor;
	fetch next from stagingCursor
		into @timeSinceEpochInMillisecs
			,@macName
			,@statName
			,@statValue
			,@quality;

	while @@FETCH_STATUS = 0  
	begin
		-- ensure consistency regarding machine:

		declare @macId smallint;
		set @macId = (select macId from Machine where macName = @macName);

		if @macId is null
		begin
			insert into Machine (macName) values (@macName);
			set @macId = (select macId from Machine where macName = @macName);
		end;

		-- ensure consistency regarding statistic:

		declare @statId smallint;
		set @statId = (select statId from Statistic where statName = @statName);
	
		if @statId is null
		begin
			insert into Statistic (statName) values (@statName);
			set @statId = (select statId from Statistic where statName = @statName);
		end;

		-- finally insert data:
		insert into StatsValFloat32 (
			macId,
			statId,
			instant,
			statVal,
			quality
		)
		values (
			@macId,
			@statId,
			@timeSinceEpochInMillisecs,
			@statValue,
			@quality
		);

		fetch next from stagingCursor
		into @timeSinceEpochInMillisecs
			,@macName
			,@statName
			,@statValue
			,@quality;

	end; -- end of loop

	close stagingCursor;
	deallocate stagingCursor;

	delete from StagingStatsValFloat32
		where batchId = @batchId;

end; -- end of stored procedure
go

if object_id(N'InsertIntoStatsInt32Proc', N'P') is not null
begin
	drop procedure InsertIntoStatsInt32Proc;
end;
go

create procedure InsertIntoStatsInt32Proc (@batchId smallint) as
begin

	declare @timeSinceEpochInMillisecs bigint;
	declare @macName nvarchar(50);
	declare @statName nvarchar(50);
	declare @statValue int;
	declare @quality tinyint;

	declare stagingCursor cursor for (
		select instant
              ,macName
			  ,statName
			  ,statVal
			  ,quality
			from StagingStatsValInt32
			where batchId = @batchId
	);

	open stagingCursor;
	fetch next from stagingCursor
		into @timeSinceEpochInMillisecs
			,@macName
			,@statName
			,@statValue
			,@quality;

	while @@FETCH_STATUS = 0  
	begin
		-- ensure consistency regarding machine:

		declare @macId smallint;
		set @macId = (select macId from Machine where macName = @macName);

		if @macId is null
		begin
			insert into Machine (macName) values (@macName);
			set @macId = (select macId from Machine where macName = @macName);
		end;

		-- ensure consistency regarding statistic:

		declare @statId smallint;
		set @statId = (select statId from Statistic where statName = @statName);
	
		if @statId is null
		begin
			insert into Statistic (statName) values (@statName);
			set @statId = (select statId from Statistic where statName = @statName);
		end;

		-- finally insert data:
		insert into StatsValInt32 (
			macId,
			statId,
			instant,
			statVal,
			quality
		)
		values (
			@macId,
			@statId,
			@timeSinceEpochInMillisecs,
			@statValue,
			@quality
		);

		fetch next from stagingCursor
		into @timeSinceEpochInMillisecs
			,@macName
			,@statName
			,@statValue
			,@quality;

	end; -- end of loop

	close stagingCursor;
	deallocate stagingCursor;

	delete from StagingStatsValInt32
		where batchId = @batchId;

end; -- end of stored procedure
go