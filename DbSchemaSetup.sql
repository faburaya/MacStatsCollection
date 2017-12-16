use IntranetMacStats;
go

if exists (select * from sys.tables where name = N'SvcAccessCredential')
begin
	drop table SvcAccessCredential;
end;

-- This table holds pairs "machine & key", that work as credentials to access the server:
create table SvcAccessCredential (
	machine nvarchar(50) not null primary key,
	idKey   nvarchar(30) not null
);
go

if exists (select * from sys.tables where name = N'StatsValFloat32')
begin
	drop table StatsValFloat32;
end;

/* This table holds historical data for statistics whose value has
   data type compatible with "floating point 32-bits precision" */
create table StatsValFloat32 (
	macId   smallint  not null,
	statId  smallint  not null,
	instant bigint    not null, -- time in milliseconds since 1970
	statVal float(24) not null,
	quality tinyint	  not null,

	primary key (macId, statId, instant)
);
go

if exists (select * from sys.tables where name = N'StagingStatsValFloat32')
begin
	drop index IdxStagStatsValFloat32ByBatch on StagingStatsValFloat32;
	drop table StagingStatsValFloat32;
end;

-- This table is an staging area for insertion into StatsValFloat32
create table StagingStatsValFloat32 (
	batchId  smallint     not null,
	macName  nvarchar(50) not null,
	statName nvarchar(50) not null,
	instant  bigint       not null, -- time in milliseconds since 1970
	statVal  float(24)    not null,
	quality  tinyint	  not null
);
go

create nonclustered index IdxStagStatsValFloat32ByBatch on StagingStatsValFloat32(batchId);
go

if exists (select * from sys.tables where name = N'StatsValInt32')
begin
	drop table StatsValInt32;
end;

/* This table holds historical data for statistics whose value
   has data type compatible with "signed integer 32-bits long" */
create table StatsValInt32 (
	macId   smallint  not null,
	statId  smallint  not null,
	instant bigint    not null, -- time in milliseconds since 1970
	statVal int       not null,
	quality tinyint	  not null,

	primary key (macId, statId, instant)
);
go

if exists (select * from sys.tables where name = N'StagingStatsValInt32')
begin
	drop index IdxStagStatsValInt32ByBatch on StagingStatsValInt32;
	drop table StagingStatsValInt32;
end;

-- This table is an staging area for insertion into StatsValInt32
create table StagingStatsValInt32 (
	batchId  smallint     not null,
	macName  nvarchar(50) not null,
	statName nvarchar(50) not null,
	instant  bigint       not null, -- time in milliseconds since 1970
	statVal  int          not null,
	quality  tinyint	  not null
);
go

create nonclustered index IdxStagStatsValInt32ByBatch on StagingStatsValInt32(batchId);
go

-- Normalization for machine ID and statitic ID:

if exists (select * from sys.tables where name = N'Machine')
begin
	drop index IdxMachineByName on Machine;
	drop table Machine;
end;

create table Machine (
	macId   smallint identity(1,1) primary key,
	macName nvarchar(50) not null
);
go

if exists (select * from sys.tables where name = N'Statistic')
begin
	drop index IdxStatisticByName on Statistic;
	drop table Statistic;
end;

create table Statistic (
	statId   smallint identity(1,1) primary key,
	statName nvarchar(50) not null
);
go

create nonclustered index IdxMachineByName on Machine(macName);
create nonclustered index IdxStatisticByName on Statistic(statName);
go

alter table StatsValFloat32
	add foreign key (macId)
	references Machine(macId);

alter table StatsValFloat32
	add foreign key (statId)
	references Statistic(statId);

alter table StatsValInt32
	add foreign key (macId)
	references Machine(macId);

alter table StatsValInt32
	add foreign key (statId)
	references Statistic(statId);
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