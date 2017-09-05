#include <sys/resource.h>

#include <unistd.h>
#include <string>
#include <list>
#include <sstream>
#include <exception>

#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "OpenLTFSCommand.h"
#include "InfoRequestsCommand.h"

void InfoRequestsCommand::printUsage()
{
    INFO(LTFSDMC0009I);
}

void InfoRequestsCommand::doCommand(int argc, char **argv)
{
    long reqOfInterest;

    processOptions(argc, argv);

    TRACE(Trace::normal, *argv, argc, optind);

    if (argc != optind) {
        printUsage();
        THROW(Error::LTFSDM_GENERAL_ERROR);
    } else if (requestNumber < Const::UNSET) {
        printUsage();
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    reqOfInterest = requestNumber;

    try {
        connect();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0026E);
        return;
    }

    LTFSDmProtocol::LTFSDmInfoRequestsRequest *inforeqs =
            commCommand.mutable_inforequestsrequest();

    inforeqs->set_key(key);
    inforeqs->set_reqnumber(reqOfInterest);

    try {
        commCommand.send();
    } catch (const std::exception& e) {
        MSG(LTFSDMC0027E);
        THROW(Error::LTFSDM_GENERAL_ERROR);
    }

    INFO(LTFSDMC0060I);
    int recnum;

    do {
        try {
            commCommand.recv();
        } catch (const std::exception& e) {
            MSG(LTFSDMC0028E);
            THROW(Error::LTFSDM_GENERAL_ERROR);
        }

        const LTFSDmProtocol::LTFSDmInfoRequestsResp inforeqsresp =
                commCommand.inforequestsresp();
        std::string operation = inforeqsresp.operation();
        recnum = inforeqsresp.reqnumber();
        std::string tapeid = inforeqsresp.tapeid();
        std::string tstate = inforeqsresp.targetstate();
        std::string state = inforeqsresp.state();
        std::string pool = inforeqsresp.pool();
        if (recnum != Const::UNSET)
            INFO(LTFSDMC0061I, operation, state, recnum, pool, tapeid, tstate);

    } while (recnum != Const::UNSET);

    return;
}
