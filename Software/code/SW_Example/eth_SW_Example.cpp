// Copyright: Microsoft 2009
//
// Original Author: Ken Eguro and Sandro Forin
// Includes code written by 2009 MSR intern Rene Mueller.
//
// Modified by	: Praveen Kumar Pendyala
// Created		: 11/20/13 
// Modified		: 01/16/14
//
// Description:
// Send 128 bits of config data and 2 32-bit operands A, B as parameters
// Receive 16-bit response through data read
//
//	Bugs:
//	Due to a bug in the hardware code, responses actually start from memory address 1 and extend till memory addresss 3.
//	So we read back 1 bit more to adjust for the offset.
//
//  Notes:
//	Interpretation of response: The response of the output has to be interpreted in the format mentioned below
//	bit15 bit14 bit13 bit12 bit11 bit10 bit9 bit8   bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 an example of output is,
//  00001010 10100001. Here, 0th bit is 1, 7th - 1, 8th - 1, 15th - 0 
//
//  STAGE 1 code
//
//
#include <windows.h>
#include <WinIoctl.h>
#include <setupapi.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>
#include <assert.h>
#include <list>
#include <vector>
#include <time.h>
#include <direct.h>
#include <math.h>
#include <sstream>

#include "sirc_internal.h"
#include "display.h"

using namespace std;
using namespace System;
using namespace System::IO;
using namespace System::Collections;


//################################	Start of Global variables ####################################
#define runSize 1000 //# times output should be evaluated for same challenge
#define defaultTimeOut 30 //Time after which the code should giveup

//Configuration bits as integer arrays.
//The code will attempt to read values from file and if that fails then the below values will be used
int configTop[16]	 = {1,2,3,4,5,6,7,8};
int configBottom[16] = {8,7,6,5,4,3,2,1};

uint32_t A = 0x55555555;//55555555;//ffffffff;//operand A
uint32_t B = 0xAAAAAAAA;//AAAAAAAA;//operand B
	
uint32_t numOpsWrite = 16;	//write Length;
uint32_t numOpsRead = 2;	//read Length;

//For couting #1's in 100 runs for each bits. Initialized to 0 for each bit.
int nOfOnes[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//Tuning level = # 1's in topline - # 1's in bottom line. Taken from text file and used for naming output file.
//Default used when file is invalid
int tuningLevelDefault = 55555;

//################################	End of Global variables ####################################




//################################	Start of other function ####################################

void error(string inErr){
	cerr << "Error:" << endl;
	cerr << "\t" << inErr << endl;

    PrintZeLog();
	exit(-1);
}

string get8bitBinary(int n){
	int k;
	std::string s;
    std::stringstream out;
	for(int i=7; i>=0; i--){
		k = n/(pow(2.0,i));
		n = n%((int)pow(2.0,i));
		out<<k;
		s = out.str();
	}
	return s;
}

void adjustCounters(bool byteNum, int val){
	int k;
	//The current val is for the 1st 8-bits of response
	if(!byteNum){
		for(int i=7; i>=0; i--){
			k = val/(pow(2.0,i));
			if(k == 1)
				nOfOnes[i]++;
			val = val%((int)pow(2.0,i));
		}
	}

	//The current val is for the last 8-bits of response
	else{
		for(int i=7; i>=0; i--){
			k = val/(pow(2.0,i));
			if(k == 1)
				nOfOnes[i+8]++;
			val = val%((int)pow(2.0,i));
		}
	}
}

//################################	End of other function ####################################





/*################################	Main function starts ####################################
#############################################################################################*/

int main(int argc, char* argv[]){

	//Test harness variables
	ETH_SIRC *SIRC_P;
	uint8_t FPGA_ID[6];
	bool FPGA_ID_DEF = false;

	uint32_t numOpsReturned;

	cout<<endl<<endl;

/************************************************ Start of general SIRC_SW code ************************************************************************
************************************************* Do NOT have to modify this part of code for any case of SIRC_SW use **********************************
********** NOTE: For my convienence I commented out all print statements in the below portion of code. Uncomment to debug any issue ********************/

	uint32_t tempInt;
    uint32_t driverVersion = 0;

	char *token = NULL;
	char *next_token = NULL;
	uint32_t val;
	int waitTimeOut = defaultTimeOut;

	//Input buffer
	uint8_t *inputValues;
	//Output buffer
	uint8_t *outputValues;

	//Speed testing variables
	DWORD start, end;

	std::ostringstream tempStream;

#ifndef BUGHUNT
    StartLog();
#endif

	//**** Process command line input.
	//	We may ignore this. This is just to facilitate passing few params like FPGA id, numOps, waitTime and bandwidth params through terminal
	if (argc > 1) {
		cout<<"Passed arguments while executing.." << endl;
		for(int i = 1; i < argc; i++){
			if(strcmp(argv[i], "-mac") == 0){ 
				if(argc <= i + 1){
					tempStream << "-mac option requires argument";
					error(tempStream.str());					
				}
				
				//Map the target MAC address to an FPGA id.
				val = hexToFpgaId(argv[i + 1],FPGA_ID, sizeof(FPGA_ID));
				//Check to see if there were exactly 6 bytes
				if (val!=6) {
					tempStream << "Invalid MAC address. macToFpgaId returned " << i;
					error(tempStream.str());
				}

				cout << "destination MAC: "
						<< hex << setw(2) << setfill('0') 
						<< setw(2) << (int)FPGA_ID[0] << ":" 
						<< setw(2) << (int)FPGA_ID[1] << ":" 
						<< setw(2) << (int)FPGA_ID[2] << ":" 
						<< setw(2) << (int)FPGA_ID[3] << ":" 
						<< setw(2) << (int)FPGA_ID[4] << ":" 
						<< setw(2) << (int)FPGA_ID[5] << dec << endl;
				FPGA_ID_DEF = true;
				i++;
			}
			else if (strcmp(argv[i], "-waitTimeOut") == 0){
				if(argc <= i + 1){
					tempStream << "-waitTime option requires argument";
					error(tempStream.str());					
				}
				waitTimeOut = (uint32_t) atoi(argv[i + 1]);
				if ((waitTimeOut < 1)) {
					tempStream << "Invalid waitTime: " << waitTimeOut << ".  Must be >= 1";
					error(tempStream.str());
				}
				i++;
			}
			else{
				tempStream << "Unknown option: " << argv[i] << endl;
				tempStream << "Usage: " << argv[0] << " {-mac X:X:X:X:X:X} {-waitTimeOut X}" << endl;
				error(tempStream.str());
			}
		}
	} 
	if(!FPGA_ID_DEF){
		//cout << "****USING DEFAULT MAC ADDRESS - AA:AA:AA:AA:AA:AA" << endl;
		FPGA_ID[0] = 0xAA;
		FPGA_ID[1] = 0xAA;
		FPGA_ID[2] = 0xAA;
		FPGA_ID[3] = 0xAA;
		FPGA_ID[4] = 0xAA;
		FPGA_ID[5] = 0xAA;
	}

	//**** Set up communication with FPGA
	//Create communication object
	SIRC_P = new ETH_SIRC(FPGA_ID, driverVersion, NULL);
	//Make sure that the constructor didn't run into trouble
    if (SIRC_P == NULL){
		tempStream << "Unable to find a suitable SIRC driver or unable to ";
		error(tempStream.str());
    }
	if(SIRC_P->getLastError() != 0){
		tempStream << "Constructor failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}

    //Get runtime parameters, for what we won't change.
    SIRC::PARAMETERS params;
    if (!SIRC_P->getParameters(&params,sizeof(params))){
		tempStream << "Cannot getParameters from SIRC interface, code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
    }

    //These MUST match the buffer sizes in the hw design.
    params.maxInputDataBytes  = 1<<17; //2**17 128KBytes
    params.maxOutputDataBytes = 1<<13; //2**13 8KBytes

	if(waitTimeOut != 0){
		params.writeTimeout = waitTimeOut;
		params.readTimeout = waitTimeOut;
	}

	if (!SIRC_P->setParameters(&params,sizeof(params))){
		tempStream << "Cannot setParameters on SIRC interface, code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
    }

    //Fill up the input buffer
    if (numOpsWrite == 0){
       numOpsWrite = min(params.maxInputDataBytes, params.maxOutputDataBytes);
	}
	else if(numOpsWrite > params.maxInputDataBytes || numOpsWrite > params.maxOutputDataBytes){
		tempStream << "Invalid number of operations defined, must be less than or equal to " 
			<< (int)  min(params.maxInputDataBytes, params.maxOutputDataBytes);
		error(tempStream.str());
	}

	//cout << endl << endl << "Doing a soft rest before proceeding..." << endl;
    LogIt(LOGIT_TIME_MARKER);
	if(!SIRC_P->sendReset()){
		tempStream << "Reset failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}
	//cout << "Soft reset passed !" << endl << endl;
	cout<<endl;


/************************************************ End of general SIRC_SW code ************************************************************************
************************************************* Do NOT have to modify this part of code for any case of SIRC_SW use ********************************/ 






/************************************************ Start of user end of code ************************************************************************
************************************************* All required changes to be done here  ************************************************************/ 

	//This example circuit takes a few parameters:
	//	Param Register 0 - Operand A
	//	Param Register 1 - Operand B
	//	Input buffer	 - 128-configuration bits. 64 for top line, 64 for bottom line.
	//	Output			 - 16-bit response from the PUF

	//0 - read config data specified in the begining of this file
	//1 - read config data from file
	//The source will be decided depending upon the availability of proper config data in the text file
	bool configSource = 0;

	//Attempting to read configuration data from config.txt
	string configVector;
	ifstream configFile ("config.txt");
	char num[10];
	int configFileData[17];
	int n = 0;
	if (configFile.is_open()){
		cout << endl << "Reading from config file..." << endl;
		while (configFile.good() && n!=17){
			//17th value is tuning level
			configFile.getline (num, 256, ',');
			configFileData[n] = atoi(num);
			n++;
		}
		configFile.close();
	} else{
		cout << endl << "Unable to open file challenges file. Assigning default challenges.." << endl;
	}

	if(n==17)
		configSource = 1;
	else
		cout<<"Invalid data format in config file. Will use default config data. Please correct."<<endl;

	
	//cout << "Data sending phase started..." << endl;

	inputValues = (uint8_t *) malloc(sizeof(uint8_t) * numOpsWrite);
	assert(inputValues);
	outputValues = (uint8_t *) malloc(sizeof(uint8_t) * (numOpsRead+1));
	assert(outputValues);

	//Set input values to configuration bits
	//cout<<"Reading in configuration bits to inputvalue"<<endl;
	
	cout<<"Configuration for TOP line:    ";
	if(!configSource){
		for(int i = 0; i < 8; i++){
			inputValues[i] = configTop[i];
			cout<<(int)inputValues[i]<<", ";
		}
	} else{
		for(int i = 0; i < 8; i++){
			inputValues[i] = configFileData[i];
			cout<<(int)inputValues[i]<<", ";
		}
	}

	cout<<endl<<"Configuration for BOTTOM line: ";
	if(!configSource){
		for(int i = 0; i < 8; i++){
			inputValues[i+8] = configBottom[i];
			cout<<(int)inputValues[i+8]<<", ";
		}
	} else{
		for(int i = 0; i < 8; i++){
			inputValues[i+8] = configFileData[i+8];
			cout<<(int)inputValues[i+8]<<", ";
		}
	}
	cout<<endl;
	
	//cout<<endl<<"End of inputs. Data ready."<<endl<<endl;


    LogIt(LOGIT_TIME_MARKER);
	start = GetTickCount();
	//Set parameter register 0 to the operand A
	if(!SIRC_P->sendParamRegisterWrite(0, A)){
		tempStream << "Parameter register write failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}
	//Set parameter register 1 to the operand B
	if(!SIRC_P->sendParamRegisterWrite(1, B)){
		tempStream << "Parameter register write failed with code " << (int) SIRC_P->getLastError();
		error(tempStream.str());
	}


	//Next, send the input data
	//Start writing at address 0
//############################################## LOOPING STARTS HERE ###########################################
	cout << endl << endl;
	for(int i=0; i<runSize; i++){
		if(i%50 != 0)
			cout<<"#";
		else if(i != 0)
			cout << " " << i << "done!" << endl << "#";
		else
			cout << "#";

		//cout<<"Writing inputs to FPGA..."<<endl;
		if(!SIRC_P->sendWrite(0, numOpsWrite, inputValues)){
			tempStream << "Write to FPGA failed with code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
		} else{
			//cout<<"Write success !"<<endl<<endl;
		}

		//Set the run signal
		//cout<<"Issued a run signal"<<endl;
		if(!SIRC_P->sendRun()){
			tempStream << "Run command failed with code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
		} else{
			//cout<<"Run command issue success !"<<endl<<endl;
		}

		//Wait up to N seconds for the execution to finish (we can compute ~500M numbers in that time)
		if(waitTimeOut == 0){
			//cout<<"Allowed a waitTimeOut of : 30 secs"<<endl;
			if(!SIRC_P->waitDone(30)){
				tempStream << "Wait till done failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			} else{
				//cout<<"User code exectution completed successfully !"<<endl<<endl;
			}
		}
		else{
			if(!SIRC_P->waitDone(waitTimeOut)){
				tempStream << "Wait till done failed with code " << (int) SIRC_P->getLastError();
				error(tempStream.str());
			} else{
				//cout<<"User code exectution completed successfully !"<<endl<<endl;
			}
		}	

		//Read the data back
		//cout<<"Atempting to read back responses"<<endl;
		if(!SIRC_P->sendRead(0, (numOpsRead+1), outputValues)){
			tempStream << "Read from FPGA failed with code " << (int) SIRC_P->getLastError();
			error(tempStream.str());
		} else{
			//cout<<"Read back from memory success !"<<endl<<endl;
		}
		end = GetTickCount();

		// Adjust counters
		for(int i = 1; i < (numOpsRead+1); i++){
			if(i==1)
				adjustCounters(1, (int)outputValues[i]);
			if(i==2)
				adjustCounters(0, (int)outputValues[i]);
		}
		//cout<<endl<<"End of Outputs"<<endl<<endl;
	}
	cout << " 1000 done!" << endl << endl << "\t\tCompleted in " << (end - start) << " ms" << endl;
//###########################################     End of execution    ###############################################################




//################################################      Reports    ###################################################################

	//Final report
	cout<<endl<<endl<<"Final report for each bit:"<<endl<<endl;
	for(int i=0; i<16; i++){
		cout<<"# of 1's in bit "<<i<<" is: "<<nOfOnes[i]<<endl;
	}

	//Displaying config for one final time. Last display was long back.
	cout<<endl<<endl<<"Config used :"<<endl;
	cout<<"Top line: ";
	for(int i = 0; i < 8; i++){
		cout<<(int)inputValues[i]<<", ";
	}

	cout<<endl<<"Bottom line: ";
	for(int i = 8; i < 16; i++){
		cout<<(int)inputValues[i]<<", ";
	}
	cout<<endl<<endl<<endl;
	
//################################################      End of report    ###################################################################



//###########################################     Writing results to files    ###############################################################
	std::string tuningLevel;
    std::stringstream out;

	//Tuning level to be taken from file
	if(configSource){
		out<<configFileData[16];
	}
	//Default tuning level. The file is probably corrupted/invalid
	else{
		out<<tuningLevelDefault;
	}
	tuningLevel = out.str();


	//Writing Final report and config data to text file
	string outFileName = "results.txt";
	ofstream outfile (outFileName);

	//Writing final report
	outfile<<"Final report for each bit:"<<endl<<endl;
	for(int i=0; i<16; i++){
		outfile<<"# of 1's in bit "<<i<<" is: "<<nOfOnes[i]<<endl;
	}

	//Writing the Config
	outfile<<endl<<endl<<"Config used :"<<endl;
	outfile<<"Top line: ";
	for(int i = 0; i < 8; i++){
		outfile<<(int)inputValues[i]<<", ";
	}

	outfile<<endl<<"Bottom line: ";
	for(int i = 8; i < 16; i++){
		outfile<<(int)inputValues[i]<<", ";
	}

	outfile<<endl<<endl<<"# of runs: "<<runSize;
	outfile.close();

//###########################################     End of writing    ###############################################################

	


	delete SIRC_P;
	free(inputValues);
	free(outputValues);

#ifdef BUGHUNT
    StartLog(susp);
#endif
    PrintZeLog();
	return 0;
}
