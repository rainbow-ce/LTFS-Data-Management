#include <iostream>
#include <fstream>

#include "src/common/const/Const.h"
#include "src/common/messages/Message.h"
#include "src/common/errors/errors.h"

#include "Trace.h"

Trace traceObject;

void Trace::setTrclevel(int level)

{
	trclevel = level;
}

int Trace::getTrclevel()

{
	return trclevel;
}

Trace::Trace() : trclevel(0)

{
	tracefile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		tracefile.open(Const::TRACE_FILE, std::fstream::out | std::fstream::app);
	}
	catch(...) {
		MSG_OUT(OLTFSX0001E);
		exit((int) OLTFSErr::OLTFS_GENERAL_ERROR);
	}
}

Trace::~Trace()

{
	tracefile.close();
}