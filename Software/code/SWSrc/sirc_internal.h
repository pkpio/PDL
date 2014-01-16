#ifndef DEFINEINCLUDEH
#define DEFINEINCLUDEH

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

using namespace std;

//#define BIGDEBUG

#ifndef SIRC_DLL_LINKAGE
#define SIRC_DLL_LINKAGE /* Only valid when building static library */
#endif

#include "sirc.h"

#include "sirc_error.h"

#include "log.h"

#include "packet.h"

#include "eth_SIRC.h"

#include "pcie_SIRC.h"

//Newer driver
#include "pcie2_SIRC.h"

//#include "pico_SIRC.h"

#include "srv_SIRC.h"

#include "sirc_util.h"

#include "cputools.h"

#endif
