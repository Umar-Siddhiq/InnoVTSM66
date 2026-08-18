

#ifndef						_NMEA_H

#define						_NMEA_H

#include "NuMicro.h"

#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


//***************NMEA***********************

#define NMEA_MAX_LENGTH 100

enum NMEA_SENTENCE_ID {
    NMEA_INVALID = -1,
    NMEA_UNKNOWN = 0,
    NMEA_SENTENCE_RMC,
    NMEA_SENTENCE_GGA,
    NMEA_SENTENCE_GSA,
    NMEA_SENTENCE_GLL,
    NMEA_SENTENCE_GST,
    NMEA_SENTENCE_GSV,
    NMEA_SENTENCE_VTG,
    NMEA_SENTENCE_ZDA,
		NMEA_SENTENCE_IRNS
};

struct NMEA_FLOAT {
    int_least32_t Value;
    int_least32_t Scale;
};

struct NMEA_DATE {
    int8_t Day;
    int8_t Month;
    int16_t Year;
};

struct NMEA_TIME {
    int8_t Hours;
    int8_t Minutes;
    int8_t Seconds;
    int8_t MicroSeconds;
};

typedef struct  {
    struct NMEA_TIME time;
    bool Valid;
    struct NMEA_FLOAT Latitude;
    struct NMEA_FLOAT Longitude;
    struct NMEA_FLOAT Speed;
    struct NMEA_FLOAT Course;
    struct NMEA_DATE Date;
    struct NMEA_FLOAT Variation; 
}RMCTypedef;

extern RMCTypedef RMCData;

typedef struct {
    struct NMEA_TIME Time;
    struct NMEA_FLOAT Latitude; char LatDir;
    struct NMEA_FLOAT Longitude; char LngDir;
    int Fix_Quality;
    int Satellites_Tracked;
    struct NMEA_FLOAT HDOP;
    struct NMEA_FLOAT Altitude; char Altitude_Units;
    struct NMEA_FLOAT Height; char Height_Units;
    struct NMEA_FLOAT Dgps_Age;
}GGATypedef ;

extern GGATypedef GGAData;

enum nmea_gll_status {
    nmea_GLL_STATUS_DATA_VALID = 'A',
    nmea_GLL_STATUS_DATA_NOT_VALID = 'V',
};

// FAA mode added to some fields in NMEA 2.3.
enum NMEA_FAA_MODE {
    NMEA_FAA_MODE_AUTONOMOUS = 'A',
    NMEA_FAA_MODE_DIFFERENTIAL = 'D',
    nmea_FAA_MODE_ESTIMATED = 'E',
    nmea_FAA_MODE_MANUAL = 'M',
    nmea_FAA_MODE_SIMULATED = 'S',
    nmea_FAA_MODE_NOT_VALID = 'N',
    nmea_FAA_MODE_PRECISE = 'P',
};

typedef struct {
    struct NMEA_FLOAT latitude;
    struct NMEA_FLOAT longitude;
    struct NMEA_TIME time;
    char status;
    char mode;
}GLLTypedef;

typedef struct {
    struct NMEA_TIME time;
    struct NMEA_FLOAT rms_deviation;
    struct NMEA_FLOAT semi_major_deviation;
    struct NMEA_FLOAT semi_minor_deviation;
    struct NMEA_FLOAT semi_major_orientation;
    struct NMEA_FLOAT latitude_error_deviation;
    struct NMEA_FLOAT longitude_error_deviation;
    struct NMEA_FLOAT altitude_error_deviation;
}GSTTypedef;

enum NMEA_GSA_MODE {
    NMEA_GPGSA_MODE_AUTO = 'A',
    NMEA_GPGSA_MODE_FORCED = 'M',
};

enum NMEA_GSA_FIX_TYPE {
    NMEA_GPGSA_FIX_NONE = 1,
    NMEA_GPGSA_FIX_2D = 2,
    NMEA_GPGSA_FIX_3D = 3,
};

typedef struct {
    char Mode;
    int Fix_Type;
    int Sats[12];
    struct NMEA_FLOAT PDOP;
    struct NMEA_FLOAT HDOP;
    struct NMEA_FLOAT VDOP;
}GSATypedef;

extern GSATypedef GSAData;

struct NMEA_SAT_INFO {
    int nr;
    int elevation;
    int azimuth;
    int snr;
};

typedef struct {
    int total_msgs;
    int msg_nr;
    int total_sats;
    struct NMEA_SAT_INFO sats[4];
}GSVTypedef;

extern GSVTypedef GSVData;

typedef struct {
    struct NMEA_FLOAT true_track_degrees;
    struct NMEA_FLOAT magnetic_track_degrees;
    struct NMEA_FLOAT speed_knots;
    struct NMEA_FLOAT speed_kph;
    enum NMEA_FAA_MODE faa_mode;
}VTGTypedef;

extern VTGTypedef VTGData;

typedef struct {
    struct NMEA_TIME time;
    struct NMEA_DATE date;
    int hour_offset;
    int minute_offset;
}ZDATypedef;


static inline int_least32_t nmea_rescale(struct NMEA_FLOAT *f, int_least32_t new_scale)
{
    if (f->Scale == 0)
        return 0;
    if (f->Scale == new_scale)
        return f->Value;
    if (f->Scale > new_scale)
        return (f->Value + ((f->Value > 0) - (f->Value < 0)) * f->Scale/new_scale/2) / (f->Scale/new_scale);
    else
        return f->Value * (new_scale/f->Scale);
}

static inline double nmea_tofloat(struct NMEA_FLOAT *f)
{
    if (f->Scale == 0)
        return 0.0;
    return (double) f->Value / (double) f->Scale;
}

static inline double nmea_tocoord(struct NMEA_FLOAT *f)
{
    if (f->Scale == 0)
        return 0.0;
    int_least32_t degrees = f->Value / (f->Scale * 100);
    int_least32_t minutes = f->Value % (f->Scale * 100);
    return (double) degrees + (double) minutes / (60 * f->Scale);
}

bool NMEA_Parse_RMC( RMCTypedef *frame, const char *sentence);
bool NMEA_Parse_VTG( VTGTypedef *frame, const char *sentence);
bool NMEA_Parse_GGA(GGATypedef *frame, const char *sentence);

#endif						// _NMEA_H
