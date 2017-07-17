#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <errno.h>

#include <string>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/errors/errors.h"
#include "src/common/tracing/Trace.h"

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

std::atomic<bool> exitClient(false);

void LTFSDmCommClient::connect()

{
	struct sockaddr_un addr;

	if ( (socRefFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		TRACE(Trace::error, errno);
		throw(EXCEPTION(Const::UNSET));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, Const::SOCKET_FILE.c_str(), sizeof(addr.sun_path)-1);

	if (::connect(socRefFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		TRACE(Trace::error, errno);
		close(socRefFd);
		socRefFd = Const::UNSET;
		throw(EXCEPTION(Const::UNSET));
	}

	GOOGLE_PROTOBUF_VERIFY_VERSION;
}

void LTFSDmCommServer::listen()

{
	struct sockaddr_un addr;

	if ( (socRefFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		TRACE(Trace::error, errno);
		throw(EXCEPTION(Const::UNSET));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, Const::SOCKET_FILE.c_str(), sizeof(addr.sun_path)-1);

	unlink(Const::SOCKET_FILE.c_str());

	if (bind((int) socRefFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		TRACE(Trace::error, errno);
		::close(socRefFd);
		socRefFd = Const::UNSET;
		throw(EXCEPTION(Const::UNSET));
	}

	if (::listen(socRefFd, SOMAXCONN) == -1) {
		TRACE(Trace::error, errno);
		::close(socRefFd);
		socRefFd = Const::UNSET;
		throw(EXCEPTION(Const::UNSET));
	}
}

void LTFSDmCommServer::accept()

{
	if ( (socAccFd = ::accept(socRefFd, NULL, NULL)) == -1) {
		TRACE(Trace::error, errno);
		socRefFd = Const::UNSET;
		throw(EXCEPTION(Const::UNSET));
	}
}

void LTFSDmComm::send(int fd)

{
	unsigned long MessageSize;
	unsigned long rsize;
	char *buffer;

	MessageSize = this->ByteSize();

	buffer = (char *) malloc(MessageSize + sizeof(long));
	memset(buffer, 0, MessageSize + sizeof(long));
	memcpy(buffer, &MessageSize, sizeof(long));
	if ( this->SerializeToArray(buffer + sizeof(long), MessageSize) == false ) {
		TRACE(Trace::error, buffer);
		throw(EXCEPTION(Const::UNSET));
	}

	TRACE(Trace::full, strlen(buffer));
	TRACE(Trace::full, MessageSize);

	if ( exitClient ) {
		buffer[0] = 0;
	}

	rsize = write(fd, buffer, MessageSize + sizeof(long));

	if ( rsize != 0 && rsize != MessageSize + sizeof(long) ) {
		free(buffer);
		TRACE(Trace::error, rsize);
		TRACE(Trace::error, MessageSize);
		TRACE(Trace::error, errno);
		TRACE(Trace::error, sizeof(long));
		MSG(LTFSDMX0008E);
		throw(EXCEPTION(Const::UNSET));
	}

	free(buffer);
}

ssize_t readx(int fd, char *buffer, size_t size)

{
    unsigned long bread = 0;
    ssize_t rsize;
    while (bread < size)
    {
        rsize = read(fd, buffer + bread, size - bread);
        if (rsize == -1 ) {
			TRACE(Trace::error, errno);
            return -1;
		}
        bread += rsize;
    }

	return bread;
}


void LTFSDmComm::recv(int fd)

{
	ssize_t MessageSize;
	ssize_t rsize;
	char *buffer;

	rsize = readx(fd, (char *) &MessageSize, sizeof(long));

	if (rsize != sizeof(long)) {
		TRACE(Trace::error, rsize);
		TRACE(Trace::error, sizeof(long));
		throw(EXCEPTION(Const::UNSET));
	}

	TRACE(Trace::full, MessageSize);

	buffer = (char *) malloc(MessageSize);
	memset(buffer, 0, MessageSize);

	rsize = readx(fd, buffer, MessageSize);

	if (rsize != MessageSize) {
		TRACE(Trace::error, rsize);
		TRACE(Trace::error, MessageSize);
		free(buffer);
		throw(EXCEPTION(Const::UNSET));
	}

	if ( rsize == 0 ) {
		throw(EXCEPTION(Const::UNSET));
	}

	this->ParseFromArray(buffer, MessageSize);
	free(buffer);
}
