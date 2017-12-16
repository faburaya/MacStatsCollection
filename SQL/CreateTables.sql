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
