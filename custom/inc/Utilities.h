#ifndef __UTILITIES_H__
#define __UTILITIES_H__
#include "VTS.h"
#include "GPRS.h"


void LowerString(char	*str);

// define max integer values if not defined
#ifndef MAX_UINT8
#define MAX_UINT8 255U
#endif

#ifndef MAX_INT8
#define MAX_INT8 127
#endif

#ifndef MIN_INT8
#define MIN_INT8 (-128)
#endif

#ifndef MAX_UINT16
#define MAX_UINT16 65535U
#endif

#ifndef MAX_INT16
#define MAX_INT16 32767
#endif

#ifndef MIN_INT16
#define MIN_INT16 (-32768)
#endif

#ifndef MAX_UINT32
#define MAX_UINT32 4294967295UL
#endif

#ifndef MAX_INT32
#define MAX_INT32 2147483647L
#endif

#ifndef MIN_INT32
#define MIN_INT32 (-2147483648L)
#endif



#define StringAdd(dest,format,...) \
    do { \
        char temp[512]; \
        Ql_sprintf(temp, format, __VA_ARGS__); \
        Ql_strcat(dest, temp); \
    } while (0)

void InsertChar(char* s, char value);
#ifndef PROTO_CDAC
void InsertIntValue(char* target,uint16_t value,const char* decimal);
void InsertFloatValue(char* target,double value, const char* decimal);
#else
void InsertIntValue_OLD(char* target,uint16_t value,const char* decimal);
void InsertFloatValue_OLD(char* target,double value, const char* decimal);
#endif

void InsertCurrentDateTime(char* target, uint8_t IsTime);
void InsertSpecificDateTime(char* target, uint8_t IsTime,_RTC *dt);
void AppendVariableString(char* dest,char* source,uint8_t maxLength,uint8_t MinLength,char* defaultValue);
void AppendFixString(char* dest, char* source, uint8_t length, char* defaultValue);

#ifdef PROTO_CDAC
void AdjustGPSTimeToIST(_RTC *dateTime);
#endif

#endif // __UTILITIES_H__
// End of file