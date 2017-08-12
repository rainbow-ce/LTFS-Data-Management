#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/resource.h>

#include <iostream>
#include <fstream>
#include <set>
#include <mutex>

#include "src/common/const/Const.h"
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"
#include "src/common/util/util.h"
#include "src/common/exception/OpenLTFSException.h"

#include "Trace.h"

extern const char *__progname;

Trace traceObject;

Trace::~Trace()

{
    if ( fd != Const::UNSET )
        close(fd);
    fd = Const::UNSET;
}

void Trace::setTrclevel(traceLevel level)

{
    traceLevel oldLevel;

    switch (level) {
        case Trace::none:
            trclevel = level;
            break;
        case Trace::error:
        case Trace::normal:
        case Trace::full:
            oldLevel = trclevel;
            TRACE(Trace::error, oldLevel, level);
            trclevel = level;
            break;
        default:
            trclevel = Trace::error;
            break;
    }
}

int Trace::getTrclevel()

{
    return trclevel;
}

void Trace::init(std::string extension)

{
    if ( extension.compare("") != 0 )
        fileName.append(extension);

    fd = open(fileName.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC | O_SYNC , 0700 );

    if ( fd == Const::UNSET ) {
        MSG(LTFSDMX0001E);
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }
}

void Trace::rotate()

{
    std::lock_guard<std::mutex> lock(mtx);

    if (lseek(fd, 0, SEEK_CUR) < 100 * 1024 * 1024)
        return;

    close(fd);

    if (std::string(__progname).compare("ltfsdmd") == 0) {
        unlink((fileName + ".2").c_str());
        rename((fileName + ".1").c_str(),
                (fileName + ".2").c_str());
        rename(fileName.c_str(), (fileName + ".1").c_str());
    }

    init("");
}
