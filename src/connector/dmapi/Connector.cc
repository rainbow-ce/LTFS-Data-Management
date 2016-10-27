#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <atomic>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include <xfs/dmapi.h>

#include "src/connector/Connector.h"

std::atomic<dm_sessid_t> dmapiSession;
std::atomic<dm_token_t> dmapiToken;

void dmapiSessionCleanup()

{
	int i;
	unsigned int j;
	unsigned int num_sessions = 0;
	unsigned int num_sessions_res = 0;
	dm_sessid_t *sidbufp = NULL;
    unsigned long rseslenp;
	unsigned int rtoklenp;
    char buffer[DM_SESSION_INFO_LEN];

	sidbufp = (dm_sessid_t*)malloc(sizeof( dm_sessid_t ));

	if ( sidbufp == NULL ) {
		MSG(LTFSDMD0001E);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	while ( dm_getall_sessions(num_sessions, sidbufp, &num_sessions_res) == -1 ) {
        free(sidbufp);

        if ( errno != E2BIG ) {
            TRACE(Trace::error, errno);
			throw(Error::LTFSDM_GENERAL_ERROR);
        }

		sidbufp = NULL;
        sidbufp = (dm_sessid_t*)malloc(num_sessions_res * sizeof( dm_sessid_t ));

		if ( !sidbufp ) {
            MSG(LTFSDMD0001E);
			throw(Error::LTFSDM_GENERAL_ERROR);
        }

		num_sessions = num_sessions_res;
    }

	MSG(LTFSDMD0002I, num_sessions);

	unsigned int num_tokens = 1024;
	dm_token_t *tokbufp = NULL;

	for ( i = 0; i < (int)num_sessions; i++ ) {
        if ( dm_query_session(sidbufp[i], sizeof( buffer ), buffer, &rseslenp) == -1 ) {
            MSG(LTFSDMD0001E);
			free(sidbufp);
			throw(Error::LTFSDM_GENERAL_ERROR);
        }
		else if (Const::DMAPI_SESSION_NAME.compare(buffer) == 0) {
			TRACE(Trace::error, i);
			TRACE(Trace::error, (unsigned long) sidbufp[i]);

			tokbufp =  (dm_token_t *) malloc(sizeof(dm_token_t) * num_tokens);
			if ( !tokbufp ) {
				MSG(LTFSDMD0001E);
				free(sidbufp);
				throw(Error::LTFSDM_GENERAL_ERROR);
			}

			while (dm_getall_tokens(sidbufp[i], num_tokens, tokbufp, &rtoklenp) == -1) {
				if ( errno != E2BIG ) {
					TRACE(Trace::error, errno);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}
				free(tokbufp);
				tokbufp = NULL;
				tokbufp =  (dm_token_t *) malloc(sizeof(dm_token_t) * rtoklenp);
				if ( !tokbufp ) {
					MSG(LTFSDMD0001E);
					free(sidbufp);
					throw(Error::LTFSDM_GENERAL_ERROR);
				}
			}

			for (j = 0; j<rtoklenp; j++)
			{
				TRACE(Trace::error, j);
				TRACE(Trace::error, (unsigned long) tokbufp[j]);
				if ( dm_respond_event(sidbufp[i], tokbufp[j], DM_RESP_ABORT, EINTR, 0, NULL) == 1 )
					TRACE(Trace::error, errno);
				else
					MSG(LTFSDMD0003I);
			}

			if ( dm_destroy_session(sidbufp[i]) == -1 ) {
				TRACE(Trace::error, errno);
				MSG(LTFSDMD0004E);
			}
			else {
				MSG(LTFSDMD0005I, (unsigned long) sidbufp[i]);
			}
		}
	}
	if (sidbufp != NULL)
		free(sidbufp);
	if (tokbufp != NULL)
		free(tokbufp);
}

Connector::Connector()

{
	char          *version          = NULL;
    size_t         msglen           = 8;
    char           msgdatap[8];
	dm_sessid_t dmapiSessionLoc;
	dm_token_t dmapiTokenLoc;

	memset((char *) msgdatap, 0, sizeof(msgdatap));

    if (dm_init_service(&version)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

	dmapiSessionCleanup();

	if (dm_create_session(DM_NO_SESSION, (char *) Const::DMAPI_SESSION_NAME.c_str(), &dmapiSessionLoc)) {
		TRACE(Trace::error, errno);
		goto failed;
	}

    if(dm_create_userevent(dmapiSessionLoc, msglen, (void*)msgdatap, &dmapiTokenLoc)) {
        dm_destroy_session(dmapiSessionLoc);
		TRACE(Trace::error, errno);
		goto failed;
	}

	dmapiSession = dmapiSessionLoc;
	dmapiToken = dmapiTokenLoc;
	return;

failed:
	MSG(LTFSDMS0016E);
}

Connector::~Connector()

{
	if ( dm_respond_event(dmapiSession, dmapiToken, DM_RESP_ABORT, EINTR, 0, NULL) == 1 )
		TRACE(Trace::error, errno);
	else
		MSG(LTFSDMD0003I);

	dm_destroy_session(dmapiSession);
}

FsObj::FsObj(std::string fileName) : handle(NULL), handleLength(0), isLocked(false)

{
	char *fname = (char *) fileName.c_str();

	if ( dm_path_to_handle(fname, &handle, &handleLength) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::FsObj(unsigned long long fsId, unsigned int iGen, unsigned long long iNode)
	: handle(NULL), handleLength(0), isLocked(false)

{
	if ( dm_make_handle(&fsId, &iNode, &iGen, &handle, &handleLength) != 0 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

FsObj::~FsObj()

{
	dm_handle_free(handle, handleLength);
}

struct stat FsObj::stat()

{
	struct stat statbuf;
	dm_stat_t dmstatbuf;

	memset(&statbuf, 0, sizeof(statbuf));

	if ( handle == NULL )
		return statbuf;

 	if ( dm_get_fileattr(dmapiSession, handle, handleLength, DM_NO_TOKEN, DM_AT_STAT, &dmstatbuf) != 0 ) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	statbuf.st_dev = dmstatbuf.dt_dev;
	statbuf.st_ino = dmstatbuf.dt_ino;
	statbuf.st_mode = dmstatbuf.dt_mode;
	statbuf.st_nlink = dmstatbuf.dt_nlink;
	statbuf.st_uid = dmstatbuf.dt_uid;
	statbuf.st_gid = dmstatbuf.dt_gid;
	statbuf.st_rdev = dmstatbuf.dt_rdev;
	statbuf.st_size = dmstatbuf.dt_size;
	statbuf.st_blksize = dmstatbuf.dt_blksize;
	statbuf.st_blocks = dmstatbuf.dt_blocks;
	statbuf.st_atime = dmstatbuf.dt_atime;
	statbuf.st_mtime = dmstatbuf.dt_mtime;
	statbuf.st_ctime = dmstatbuf.dt_ctime;

	return statbuf;
}

unsigned long long FsObj::getFsId()

{
	unsigned long long fsid;

	if ( handleLength == 0 )
		return 0;

	if (dm_handle_to_fsid(handle, handleLength, &fsid)) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	return fsid;
}

unsigned int FsObj::getIGen()

{
	unsigned int igen;

	if ( handleLength == 0 )
		return 0;

	if (dm_handle_to_igen(handle, handleLength, &igen)) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	return igen;
}

unsigned long long FsObj::getINode()

{
	unsigned long long ino;

	if ( handleLength == 0 )
		return 0;

	if (dm_handle_to_ino(handle, handleLength, &ino)) {
		TRACE(Trace::error, errno);
	    throw(errno);
	}

	return ino;
}

std::string FsObj::getTapeId()

{
	std::stringstream sstr;

	sstr << "DV148" << random() % 2 <<  "L6";

	return sstr.str();
}

void FsObj::lock()

{
	int rc;

	rc = dm_request_right(dmapiSession, handle, handleLength, dmapiToken, DM_RR_WAIT, DM_RIGHT_EXCL);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	isLocked = true;
}

void FsObj::unlock()

{
	int rc;

	if ( isLocked == false ) {
		TRACE(Trace::error, isLocked);
		throw(Error::LTFSDM_GENERAL_ERROR);
	}

	rc = dm_release_right(dmapiSession, handle, handleLength, dmapiToken);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	isLocked = false;
}

long FsObj::read(long offset, unsigned long size, char *buffer)

{
	long rsize;

	rsize = dm_read_invis(dmapiSession, handle, handleLength, dmapiToken, offset, size, buffer);

	TRACE(Trace::much, offset);
	TRACE(Trace::much, size);
	TRACE(Trace::much, rsize);

	return rsize;
}


void FsObj::addAttribute(std::string key, std::string value)

{
	int rc;

	rc = dm_set_dmattr(dmapiSession, handle, handleLength, dmapiToken, (dm_attrname_t *) key.c_str(),
					   0, value.length(), (void *) value.c_str());

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

std::string FsObj::getAttribute(std::string key)

{
	int rc;
	char *buffer;
	unsigned long rsize;
	std::string value;

	buffer = (char *) malloc(1024);
	memset(buffer, 0, 1024);

	rc = dm_get_dmattr(dmapiSession, handle, handleLength, DM_NO_TOKEN,
					   (dm_attrname_t *) key.c_str(), 1024, buffer, &rsize);

	if ( rc == -1 && errno == E2BIG) {
		free(buffer);
		buffer = (char *) malloc(rsize);
		memset(buffer, 0, rsize);
		rc = dm_get_dmattr(dmapiSession, handle, handleLength, DM_NO_TOKEN,
						   (dm_attrname_t *) key.c_str(), rsize, buffer, &rsize);
	}

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}

	value = std::string(buffer);
	free(buffer);
	return buffer;
}

void FsObj::finishPremigration()

{
	int rc;
	dm_region_t reg;
	dm_boolean_t exact;


	/* Always remove entire file */
	reg.rg_offset = 0;
	reg.rg_size = 0;;         /* 0 = infinity */

	/* Mark the region as off-line */
	reg.rg_flags = DM_REGION_WRITE | DM_REGION_TRUNCATE;
	rc = dm_set_region(dmapiSession, handle, handleLength, dmapiToken, 1, &reg, &exact);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}

void FsObj::prepareStubbing()

{
	int rc;
	dm_region_t reg;
	dm_boolean_t exact;


	/* Always remove entire file */
	reg.rg_offset = 0;
	reg.rg_size = 0;;         /* 0 = infinity */

	/* Mark the region as off-line */
	reg.rg_flags = DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE;
	rc = dm_set_region(dmapiSession, handle, handleLength, dmapiToken, 1, &reg, &exact);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}


void FsObj::stub()

{
	int rc;

	/* Remove online data within the region */
	rc = dm_punch_hole(dmapiSession, handle, handleLength, dmapiToken, 0, 0);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
	}
}


FsObj::file_state FsObj::getMigState()
{
    unsigned int nelem = 2;
    dm_region_t regbuf[2];
	std::stringstream infos;
	int rc;
	unsigned int i;



    memset(regbuf, 0, sizeof( regbuf ));

    rc = dm_get_region(dmapiSession, handle, handleLength, DM_NO_TOKEN, nelem, regbuf, &nelem);

	if ( rc == -1 ) {
		TRACE(Trace::error, errno);
		throw(errno);
    }

    for (i = 0; i < nelem; i++ ) {
		infos << "region nr: " << i << ", offset: " << regbuf[i].rg_offset << ", size: " << regbuf[i].rg_size << ", flag: " << regbuf[i].rg_size;
		TRACE(Trace::much, infos.str());
	}

	if ( nelem > 1 ) {
		TRACE(Trace::error, nelem);
		throw(nelem);
    }

	if ( nelem == 0 )
		return FsObj::RESIDENT;
	else if ( regbuf[0].rg_flags == (DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE) )
		return FsObj::MIGRATED;
	else
		return FsObj::PREMIGRATED;
}