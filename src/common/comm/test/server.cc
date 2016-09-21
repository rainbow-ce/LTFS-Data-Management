#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

char *socket_path = (char *) "/tmp/ltfsdmd";

int main(int argc, char *argv[]) {
	LTFSDmCommServer command;
	int cl;

	command.connect();

	while (1) {
		if ( (cl = accept(fd, NULL, NULL)) == -1) {
			perror("accept error");
			continue;
		}

		// command.ParseFromFileDescriptor(cl);
		command.recv(cl);

		printf("============================================================\n");

		// MIGRATION
		if ( command.has_migrequest() ) {
			printf("Migration Request\n");
			const LTFSDmProtocol::LTFSDmMigRequest migreq = command.migrequest();
			printf("key: %lu\n", migreq.key());
			printf("token: %lu\n", migreq.token());
			pid = migreq.pid();
			printf("client pid: %lu\n", pid);
			switch (migreq.state()) {
				case LTFSDmProtocol::LTFSDmMigRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			for (int j = 0; j < migreq.filenames_size(); j++) {
				const LTFSDmProtocol::LTFSDmMigRequest::FileName& filename = migreq.filenames(j);
				printf("file name: %s\n", filename.filename().c_str());
			}

			for (int i=0; i<1; i++) {
				printf("... wait\n");
				sleep(1);
			}

			// RESPONSE

			LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp = command.mutable_migrequestresp();

			migreqresp->set_success(true);
			migreqresp->set_token(time(NULL));
			migreqresp->set_pid(pid);

			command.send(cl);
		}

		// SELECTIVE RECALL
		else if ( command.has_selrecrequest() ) {
			printf("Selective Recall Request\n");
			const LTFSDmProtocol::LTFSDmSelRecRequest selrecreq = command.selrecrequest();
			printf("key: %lu\n", selrecreq.key());
			printf("key: %lu\n", selrecreq.token());
			switch (selrecreq.state()) {
				case LTFSDmProtocol::LTFSDmSelRecRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			for (int j = 0; j < selrecreq.filenames_size(); j++) {
				const LTFSDmProtocol::LTFSDmSelRecRequest::FileName& filename = selrecreq.filenames(j);
				printf("file name: %s\n", filename.filename().c_str());
			}
		}

		// TRANSPARENT RECALL
		else if ( command.has_selrecrequest() ) {
			printf("Transparent Recall Request\n");
			const LTFSDmProtocol::LTFSDmTransRecRequest transrecreq = command.transrecrequest();
			printf("key: %lu\n", transrecreq.key());
			printf("key: %lu\n", transrecreq.token());
			switch (transrecreq.state()) {
				case LTFSDmProtocol::LTFSDmTransRecRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmProtocol::LTFSDmTransRecRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			printf("file with inode %lu will be recalled transparently\n", transrecreq.inum());
		}
		else
			printf("unkown command\n");

		printf("============================================================\n");

	}


	return 0;
}
