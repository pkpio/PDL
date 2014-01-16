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

#ifndef DEFINETYPESH
#define DEFINETYPESH
#include "types.h"
#endif

#ifndef LOGH
#define LOGH
#include "log.h"
#endif

#ifndef DEFINESIRCERRORH
#define DEFINESIRCERRORH
#include "sirc_error.h"
#endif

#ifndef DEFINESIRCH
#define DEFINESIRCH
#include "sirc.h"
#endif

#ifndef DEFINEPACKETH
#define DEFINEPACKETH
#include "packet.h"
#endif

#ifndef DEFINEETHSIRCH
#define DEFINEETHSIRCH
#include "eth_SIRC.h"
#endif

#ifndef DEFINEPCIESIRCH
#define DEFINEPCIESIRCH
#include "pcie_SIRC.h"
#endif

//Newer driver
#ifndef DEFINEPCIE2SIRCH
#define DEFINEPCIE2SIRCH
#include "pcie2_SIRC.h"
#endif

#ifndef DEFINEPICOSIRCH
#define DEFINEPICOSIRCH
#include "pico_SIRC.h"
#endif

#ifndef DEFINESRVSIRCH
#define DEFINESRVSIRCH
#include "srv_SIRC.h"
#endif

#ifndef DEFINEUTILH
#define DEFINEUTILH
#include "util.h"
#endif

#ifndef DEFINECPUTOOLSH
#define DEFINECPUTOOLSH
#include "cputools.h"
#endif

#endif
