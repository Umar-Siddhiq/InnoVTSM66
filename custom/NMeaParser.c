
#include "NMeaParser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#define boolstr(s) ((s) ? "true" : "false")


static  bool NMEA_Isfield(char c) {
    return isprint((unsigned char) c) && c != ',' && c != '*';
}

static int Hex2Int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

uint8_t NMEA_Checksum(const char *sentence)
{
	uint8_t checksum = 0x00;
    // Support senteces with or without the starting dollar sign.
    if (*sentence == '$')
        sentence++;

    

    // The optional checksum is an XOR of all bytes between "$" and "*".
    while (*sentence && *sentence != '*')
        checksum ^= *sentence++;

    return checksum;
}

bool NMEA_Check(const char *sentence, bool strict)
{
    uint8_t checksum = 0x00;
		int upper, lower, expected;
    // Sequence length is limited.
    if (Ql_strlen(sentence) > NMEA_MAX_LENGTH + 3)
        return false;

    // A valid sentence starts with "$".
    if (*sentence++ != '$')
        return false;

    // The optional checksum is an XOR of all bytes between "$" and "*".
    while (*sentence && *sentence != '*' && isprint((unsigned char) *sentence))
        checksum ^= *sentence++;

    // If checksum is present...
    if (*sentence == '*') {
        // Extract checksum.
        sentence++;
        upper = Hex2Int(*sentence++);
        if (upper == -1)
            return false;
        lower = Hex2Int(*sentence++);
        if (lower == -1)
            return false;
        expected = upper << 4 | lower;

        // Check for checksum mismatch.
        if (checksum != expected)
            return false;
    } else if (strict) {
        // Discard non-checksummed frames in strict mode.
        return false;
    }

    // The only stuff allowed at this point is a newline.
    if (*sentence && Ql_strcmp(sentence, "\n") && Ql_strcmp(sentence, "\r\n"))
        return false;

    return true;
}

bool  NMEA_Scan(const char *sentence, const char *format, ...)
{
	bool result = false;
	bool optional = false;
	const char *field = sentence;
	va_list ap;
	va_start(ap, format);

    
#define next_field() \
    do { \
        /* Progress to the next field. */ \
        while (NMEA_Isfield(*sentence)) \
            sentence++; \
        /* Make sure there is a field there. */ \
        if (*sentence == ',') { \
            sentence++; \
            field = sentence; \
        } else { \
            field = NULL; \
        } \
    } while (0)

    while (*format) {
        char type = *format++;

        if (type == ';') {
            // All further fields are optional.
            optional = true;
            continue;
        }

        if (!field && !optional) {
            // Field requested but we ran out if input. Bail out.
            goto parse_error;
        }

        switch (type) {
            case 'c': { // Single character field (char).
                char value = '\0';

                if (field && NMEA_Isfield(*field))
                    value = *field;

                *va_arg(ap, char *) = value;
            } break;

            case 'd': { // Single character direction field (int).
                int value = 0;

                if (field && NMEA_Isfield(*field)) {
                    switch (*field) {
                        case 'N':
													value='N';
													break;
                        case 'E':
                            value = 'E';
                            break;
                        case 'S':
														value = 'S';
                            break;
                        case 'W':
                            value = 'W';
                            break;
                        default:
                            goto parse_error;
                    }
                }

                *va_arg(ap, int *) = value;
            } break;

            case 'f': { // Fractional value with scale (struct minmea_float).
                int sign = 0;
                int_least32_t value = -1;
                int_least32_t scale = 0;

                if (field) {
                    while (NMEA_Isfield(*field)) {
                        if (*field == '+' && !sign && value == -1) {
                            sign = 1;
                        } else if (*field == '-' && !sign && value == -1) {
                            sign = -1;
                        } else if (isdigit((unsigned char) *field)) {
                            int digit = *field - '0';
                            if (value == -1)
                                value = 0;
                            if (value > (INT_LEAST32_MAX-digit) / 10) {
                                /* we ran out of bits, what do we do? */
                                if (scale) {
                                    /* truncate extra precision */
                                    break;
                                } else {
                                    /* integer overflow. bail out. */
                                    goto parse_error;
                                }
                            }
                            value = (10 * value) + digit;
                            if (scale)
                                scale *= 10;
                        } else if (*field == '.' && scale == 0) {
                            scale = 1;
                        } else if (*field == ' ') {
                            /* Allow spaces at the start of the field. Not NMEA
                             * conformant, but some modules do this. */
                            if (sign != 0 || value != -1 || scale != 0)
                                goto parse_error;
                        } else {
                            goto parse_error;
                        }
                        field++;
                    }
                }

                if ((sign || scale) && value == -1)
                    goto parse_error;

                if (value == -1) {
                    /* No digits were scanned. */
                    value = 0;
                    scale = 0;
                } else if (scale == 0) {
                    /* No decimal point. */
                    scale = 1;
                }
                if (sign)
                    value *= sign;

                *va_arg(ap, struct NMEA_FLOAT *) = (struct NMEA_FLOAT) {value, scale};
								
            } break;

            case 'i': { // Integer value, default 0 (int).
                int value = 0;

                if (field) {
                    char *endptr;
                    value = strtol(field, &endptr, 10);
                    if (NMEA_Isfield(*endptr))
                        goto parse_error;
                }

                *va_arg(ap, int *) = value;
            } break;

            case 's': { // String value (char *).
                char *buf = va_arg(ap, char *);

                if (field) {
                    while (NMEA_Isfield(*field))
                        *buf++ = *field++;
                }

                *buf = '\0';
            } break;

            case 't': { // NMEA talker+sentence identifier (char *).
                // This field is always mandatory.
							int f;
							char *buf = va_arg(ap, char *);
                if (!field)
                    goto parse_error;

                if (field[0] != '$')
                    goto parse_error;
                for ( f=0; f<5; f++)
                    if (!NMEA_Isfield(field[1+f]))
                        goto parse_error;

                
                memcpy(buf, field+1, 5);
                buf[5] = '\0';
            } break;

            case 'D': { // Date (int, int, int), -1 if empty.
							int f;
							
                struct NMEA_DATE *date = va_arg(ap, struct NMEA_DATE *);

                int d = -1, m = -1, y = -1;

                if (field && NMEA_Isfield(*field)) {
                    // Always six digits.
                    for (f=0; f<6; f++)
                        if (!isdigit((unsigned char) field[f]))
                            goto parse_error;

                    char dArr[] = {field[0], field[1], '\0'};
                    char mArr[] = {field[2], field[3], '\0'};
                    char yArr[] = {field[4], field[5], '\0'};
                    d = strtol(dArr, NULL, 10);
                    m = strtol(mArr, NULL, 10);
                    y = strtol(yArr, NULL, 10);
                }

                date->Day = d;
                date->Month = m;
                date->Year = y;
            } break;

            case 'T': { // Time (int, int, int, int), -1 if empty.
                struct NMEA_TIME *time_ = va_arg(ap, struct NMEA_TIME *);

                int h = -1, i = -1, s = -1, u = -1;

                if (field && NMEA_Isfield(*field)) {
                    // Minimum required: integer time.
                    for (int f=0; f<6; f++)
                        if (!isdigit((unsigned char) field[f]))
                            goto parse_error;

                    char hArr[] = {field[0], field[1], '\0'};
                    char iArr[] = {field[2], field[3], '\0'};
                    char sArr[] = {field[4], field[5], '\0'};
                    h = strtol(hArr, NULL, 10);
                    i = strtol(iArr, NULL, 10);
                    s = strtol(sArr, NULL, 10);
                    field += 6;

                    // Extra: fractional time. Saved as microseconds.
                    if (*field++ == '.') {
                        uint32_t value = 0;
                        uint32_t scale = 1000000LU;
                        while (isdigit((unsigned char) *field) && scale > 1) {
                            value = (value * 10) + (*field++ - '0');
                            scale /= 10;
                        }
                        u = value * scale;
                    } else {
                        u = 0;
                    }
                }

                time_->Hours = h;
                time_->Minutes = i;
                time_->Seconds = s;
                time_->MicroSeconds = u;
            } break;

            case '_': { // Ignore the field.
            } break;

            default: { // Unknown.
                goto parse_error;
            }
        }

        next_field();
    }

    result = true;

parse_error:
    va_end(ap);
    return result;
}


bool minmea_talker_id(char talker[3], const char *sentence)
{
    char type[6];
    if (!NMEA_Scan(sentence, "t", type))
        return false;

    talker[0] = type[0];
    talker[1] = type[1];
    talker[2] = '\0';

    return true;
}

enum NMEA_SENTENCE_ID nmea_sentence_id(const char *sentence, bool strict)
{
    if (!NMEA_Check(sentence, strict))
        return NMEA_INVALID;

    char type[6];
    if (!NMEA_Scan(sentence, "t", type))
        return NMEA_INVALID;

    if (!Ql_strcmp(type+2, "RMC"))
        return NMEA_SENTENCE_RMC;
    if (!Ql_strcmp(type+2, "GGA"))
        return NMEA_SENTENCE_GGA;
    if (!Ql_strcmp(type+2, "GSA"))
        return NMEA_SENTENCE_GSA;
    if (!Ql_strcmp(type+2, "GLL"))
        return NMEA_SENTENCE_GLL;
    if (!Ql_strcmp(type+2, "GST"))
        return NMEA_SENTENCE_GST;
    if (!Ql_strcmp(type+2, "GSV"))
        return NMEA_SENTENCE_GSV;
    if (!Ql_strcmp(type+2, "VTG"))
        return NMEA_SENTENCE_VTG;
    if (!Ql_strcmp(type+2, "ZDA"))
        return NMEA_SENTENCE_ZDA;

    return NMEA_UNKNOWN;
}

bool NMEA_Parse_RMC( RMCTypedef *frame, const char *sentence)
{
    // $GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62
    char type[6];
    char validity;
    int latitude_direction;
    int longitude_direction;
    int variation_direction;
    if (!NMEA_Scan(sentence, "tTcfdfdffDfd",
            type,
            &frame->time,
            &validity,
            &frame->Latitude, &latitude_direction,
            &frame->Longitude, &longitude_direction,
            &frame->Speed,
            &frame->Course,
            &frame->Date,
            &frame->Variation, &variation_direction))
        return false;
    if (Ql_strcmp(type+2, "RMC"))
        return false;

    frame->Valid = (validity == 'A');
//    frame->Latitude.Value *= latitude_direction;
//    frame->Longitude.Value *= longitude_direction;
//    frame->Variation.Value *= variation_direction;

    return true;
}

bool NMEA_Parse_GGA(GGATypedef *frame, const char *sentence)
{
    // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
    char type[6];
    int latitude_direction;
    int longitude_direction;

    if (!NMEA_Scan(sentence, "tTfdfdiiffcfcf_",
            type,
            &frame->Time,
            &frame->Latitude, &latitude_direction,
            &frame->Longitude, &longitude_direction,
            &frame->Fix_Quality,
            &frame->Satellites_Tracked,
            &frame->HDOP,
            &frame->Altitude, &frame->Altitude_Units,
            &frame->Height, &frame->Height_Units,
            &frame->Dgps_Age))
        return false;
    if (Ql_strcmp(type+2, "GGA"))
        return false;

//    frame->Latitude.Value *= latitude_direction;
//    frame->Longitude.Value *= longitude_direction;
			frame->LatDir=latitude_direction;
			frame->LngDir=longitude_direction;
    return true;
}

bool NMEA_Parse_GSA(GSATypedef *frame, const char *sentence)
{
    // $GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39
    char type[6];

    if (!NMEA_Scan(sentence, "tciiiiiiiiiiiiifff",
            type,
            &frame->Mode,
            &frame->Fix_Type,
            &frame->Sats[0],
            &frame->Sats[1],
            &frame->Sats[2],
            &frame->Sats[3],
            &frame->Sats[4],
            &frame->Sats[5],
            &frame->Sats[6],
            &frame->Sats[7],
            &frame->Sats[8],
            &frame->Sats[9],
            &frame->Sats[10],
            &frame->Sats[11],
            &frame->PDOP,
            &frame->HDOP,
            &frame->VDOP))
        return false;
    if (Ql_strcmp(type+2, "GSA"))
        return false;

    return true;
}

bool NMEA_Parse_GLL(GLLTypedef *frame, const char *sentence)
{
    // $GPGLL,3723.2475,N,12158.3416,W,161229.487,A,A*41$;
    char type[6];
    int latitude_direction;
    int longitude_direction;

    if (!NMEA_Scan(sentence, "tfdfdTc;c",
            type,
            &frame->latitude, &latitude_direction,
            &frame->longitude, &longitude_direction,
            &frame->time,
            &frame->status,
            &frame->mode))
        return false;
    if (Ql_strcmp(type+2, "GLL"))
        return false;

    frame->latitude.Value *= latitude_direction;
    frame->longitude.Value *= longitude_direction;

    return true;
}

bool NMEA_Parse_GST(GSTTypedef *frame, const char *sentence)
{
    // $GPGST,024603.00,3.2,6.6,4.7,47.3,5.8,5.6,22.0*58
    char type[6];

    if (!NMEA_Scan(sentence, "tTfffffff",
            type,
            &frame->time,
            &frame->rms_deviation,
            &frame->semi_major_deviation,
            &frame->semi_minor_deviation,
            &frame->semi_major_orientation,
            &frame->latitude_error_deviation,
            &frame->longitude_error_deviation,
            &frame->altitude_error_deviation))
        return false;
    if (Ql_strcmp(type+2, "GST"))
        return false;

    return true;
}

bool NMEA_Parse_GSV(GSVTypedef *frame, const char *sentence)
{
    // $GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74
    // $GPGSV,3,3,11,22,42,067,42,24,14,311,43,27,05,244,00,,,,*4D
    // $GPGSV,4,2,11,08,51,203,30,09,45,215,28*75
    // $GPGSV,4,4,13,39,31,170,27*40
    // $GPGSV,4,4,13*7B
    char type[6];

    if (!NMEA_Scan(sentence, "tiii;iiiiiiiiiiiiiiii",
            type,
            &frame->total_msgs,
            &frame->msg_nr,
            &frame->total_sats,
            &frame->sats[0].nr,
            &frame->sats[0].elevation,
            &frame->sats[0].azimuth,
            &frame->sats[0].snr,
            &frame->sats[1].nr,
            &frame->sats[1].elevation,
            &frame->sats[1].azimuth,
            &frame->sats[1].snr,
            &frame->sats[2].nr,
            &frame->sats[2].elevation,
            &frame->sats[2].azimuth,
            &frame->sats[2].snr,
            &frame->sats[3].nr,
            &frame->sats[3].elevation,
            &frame->sats[3].azimuth,
            &frame->sats[3].snr
            )) {
        return false;
    }
    if (Ql_strcmp(type+2, "GSV"))
        return false;

    return true;
}

bool NMEA_Parse_VTG( VTGTypedef *frame, const char *sentence)
{
    // $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48
    // $GPVTG,156.1,T,140.9,M,0.0,N,0.0,K*41
    // $GPVTG,096.5,T,083.5,M,0.0,N,0.0,K,D*22
    // $GPVTG,188.36,T,,M,0.820,N,1.519,K,A*3F
    char type[6];
    char c_true, c_magnetic, c_knots, c_kph, c_faa_mode;

    if (!NMEA_Scan(sentence, "tfcfcfcfc;c",
            type,
            &frame->true_track_degrees,
            &c_true,
            &frame->magnetic_track_degrees,
            &c_magnetic,
            &frame->speed_knots,
            &c_knots,
            &frame->speed_kph,
            &c_kph,
            &c_faa_mode))
        return false;
    if (Ql_strcmp(type+2, "VTG"))
        return false;
    // check chars
    if (c_true != 'T' ||
        c_magnetic != 'M' ||
        c_knots != 'N' ||
        c_kph != 'K')
        return false;
    frame->faa_mode = (enum NMEA_FAA_MODE)c_faa_mode;

    return true;
}

bool NMEA_Parse_ZDA( ZDATypedef *frame, const char *sentence)
{
  // $GPZDA,201530.00,04,07,2002,00,00*60
  char type[6];

  if(!NMEA_Scan(sentence, "tTiiiii",
          type,
          &frame->time,
          &frame->date.Day,
          &frame->date.Month,
          &frame->date.Year,
          &frame->hour_offset,
          &frame->minute_offset))
      return false;
  if (Ql_strcmp(type+2, "ZDA"))
      return false;

  // check offsets
  if (abs(frame->hour_offset) > 13 ||
      frame->minute_offset > 59 ||
      frame->minute_offset < 0)
      return false;

  return true;
}

