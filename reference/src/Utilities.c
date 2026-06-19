


#include "Utilities.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
//#include <math.h>
#include "VTS.h"


void StringAdd(char* dest,const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(dest + strlen(dest), format, args);
    va_end(args);
}

void InsertChar(char* s, char value)
{
    unsigned int i;
    i=strlen(s);
    s[i]=value;
    s[i+1]=0;
}



void AppendVariableString(char* dest,char* source,uint8_t maxLength,uint8_t MinLength,char* defaultValue)
{
	uint16_t ln;
	ln = strlen(source);
	if((ln > maxLength) || (ln < MinLength))
	{
		strcat(dest,defaultValue);
	}
	else
	{
		strcat(dest,source);
	}
}

void AppendFixString(char* dest, char* source, uint8_t length, char* defaultValue)
{
	uint16_t ln;
	ln = strlen(source);
	if(ln == length)
	{
		strcat(dest,source);
	}
	else
		strcat(dest,defaultValue);
}



// void InsertStringValue(char* target,const char* value, uint16_t position, uint16_t length, uint8_t wh)
// {
// 	uint16_t n=0,i=0;
// 	char vl;
// 	uint16_t ln=strlen(value);
// 	memset(&target[position],'0',length);
	
// 	if(!value) // Or ln == 0
// 		return;
// 	if(ln > 0)
// 	{
// 		if(ln <= length)
// 			n=length-ln;
// 		else
// 			ln=length;

// 		position = position + n;
// 		while(i<ln)
// 		{
// 			if(isprint(value[i]))
// 				vl=value[i];
// 			else
// 				vl='0';
// 			if((wh) && (vl=='-'))
// 				vl='0';
// 			i++;
// 			target[position++]=vl;
// 		}
// 	}
// }
void remove_spaces(char* restrict str_trimmed, const char* restrict str_untrimmed)
{
  while (*str_untrimmed != '\0')
  {
    if(!isspace(*str_untrimmed))
    {
      *str_trimmed = *str_untrimmed;
      str_trimmed++;
    }
    str_untrimmed++;
  }
  *str_trimmed = '\0';
}

#ifndef PROTO_CDAC
void InsertIntValue(char* target,uint16_t value,const char* decimal)
{
	char ss[20];
	memset(ss,0,18);
	sprintf(ss,decimal,value);
	strcat(target,ss);
}



void InsertFloatValue(char* target,double value, const char* decimal)
{
	char ss[20];
	memset(ss,0,18);
	sprintf(ss,decimal,value);
	
	strcat(target,ss);
}

void InsertCurrentDateTime(char* target, uint8_t IsTime)
{
	char tempData[12];
	if(IsTime)
	{
		sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Hour ,CurrentDateTime.Min , CurrentDateTime.Sec);
		strcat(target,tempData);
	}
	else
	{
		sprintf(tempData,"%02d%02d20%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
		strcat(target,tempData);
	}
	//target[7]='\0';
}
#else
void InsertIntValue_OLD(char* target,uint16_t value,const char* decimal)
{
	char ss[20];
	memset(ss,0,18);
	sprintf(ss,decimal,value);
	strcat(target,ss);
}



void InsertFloatValue_OLD(char* target,double value, const char* decimal)
{
	char ss[20];
	memset(ss,0,18);
	sprintf(ss,decimal,value);
	
	strcat(target,ss);
}
void InsertCurrentDateTime_OLD(char* target, uint8_t IsTime)
{
	char tempData[12];
	if(IsTime)
	{
		sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Hour ,CurrentDateTime.Min , CurrentDateTime.Sec);
		strcat(target,tempData);
	}
	else
	{
		sprintf(tempData,"%02d%02d20%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
		strcat(target,tempData);
	}
	//target[7]='\0';
}
#endif

uint16_t ParseData(char* src,char* field,char delim,uint16_t num)
{
	uint16_t i = 0,b = 0, c = 0, k, ln;
	ln=strlen(src);
	if(ln < num)
		return 0;
	field[0]=0;
	while(i < ln)
	{
		if(src[i]==delim)
		{
			if(num == b)
			{
				k=i - (c);
				if((k > 0) && (k < 20))
				{
					strncpy(field,&src[c],k);
					return c;
				}
			}
			b++;
			c=i+1;
		}
		i++;
	}
	return 0;
}


// /**
//   * @brief  Splits a float into two integer values.
//   * @param  in the float value as input
//   * @param  out_value the pointer to the output integer structure
//   * @param  dec_prec the decimal precision to be used
//   * @retval None
//   */
// void floatToInt(float in, displayFloatToInt_t *out_value, int32_t dec_prec)
// {
//   if(in >= 0.0f)
//   {
//     out_value->sign = 0;
//   }else
//   {
//     out_value->sign = 1;
//     in = -in;
//   }

//   in = in + (0.5f / pow(10, dec_prec));
//   out_value->out_int = (int32_t)in;
//   in = in - (float)(out_value->out_int);
//   // out_value->out_dec = (int32_t)trunc(in * pow(10, dec_prec));
// }

