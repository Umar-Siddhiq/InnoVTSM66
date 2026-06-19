

#ifndef		_UTILITIES_H
#define		_UTILITIES_H

#include "Project.h"

typedef struct displayFloatToInt_s {
  int8_t sign; /* 0 means positive, 1 means negative*/
  uint32_t  out_int;
  uint32_t  out_dec;
} displayFloatToInt_t;


void StringAdd(char* dest,const char* format, ...);
void InsertChar(char* s, char value);
void AppendVariableString(char* dest,char* source,uint8_t maxLength,uint8_t MinLength,char* defaultValue);
void AppendFixString(char* dest, char* source, uint8_t length, char* defaultValue);
//void InsertStringValue(char* target,const char* value, uint16_t position, uint16_t length, uint8_t wh);
#ifndef PROTO_CDAC
void InsertIntValue(char* target,uint16_t value,const char* decimal);
void InsertFloatValue(char* target,double value, const char* decimal);
//void InsertSchedule(char* target,uint16_t pos);
void InsertCurrentDateTime(char* target, uint8_t IsTime);
#else
void InsertIntValue_OLD(char* target,uint16_t value,const char* decimal);
void InsertFloatValue_OLD(char* target,double value, const char* decimal);
void InsertCurrentDateTime_OLD(char* target, uint8_t IsTime);
#endif
uint16_t ParseData(char* src,char* field,char delim,uint16_t num);
void remove_spaces(char* restrict str_trimmed, const char* restrict str_untrimmed);


#endif	// _UTILITIES_H

