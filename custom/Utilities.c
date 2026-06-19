#include "Utilities.h"
#include "ql_stdlib.h"
#include <ctype.h>

static unsigned char tolow(unsigned char c)
{
    if ((c > 64) && (c<=90))
        c -= 'A'-'a';
    return c;
}



void LowerString(char	*str)
{
	uint16_t len = Ql_strlen(str);
	uint16_t i;
	for(i = 0; i < len;i++)
	{
		str[i] = tolow(str[i]);
	}
}

void InsertChar(char* s, char value)
{
    unsigned int i;
    i=Ql_strlen(s);
    s[i]=value;
    s[i+1]=0;
}


void AppendVariableString(char* dest,char* source,uint8_t maxLength,uint8_t MinLength,char* defaultValue)
{
	uint16_t ln;
	ln = Ql_strlen(source);
	if((ln > maxLength) || (ln < MinLength))
	{
		Ql_strcat(dest,defaultValue);
	}
	else
	{
		Ql_strcat(dest,source);
	}
}

void AppendFixString(char* dest, char* source, uint8_t length, char* defaultValue)
{
	uint16_t ln;
	ln = Ql_strlen(source);
	if(ln == length)
	{
		Ql_strcat(dest,source);
	}
	else
		Ql_strcat(dest,defaultValue);
}

void remove_spaces(char* restrict str_trimmed, const char* restrict str_untrimmed)
{
  while (*str_untrimmed != '\0')
  {
	if(!isspace((unsigned char)*str_untrimmed))
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
	Ql_memset(ss,0,18);
	Ql_sprintf(ss,decimal,value);
	Ql_strcat(target,ss);
}



void InsertFloatValue(char* target,double value, const char* decimal)
{
	char ss[20];
	Ql_memset(ss,0,18);
	Ql_sprintf(ss,decimal,value);
	
	Ql_strcat(target,ss);
}

void InsertCurrentDateTime(char* target, uint8_t IsTime)
{
	char tempData[12];
	if(IsTime)
	{
		Ql_sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Hour ,CurrentDateTime.Min , CurrentDateTime.Sec);
		Ql_strcat(target,tempData);
	}
	else
	{
		Ql_sprintf(tempData,"%02d%02d20%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
		Ql_strcat(target,tempData);
	}
	//target[7]='\0';
}

void InsertSpecificDateTime(char* target, uint8_t IsTime,_RTC *dt)
{
	char tempData[12];
	if(IsTime)
	{
		Ql_sprintf(tempData,"%02d%02d%02d",dt->Hour ,dt->Min , dt->Sec);
		Ql_strcat(target,tempData);
	}
	else
	{
		Ql_sprintf(tempData,"%02d%02d20%02d",dt->Date,dt->Month,dt->Year);
		Ql_strcat(target,tempData);
	}
	//target[7]='\0';
}
#else
void InsertIntValue_OLD(char* target,uint16_t value,const char* decimal)
{
	char ss[20];
	Ql_memset(ss,0,18);
	Ql_sprintf(ss,decimal,value);
	Ql_strcat(target,ss);
}



void InsertFloatValue_OLD(char* target,double value, const char* decimal)
{
	char ss[20];
	Ql_memset(ss,0,18);
	Ql_sprintf(ss,decimal,value);
	
	Ql_strcat(target,ss);
}
void InsertCurrentDateTime_OLD(char* target, uint8_t IsTime)
{
	char tempData[12];
	if(IsTime)
	{
		Ql_sprintf(tempData,"%02d%02d%02d",CurrentDateTime.Hour ,CurrentDateTime.Min , CurrentDateTime.Sec);
		Ql_strcat(target,tempData);
	}
	else
	{
		Ql_sprintf(tempData,"%02d%02d20%02d",CurrentDateTime.Date,CurrentDateTime.Month,CurrentDateTime.Year);
		Ql_strcat(target,tempData);
	}
	//target[7]='\0';
}
#endif

uint16_t ParseData(char* src,char* field,char delim,uint16_t num)
{
	uint16_t i = 0,b = 0, c = 0, k, ln;
	ln=Ql_strlen(src);
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
					Ql_strncpy(field,&src[c],k);
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

#ifdef PROTO_CDAC
// Adjust GPS time (UTC) to IST (UTC + 5:30)
void AdjustGPSTimeToIST(_RTC *dateTime)
{
    if (!dateTime) return;
    
    // Add 5 hours and 30 minutes to UTC time
    dateTime->Min += 30;
    if (dateTime->Min >= 60)
    {
        dateTime->Min -= 60;
        dateTime->Hour += 1;
    }
    
    dateTime->Hour += 5;
    if (dateTime->Hour >= 24)
    {
        dateTime->Hour -= 24;
        dateTime->Date += 1;
        
        // Handle month rollover (simplified - assumes 30 days per month for GPS adjustment)
        uint8_t daysInMonth = 30;
        if (dateTime->Month == 2)
            daysInMonth = 28;
        else if (dateTime->Month == 1 || dateTime->Month == 3 || dateTime->Month == 5 || 
                 dateTime->Month == 7 || dateTime->Month == 8 || dateTime->Month == 10 || dateTime->Month == 12)
            daysInMonth = 31;
            
        if (dateTime->Date > daysInMonth)
        {
            dateTime->Date = 1;
            dateTime->Month += 1;
            
            if (dateTime->Month > 12)
            {
                dateTime->Month = 1;
                dateTime->Year += 1;
            }
        }
    }
}
#endif