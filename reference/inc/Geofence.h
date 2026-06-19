

#ifndef					_GEOFENCE_H
#define					_GEOFENCE_H

#include "project.h"
#include "VTS.h"

#define GEO_TRIGGER_IN		0x01
#define GEO_TRIGGER_OUT		0x02

#define GEO_STATE_INSIDE    0x2
#define GEO_STATE_OUTSIDE   0x1
#define GEO_STATE_NONE      0x0


//#define	GEO_OUT_SEND	0x08




uint8_t CheckIfInside(void);
int GetInsideGeofenceID(void);
void ClearGeofence(void);
void InitGeoState(void);
//int CheckGeoFence(double X, double Y, int Fence);
void DecodeGeofence(char* data);
//void GeoStatus(double lat, double lng);
int StoreGeoData(char* data, int index);
void GetActiveGeoID(char* buff);
void UpdateGeoFence(void);


uint8_t DecodeGeofenceSR(char *data);


#endif				//_GEOFENCE_H
