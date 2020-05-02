#pragma warning(disable:4214)   // bit field types other than int
#pragma error(disable:2220)
#pragma warning(disable:4201)   // nameless struct/union
#pragma warning(disable:4115)   // named type definition in parentheses
#pragma warning(disable:4127)   // conditional expression is constant
#pragma warning(disable:4054)   // cast of function pointer to PVOID
#pragma warning(disable:4244)   // conversion from 'int' to 'BOOLEAN', possible loss of data

#ifdef __cplusplus
extern "C"{  
#endif

#include <ndis.h> 

#ifdef __cplusplus
};
#endif

#include "RuleDef.h"
#include "MemoryBase.h"
#include "NetStructs.h" 
#include "Firewall.h"
#include "LockList.h"
#include "RuleManager.h"
#include "IpFilter.h" 

 



