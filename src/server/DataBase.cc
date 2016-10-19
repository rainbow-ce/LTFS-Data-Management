#include <unistd.h>

#include <string>

#include <sqlite3.h>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "DataBase.h"

DataBase DB;

DataBase::~DataBase()

{
	if ( dbNeedsClosed)
		sqlite3_close(db);

	sqlite3_shutdown();
}

void DataBase::cleanup()

{
	unlink(Const::DB_FILE.c_str());
	unlink((Const::DB_FILE + std::string("-journal")).c_str());
}

void DataBase::open()

{
	int rc;
	std::string sql;
	std::string uri = std::string("file:") + Const::DB_FILE;

	rc = sqlite3_config(SQLITE_CONFIG_URI,1);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_initialize();

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_open_v2(uri.c_str(), &db, SQLITE_OPEN_READWRITE |
						 SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
						 SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_EXCLUSIVE, NULL);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		TRACE(Trace::error, uri);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	dbNeedsClosed = true;
}

void DataBase::createTables()

{
	std::string sql;
	sqlite3_stmt *stmt;
	int rc;

	sql = std::string("CREATE TABLE JOB_QUEUE(")
		+ std::string("OPERATION INT NOT NULL,")
		+ std::string("FILE_NAME CHAR(4096) UNIQUE PRIMARY KEY NOT NULL,")
		+ std::string("REQ_NUM INT NOT NULL,")
		+ std::string("MIGRATION_STATE INT NOT NULL,")
		+ std::string("COLOC_NUM INT NOT NULL,")
		+ std::string("FILE_SIZE INT NOT NULL,")
		+ std::string("H_PTR INT NOT NULL,")
		+ std::string("H_SIZE INT NOT NULL,")
		+ std::string("FS_ID INT NOT NULL,")
		+ std::string("I_GEN INT NOT NULL,")
		+ std::string("I_NUM INT NOT NULL);")
		+ std::string("MTIME INT NOT NULL);")
		+ std::string("LAST_UPD INT NOT NULL);");

	rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);

	if( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_step(stmt);

	if ( rc != SQLITE_DONE ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}

	rc = sqlite3_finalize(stmt);

	if ( rc != SQLITE_OK ) {
		TRACE(Trace::error, rc);
		throw(LTFSDMErr::LTFSDM_GENERAL_ERROR);
	}
}
