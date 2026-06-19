/**
 * @file CDAC_Protocol.h
 * @brief CDAC VTMS Phase 5 Protocol Constants and Definitions
 * 
 * This file contains all constants, field positions, and sizes for the
 * CDAC Vehicle Tracking and Monitoring System (VTMS) Phase 5 protocol.
 * 
 * Reference: CDAC VTMS Phase 5 Specification
 */

#ifndef _CDAC_PROTOCOL_H
#define _CDAC_PROTOCOL_H

#ifdef PROTO_CDAC

/*============================================================================
 * GENERAL CONSTANTS
 *============================================================================*/

/** HTTP POST prefix - all packets start with this */
#define CDAC_HTTP_PREFIX            "vltdata="
#define CDAC_HTTP_PREFIX_LEN        8

/** Base offset for all field positions (after vltdata=) */
#define CDAC_BASE_OFFSET            8

/** Field sizes */
#define CDAC_HEADER_SIZE            3       /* Packet header: NRM, EPB, CRT, ALT, BTH, HLM, FUL, LGN */
#define CDAC_IMEI_SIZE              15
#define CDAC_ALERT_ID_SIZE          2
#define CDAC_DATETIME_SIZE          12      /* DDMMYYHHmmSS */
#define CDAC_LAT_SIZE               10      /* 010.6f */
#define CDAC_LON_SIZE               10      /* 010.6f */
#define CDAC_MCC_SIZE               3
#define CDAC_MNC_SIZE               3
#define CDAC_LAC_SIZE               4
#define CDAC_CELLID_SIZE            9
#define CDAC_SPEED_SIZE             6       /* 03.2f */
#define CDAC_HEADING_SIZE           6       /* 03.2f */
#define CDAC_SATS_SIZE              2
#define CDAC_HDOP_SIZE              2
#define CDAC_SIGNAL_SIZE            2
#define CDAC_ALTITUDE_SIZE          7       /* 04.2f */
#define CDAC_NETWORK_NAME_SIZE      6
#define CDAC_VENDOR_ID_SIZE         6
#define CDAC_FIRMWARE_VER_SIZE      6
#define CDAC_VEHICLE_REG_SIZE       16
#define CDAC_ACTIVATION_KEY_SIZE    16
#define CDAC_CRC_SIZE               8
#define CDAC_FRAME_NUM_SIZE         6
#define CDAC_BATCH_COUNT_SIZE       3

/** Packet Status Values */
#define CDAC_STATUS_LIVE            '1'     /* Live packet */
#define CDAC_STATUS_STORED          '2'     /* Stored/History packet */
#define CDAC_STATUS_CHAR_LIVE       'L'     /* Live indicator */
#define CDAC_STATUS_CHAR_HISTORY    'H'     /* History indicator */

/** Packet Headers */
#define CDAC_HDR_NORMAL             "NRM"
#define CDAC_HDR_EMERGENCY          "EPB"
#define CDAC_HDR_CRITICAL           "CRT"
#define CDAC_HDR_ALERT              "ALT"
#define CDAC_HDR_BATCH              "BTH"
#define CDAC_HDR_HEALTH             "HLM"
#define CDAC_HDR_FULL               "FUL"
#define CDAC_HDR_LOGIN              "LGN"
#define CDAC_HDR_ACK                "ACK"

/*============================================================================
 * LOGIN PACKET (LGN) FIELD POSITIONS
 * Format: LGN<IMEI><ActivationKey><Lat><LatDir><Lon><LonDir><DateTime><Speed>
 *============================================================================*/

#define LGN_HEADER_POS              0                                   /* 3 chars */
#define LGN_IMEI_POS                (LGN_HEADER_POS + CDAC_HEADER_SIZE) /* 15 chars @ pos 3 */
#define LGN_ACTIVATION_KEY_POS      (LGN_IMEI_POS + CDAC_IMEI_SIZE)     /* 16 chars @ pos 18 */
#define LGN_LATITUDE_POS            (LGN_ACTIVATION_KEY_POS + CDAC_ACTIVATION_KEY_SIZE) /* 10 chars @ pos 34 */
#define LGN_LAT_DIR_POS             (LGN_LATITUDE_POS + CDAC_LAT_SIZE)  /* 1 char @ pos 44 */
#define LGN_LONGITUDE_POS           (LGN_LAT_DIR_POS + 1)               /* 10 chars @ pos 45 */
#define LGN_LON_DIR_POS             (LGN_LONGITUDE_POS + CDAC_LON_SIZE) /* 1 char @ pos 55 */
#define LGN_DATETIME_POS            (LGN_LON_DIR_POS + 1)               /* 12 chars @ pos 56 */
#define LGN_SPEED_POS               (LGN_DATETIME_POS + CDAC_DATETIME_SIZE) /* 6 chars @ pos 68 */
#define LGN_PACKET_SIZE             (LGN_SPEED_POS + CDAC_SPEED_SIZE)   /* Total: 74 chars */

/*============================================================================
 * COMMON DATA PACKET FIELD POSITIONS (used by NRM, EPB, CRT, ALT)
 * After header: <IMEI><AlertID><Status><GPSFix><DateTime><Lat>...
 *============================================================================*/

#define DATA_HEADER_POS             0                                   /* 3 chars: NRM/EPB/CRT/ALT */
#define DATA_IMEI_POS               (DATA_HEADER_POS + CDAC_HEADER_SIZE) /* 15 chars @ pos 3 */
#define DATA_ALERT_ID_POS           (DATA_IMEI_POS + CDAC_IMEI_SIZE)    /* 2 chars @ pos 18 */
#define DATA_STATUS_POS             (DATA_ALERT_ID_POS + CDAC_ALERT_ID_SIZE) /* 1 char @ pos 20 */
#define DATA_GPS_FIX_POS            (DATA_STATUS_POS + 1)               /* 1 char @ pos 21 */
#define DATA_DATETIME_POS           (DATA_GPS_FIX_POS + 1)              /* 12 chars @ pos 22 */
#define DATA_LATITUDE_POS           (DATA_DATETIME_POS + CDAC_DATETIME_SIZE) /* 10 chars @ pos 34 */
#define DATA_LAT_DIR_POS            (DATA_LATITUDE_POS + CDAC_LAT_SIZE) /* 1 char @ pos 44 */
#define DATA_LONGITUDE_POS          (DATA_LAT_DIR_POS + 1)              /* 10 chars @ pos 45 */
#define DATA_LON_DIR_POS            (DATA_LONGITUDE_POS + CDAC_LON_SIZE) /* 1 char @ pos 55 */
#define DATA_MCC_POS                (DATA_LON_DIR_POS + 1)              /* 3 chars @ pos 56 */
#define DATA_MNC_POS                (DATA_MCC_POS + CDAC_MCC_SIZE)      /* 3 chars @ pos 59 */
#define DATA_LAC_POS                (DATA_MNC_POS + CDAC_MNC_SIZE)      /* 4 chars @ pos 62 */
#define DATA_CELLID_POS             (DATA_LAC_POS + CDAC_LAC_SIZE)      /* 9 chars @ pos 66 */
#define DATA_SPEED_POS              (DATA_CELLID_POS + CDAC_CELLID_SIZE) /* 6 chars @ pos 75 */
#define DATA_HEADING_POS            (DATA_SPEED_POS + CDAC_SPEED_SIZE)  /* 6 chars @ pos 81 */
#define DATA_SATS_POS               (DATA_HEADING_POS + CDAC_HEADING_SIZE) /* 2 chars @ pos 87 */
#define DATA_HDOP_POS               (DATA_SATS_POS + CDAC_SATS_SIZE)    /* 2 chars @ pos 89 */
#define DATA_SIGNAL_POS             (DATA_HDOP_POS + CDAC_HDOP_SIZE)    /* 2 chars @ pos 91 */
#define DATA_IGN_POS                (DATA_SIGNAL_POS + CDAC_SIGNAL_SIZE) /* 1 char @ pos 93 */
#define DATA_MAIN_POWER_POS         (DATA_IGN_POS + 1)                  /* 1 char @ pos 94 */
#define DATA_VEHICLE_MODE_POS       (DATA_MAIN_POWER_POS + 1)           /* 1 char @ pos 95 */
#define DATA_ALTITUDE_POS           (DATA_VEHICLE_MODE_POS + 1)         /* 7 chars @ pos 96 */
#define DATA_NETWORK_NAME_POS       (DATA_ALTITUDE_POS + CDAC_ALTITUDE_SIZE) /* 6 chars @ pos 103 */
#define DATA_REGULAR_SIZE           (DATA_NETWORK_NAME_POS + CDAC_NETWORK_NAME_SIZE) /* 109 chars */

/*============================================================================
 * BATCH PACKET (BTH) FIELD POSITIONS
 * Format: BTH<IMEI><Count><Status><GPSFix><DateTime><Lat>...
 *============================================================================*/

#define BTH_HEADER_POS              0                                   /* 3 chars */
#define BTH_IMEI_POS                (BTH_HEADER_POS + CDAC_HEADER_SIZE) /* 15 chars @ pos 3 */
#define BTH_COUNT_POS               (BTH_IMEI_POS + CDAC_IMEI_SIZE)     /* 3 chars @ pos 18 */
#define BTH_STATUS_POS              (BTH_COUNT_POS + CDAC_BATCH_COUNT_SIZE) /* 3 chars @ pos 21: "01L" */
#define BTH_GPS_FIX_POS             (BTH_STATUS_POS + 3)                /* 1 char @ pos 24 */
#define BTH_DATETIME_POS            (BTH_GPS_FIX_POS + 1)               /* 12 chars @ pos 25 */
#define BTH_LATITUDE_POS            (BTH_DATETIME_POS + CDAC_DATETIME_SIZE) /* 10 chars @ pos 37 */
#define BTH_LAT_DIR_POS             (BTH_LATITUDE_POS + CDAC_LAT_SIZE)  /* 1 char @ pos 47 */
#define BTH_LONGITUDE_POS           (BTH_LAT_DIR_POS + 1)               /* 10 chars @ pos 48 */
#define BTH_LON_DIR_POS             (BTH_LONGITUDE_POS + CDAC_LON_SIZE) /* 1 char @ pos 58 */
#define BTH_MCC_POS                 (BTH_LON_DIR_POS + 1)               /* 3 chars @ pos 59 */
#define BTH_MNC_POS                 (BTH_MCC_POS + CDAC_MCC_SIZE)       /* 3 chars @ pos 62 */
#define BTH_LAC_POS                 (BTH_MNC_POS + CDAC_MNC_SIZE)       /* 4 chars @ pos 65 */
#define BTH_CELLID_POS              (BTH_LAC_POS + CDAC_LAC_SIZE)       /* 9 chars @ pos 69 */
#define BTH_SPEED_POS               (BTH_CELLID_POS + CDAC_CELLID_SIZE) /* 6 chars @ pos 78 */
#define BTH_HEADING_POS             (BTH_SPEED_POS + CDAC_SPEED_SIZE)   /* 6 chars @ pos 84 */
#define BTH_SATS_POS                (BTH_HEADING_POS + CDAC_HEADING_SIZE) /* 2 chars @ pos 90 */
#define BTH_HDOP_POS                (BTH_SATS_POS + CDAC_SATS_SIZE)     /* 2 chars @ pos 92 */
#define BTH_SIGNAL_POS              (BTH_HDOP_POS + CDAC_HDOP_SIZE)     /* 2 chars @ pos 94 */
#define BTH_IGN_POS                 (BTH_SIGNAL_POS + CDAC_SIGNAL_SIZE) /* 1 char @ pos 96 */
#define BTH_MAIN_POWER_POS          (BTH_IGN_POS + 1)                   /* 1 char @ pos 97 */
#define BTH_VEHICLE_MODE_POS        (BTH_MAIN_POWER_POS + 1)            /* 1 char @ pos 98 */
#define BTH_ALTITUDE_POS            (BTH_VEHICLE_MODE_POS + 1)          /* 7 chars @ pos 99 */
#define BTH_NETWORK_NAME_POS        (BTH_ALTITUDE_POS + CDAC_ALTITUDE_SIZE) /* 6 chars @ pos 106 */
#define BTH_LIVE_PACKET_SIZE        (BTH_NETWORK_NAME_POS + CDAC_NETWORK_NAME_SIZE) /* 112 chars */

/*============================================================================
 * HEALTH PACKET (HLM) FIELD POSITIONS
 * Format: HLM<VendorID><FirmVer><IMEI><MotionInt><HaltInt><BattPerc>...
 *============================================================================*/

#define HLM_HEADER_POS              0                                   /* 3 chars */
#define HLM_VENDOR_ID_POS           (HLM_HEADER_POS + CDAC_HEADER_SIZE) /* 6 chars @ pos 3 */
#define HLM_FIRMWARE_VER_POS        (HLM_VENDOR_ID_POS + CDAC_VENDOR_ID_SIZE) /* 6 chars @ pos 9 */
#define HLM_IMEI_POS                (HLM_FIRMWARE_VER_POS + CDAC_FIRMWARE_VER_SIZE) /* 15 chars @ pos 15 */
#define HLM_MOTION_INTERVAL_POS     (HLM_IMEI_POS + CDAC_IMEI_SIZE)     /* 3 chars @ pos 30 */
#define HLM_HALT_INTERVAL_POS       (HLM_MOTION_INTERVAL_POS + 3)       /* 3 chars @ pos 33 */
#define HLM_BATT_PERCENT_POS        (HLM_HALT_INTERVAL_POS + 3)         /* 3 chars @ pos 36 */
#define HLM_LOW_BATT_THRS_POS       (HLM_BATT_PERCENT_POS + 3)          /* 2 chars @ pos 39 */
#define HLM_MEMORY_PERCENT_POS      (HLM_LOW_BATT_THRS_POS + 2)         /* 3 chars @ pos 41 */
#define HLM_INPUT1_POS              (HLM_MEMORY_PERCENT_POS + 3)        /* 1 char @ pos 44 */
#define HLM_INPUT2_POS              (HLM_INPUT1_POS + 1)                /* 1 char @ pos 45 */
#define HLM_OUTPUT1_POS             (HLM_INPUT2_POS + 1)                /* 1 char @ pos 46 */
#define HLM_OUTPUT2_POS             (HLM_OUTPUT1_POS + 1)               /* 1 char @ pos 47 */
#define HLM_RESERVED_POS            (HLM_OUTPUT2_POS + 1)               /* 2 chars @ pos 48 */
#define HLM_DATETIME_POS            (HLM_RESERVED_POS + 2)              /* 12 chars @ pos 50 */
#define HLM_PACKET_SIZE             (HLM_DATETIME_POS + CDAC_DATETIME_SIZE) /* 62 chars */

/*============================================================================
 * FULL PACKET (FUL) FIELD POSITIONS
 * Format: FUL<IMEI><AlertID25><Status><GPSFix>...<VendorID><FirmVer><VehReg>...
 *============================================================================*/

#define FUL_HEADER_POS              0                                   /* 3 chars */
#define FUL_IMEI_POS                (FUL_HEADER_POS + CDAC_HEADER_SIZE) /* 15 chars @ pos 3 */
#define FUL_ALERT_ID_POS            (FUL_IMEI_POS + CDAC_IMEI_SIZE)     /* 2 chars @ pos 18: "25" */
#define FUL_STATUS_POS              (FUL_ALERT_ID_POS + CDAC_ALERT_ID_SIZE) /* 1 char @ pos 20 */
#define FUL_GPS_FIX_POS             (FUL_STATUS_POS + 1)                /* 1 char @ pos 21 */
#define FUL_DATETIME_POS            (FUL_GPS_FIX_POS + 1)               /* 12 chars @ pos 22 */
#define FUL_LATITUDE_POS            (FUL_DATETIME_POS + CDAC_DATETIME_SIZE) /* 10 chars @ pos 34 */
#define FUL_LAT_DIR_POS             (FUL_LATITUDE_POS + CDAC_LAT_SIZE)  /* 1 char @ pos 44 */
#define FUL_LONGITUDE_POS           (FUL_LAT_DIR_POS + 1)               /* 10 chars @ pos 45 */
#define FUL_LON_DIR_POS             (FUL_LONGITUDE_POS + CDAC_LON_SIZE) /* 1 char @ pos 55 */
#define FUL_MCC_POS                 (FUL_LON_DIR_POS + 1)               /* 3 chars @ pos 56 */
#define FUL_MNC_POS                 (FUL_MCC_POS + CDAC_MCC_SIZE)       /* 3 chars @ pos 59 */
#define FUL_LAC_POS                 (FUL_MNC_POS + CDAC_MNC_SIZE)       /* 4 chars @ pos 62 */
#define FUL_CELLID_POS              (FUL_LAC_POS + CDAC_LAC_SIZE)       /* 9 chars @ pos 66 */
#define FUL_SPEED_POS               (FUL_CELLID_POS + CDAC_CELLID_SIZE) /* 6 chars @ pos 75 */
#define FUL_HEADING_POS             (FUL_SPEED_POS + CDAC_SPEED_SIZE)   /* 6 chars @ pos 81 */
#define FUL_SATS_POS                (FUL_HEADING_POS + CDAC_HEADING_SIZE) /* 2 chars @ pos 87 */
#define FUL_HDOP_POS                (FUL_SATS_POS + CDAC_SATS_SIZE)     /* 2 chars @ pos 89 */
#define FUL_SIGNAL_POS              (FUL_HDOP_POS + CDAC_HDOP_SIZE)     /* 2 chars @ pos 91 */
#define FUL_IGN_POS                 (FUL_SIGNAL_POS + CDAC_SIGNAL_SIZE) /* 1 char @ pos 93 */
#define FUL_MAIN_POWER_POS          (FUL_IGN_POS + 1)                   /* 1 char @ pos 94 */
#define FUL_VEHICLE_MODE_POS        (FUL_MAIN_POWER_POS + 1)            /* 1 char @ pos 95 */
#define FUL_VENDOR_ID_POS           (FUL_VEHICLE_MODE_POS + 1)          /* 6 chars @ pos 96 */
#define FUL_FIRMWARE_VER_POS        (FUL_VENDOR_ID_POS + CDAC_VENDOR_ID_SIZE) /* 6 chars @ pos 102 */
#define FUL_VEHICLE_REG_POS         (FUL_FIRMWARE_VER_POS + CDAC_FIRMWARE_VER_SIZE) /* 16 chars @ pos 108 */
#define FUL_ALTITUDE_POS            (FUL_VEHICLE_REG_POS + CDAC_VEHICLE_REG_SIZE) /* 7 chars @ pos 124 */
#define FUL_PDOP_POS                (FUL_ALTITUDE_POS + CDAC_ALTITUDE_SIZE) /* 2 chars @ pos 131 */
#define FUL_NETWORK_NAME_POS        (FUL_PDOP_POS + 2)                  /* 6 chars @ pos 133 */
#define FUL_NMR_POS                 (FUL_NETWORK_NAME_POS + CDAC_NETWORK_NAME_SIZE) /* 60 chars @ pos 139 */
#define FUL_NMR_SIZE                60                                  /* 4 cells x (2+4+9) = 60 */
#define FUL_MAIN_VOLTAGE_POS        (FUL_NMR_POS + FUL_NMR_SIZE)        /* 5 chars @ pos 199 */
#define FUL_BATT_VOLTAGE_POS        (FUL_MAIN_VOLTAGE_POS + 5)          /* 5 chars @ pos 204 */
#define FUL_TAMPER_POS              (FUL_BATT_VOLTAGE_POS + 5)          /* 1 char @ pos 209: O/C */
#define FUL_DIG_INPUT1_POS          (FUL_TAMPER_POS + 1)                /* 1 char @ pos 210 */
#define FUL_DIG_INPUT2_POS          (FUL_DIG_INPUT1_POS + 1)            /* 1 char @ pos 211 */
#define FUL_DIG_OUTPUT1_POS         (FUL_DIG_INPUT2_POS + 1)            /* 1 char @ pos 212 */
#define FUL_DIG_OUTPUT2_POS         (FUL_DIG_OUTPUT1_POS + 1)           /* 1 char @ pos 213 */
#define FUL_FRAME_NUM_POS           (FUL_DIG_OUTPUT2_POS + 1)           /* 6 chars @ pos 214 */
#define FUL_CRC_POS                 (FUL_FRAME_NUM_POS + CDAC_FRAME_NUM_SIZE) /* 8 chars @ pos 220 */
#define FUL_PACKET_SIZE             (FUL_CRC_POS + CDAC_CRC_SIZE)       /* 228 chars */

/*============================================================================
 * BATCH PACKET PROCESSING CONSTANTS
 *============================================================================*/

/** Maximum packets per batch */
#define CDAC_MAX_BATCH_PACKETS      2

/** Packet storage types */
#define CDAC_STORAGE_TYPE_NORMAL    0       /* Normal packets (matches NORMAL=0 enum) */
#define CDAC_STORAGE_TYPE_ALERT     3       /* ALERT type - critical/emergency alerts */

/*============================================================================
 * VEHICLE MODE CONSTANTS
 *============================================================================*/

#define CDAC_MODE_MOTION            'M'
#define CDAC_MODE_HALT              'H'
#define CDAC_MODE_SLEEP             'S'

/** Motion detection threshold (km/hr) */
#define CDAC_MOTION_SPEED_THRESHOLD 3.0

/*============================================================================
 * INTERVAL CONSTANTS (in seconds)
 *============================================================================*/

#define CDAC_DEFAULT_MOTION_INTERVAL    10
#define CDAC_DEFAULT_HALT_INTERVAL      60
#define CDAC_DEFAULT_SLEEP_INTERVAL     300
#define CDAC_DEFAULT_EMERGENCY_INTERVAL 5
#define CDAC_DEFAULT_HEALTH_INTERVAL    3600
#define CDAC_DEFAULT_FULL_INTERVAL      86400

/*============================================================================
 * BUFFER SIZES
 *============================================================================*/

#define CDAC_REGULAR_PACKET_SIZE    109     /* Size of regular data portion */
#define CDAC_MAX_PACKET_SIZE        256     /* Maximum packet size for storage */
#define CDAC_LOGIN_PACKET_SIZE      150     /* Login packet buffer size */
#define CDAC_HEALTH_PACKET_SIZE     150     /* Health packet buffer size */

#endif /* PROTO_CDAC */

#endif /* _CDAC_PROTOCOL_H */
