// SW_Server.cpp : Defines the entry point for the console application.
//

#include "include.h"

using namespace std;

#define nCounters 4
int Counters[nCounters];

void error(string inErr){
	cerr << "Error:" << endl;
	cerr << "\t" << inErr << endl;

    PrintZeLog();
    printf("Counters:");
    for (int i = 0; i < nCounters; i++)
        printf(" %u", Counters[i]);
    printf("\n");
	exit(-1);
}

int main(int argc, char* argv[])
{
    SRV_SIRC *srv;
	uint32_t *registerFile;
	uint8_t *inputBuffer;
	uint8_t *outputBuffer;
	bool writeAndExecute;
	uint32_t expectedOutputBytes;

    /* args? */
    uint32_t driverVersion = 0;
    if ((argc > 1) && (argv[1][0] == '-'))
        driverVersion = atoi(argv[1]+1);
    //BUGBUG add NIC name option

	std::ostringstream tempStream;

    srv = new SRV_SIRC(&registerFile, &inputBuffer, &outputBuffer, driverVersion);
	//Make sure that the constructor didn't run into trouble
	if(srv->getLastError() != 0){
		tempStream << "Constructor failed with code " << srv->getLastError();
		error(tempStream.str());
	}

    expectedOutputBytes = 66; //uhu?


	while(1){
		writeAndExecute = false;
		//Get data from the host until an execute command comes in
		if(!srv->processCommands(&writeAndExecute)){
			printf("processCommands failed!\n");
			exit(-1);
		}

		if(registerFile[1] != 0){
            // compute 
            uint32_t numOps = registerFile[0];
            uint32_t multiplier = registerFile[1];
            printf("Should compute %x %x...\n", numOps, multiplier);
            for (uint32_t i = 0; i < numOps; i++)
                outputBuffer[i] = inputBuffer[i] * multiplier;
            expectedOutputBytes = numOps;
		}

		srv->resetRunRegister();

		//Send the results back to the host
		//The behavior of this will depend on writeAndExecute
		if(writeAndExecute){
			//Notice, the output buffer is not hooked up.  Thus, the readback will always be 0.
			if(!srv->sendReadBacks(expectedOutputBytes)){
				exit(-1);
			}
		}
	}


    delete srv;

    PrintZeLog();

	return 0;
}

