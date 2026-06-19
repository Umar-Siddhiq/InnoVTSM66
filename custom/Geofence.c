#include "Geofence.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "SMS.h"
#include "File.h"

#define CHECK_MASK(var,mask) ((var & mask) == mask)

char tlat[10][20] = {{0}};
char tlng[10][20] = {{0}};
float myPts[10][2] = {{0}};
uint8_t GeoState[10] = {0};
static uint8_t GeoStateInitialized = 0;
extern VTSTypedef VTSData;
char GFAck[50] = {0};

float ratof(char *arr)
{
	float val = 0;
	int afterdot = 0;
	float scale = 1;
	int neg = 0;

	if(*arr == '-')
	{
		arr++;
		neg = 1;
	}
	while(*arr)
	{
		if(afterdot)
		{
			scale = scale / 10;
			val = val + (*arr - '0') * scale;
		}
		else
		{
			if(*arr == '.')
				afterdot++;
			else
				val = val * 10.0F + (*arr - '0');
		}
		arr++;
	}

	return neg ? -val : val;
}

void ClearGeofence(void)
{
	int i;
	int j;

	for(i = 0; i < 10; i++)
	{
		VTSData.GeoLatLng[i].ID = 0;
		VTSData.GeoLatLng[i].InOut = 0;
		VTSData.GeoLatLng[i].AlertInOut = 0;
		GeoState[i] = GEO_STATE_OUTSIDE;
		for(j = 0; j < 10; j++)
		{
			VTSData.GeoLatLng[i].Latitude[j] = 0.0F;
			VTSData.GeoLatLng[i].Longitude[j] = 0.0F;
		}
	}
	GeoStateInitialized = 0;
}

void InitGeoState(void)
{
	for(int i = 0; i < 10; i++)
	{
		GeoState[i] = GEO_STATE_NONE;
	}
	GeoStateInitialized = 0;
	LOGData(TAG_GEO, "GeoState reset - waiting for first GPS fix");
}

uint8_t CheckIfInside(void)
{
	for(int i = 0; i < 10; i++)
	{
		if(GeoState[i] == GEO_STATE_INSIDE)
			return 1;
	}
	return 0;
}

int GetInsideGeofenceID(void)
{
	for(int i = 0; i < 10; i++)
	{
		if(GeoState[i] == GEO_STATE_INSIDE)
			return VTSData.GeoLatLng[i].ID;
	}
	return 0;
}

int CheckGeoFence(double X, double Y, int Fence)
{
	int sides = 0;
	int pointStatus = 0;
	int j;

	for(int n = 0; n < 10; n++)
	{
		if(VTSData.GeoLatLng[Fence].Latitude[n] == 0)
			break;
		sides++;
		myPts[n][0] = VTSData.GeoLatLng[Fence].Latitude[n];
		myPts[n][1] = VTSData.GeoLatLng[Fence].Longitude[n];
	}

	if(sides < 3)
		return 0;

	j = sides - 1;
	for(int i = 0; i < sides; i++)
	{
		if((myPts[i][1] < Y && myPts[j][1] >= Y) || (myPts[j][1] < Y && myPts[i][1] >= Y))
		{
			if(myPts[i][0] + (Y - myPts[i][1]) / (myPts[j][1] - myPts[i][1]) * (myPts[j][0] - myPts[i][0]) < X)
			{
				pointStatus = !pointStatus;
			}
		}
		j = i;
	}

	return pointStatus;
}

static void GeoStatus(double lat, double lng)
{
	int rt;

	for(int i = 0; i < 10; i++)
	{
#ifndef PROTO_CDAC
		if(VTSData.GeoLatLng[i].InOut == 0)
			continue;

		rt = CheckGeoFence(lat, lng, i);
		if(rt)
		{
			if(GeoState[i] == GEO_STATE_OUTSIDE)
			{
				GeoState[i] = GEO_STATE_INSIDE;
				VAlert[GFOUT_ALERT].Enable = 0;
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut, GEO_TRIGGER_IN) && !VAlert[GFIN_ALERT].Enable)
				{
					VAlert[GFIN_ALERT].Enable = 1;
					VAlert[GFIN_ALERT].IsSMS = 1;
					return;
				}
			}
			else
			{
				GeoState[i] = GEO_STATE_INSIDE;
			}
		}
		else
		{
			if(GeoState[i] == GEO_STATE_INSIDE)
			{
				GeoState[i] = GEO_STATE_OUTSIDE;
				VAlert[GFIN_ALERT].Enable = 0;
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut, GEO_TRIGGER_OUT) && !VAlert[GFOUT_ALERT].Enable)
				{
					VAlert[GFOUT_ALERT].Enable = 1;
					VAlert[GFOUT_ALERT].IsSMS = 1;
					return;
				}
			}
			GeoState[i] = GEO_STATE_OUTSIDE;
		}
#else
		if(VTSData.GeoLatLng[i].InOut == 0 || VTSData.GeoLatLng[i].ID == 0)
			continue;

		rt = CheckGeoFence(lat, lng, i);
		if(!GeoStateInitialized)
		{
			GeoState[i] = rt ? GEO_STATE_INSIDE : GEO_STATE_OUTSIDE;
			continue;
		}

		if(rt)
		{
			if(GeoState[i] == GEO_STATE_OUTSIDE)
			{
				GeoState[i] = GEO_STATE_INSIDE;
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut, GEO_TRIGGER_IN) && !VAlert[GFIN_ALERT].Enable)
				{
					VAlert[GFIN_ALERT].Enable = 1;
					VAlert[GFIN_ALERT].WithACK = 1;
					Ql_memset(VAlert[GFIN_ALERT].ACK, 0x00, sizeof(VAlert[GFIN_ALERT].ACK));
					Ql_sprintf(VAlert[GFIN_ALERT].ACK, "%05d", VTSData.GeoLatLng[i].ID);
					AddAlert(GFIN_ALERT);
				}
			}
		}
		else
		{
			if(GeoState[i] == GEO_STATE_INSIDE)
			{
				GeoState[i] = GEO_STATE_OUTSIDE;
				VTSData.VehicleData.OverSpeed = VTSData.VehicleData.DefaultSpeed;
				UpdateConfigInFlash();
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut, GEO_TRIGGER_OUT) && !VAlert[GFOUT_ALERT].Enable)
				{
					VAlert[GFOUT_ALERT].Enable = 1;
					VAlert[GFOUT_ALERT].WithACK = 1;
					Ql_memset(VAlert[GFOUT_ALERT].ACK, 0x00, sizeof(VAlert[GFOUT_ALERT].ACK));
					Ql_sprintf(VAlert[GFOUT_ALERT].ACK, "%05d", VTSData.GeoLatLng[i].ID);
					AddAlert(GFOUT_ALERT);
				}
			}
		}
#endif
	}

	if(!GeoStateInitialized)
		GeoStateInitialized = 1;
}

int StoreGeoData(char *data, int index)
{
	char *fn;
	char ss[100];
	char lat[15];
	double f;
	int ln;
	int count;
	int j;
	int n;
	int p;
	int id;

	fn = Ql_strchr(data, '-');
	if(!fn)
	{
		LOGData(TAG_GEO, "Geo Parse error -1");
		return 0;
	}

	LOGData(TAG_GEO, "Parsing Geo[%d] data: %s", index, data);
	ln = fn - data;
	Ql_strncpy(ss, data, ln);
	ss[ln] = 0;
	id = atoi(ss);
	VTSData.GeoLatLng[index].AlertInOut = 0;
	VTSData.GeoLatLng[index].ID = id;
	fn++;
	VTSData.GeoLatLng[index].InOut = fn[0] - '0';
	LOGData(TAG_GEO, "Geoloc %d : ID: %d, InOut: %d", index, VTSData.GeoLatLng[index].ID, VTSData.GeoLatLng[index].InOut);

	fn = fn + 2;
	p = (int)(fn - data);
	j = Ql_strlen(data) + 1;
	n = 0;
	for(int i = p; i < j; i++)
	{
		if(data[i] == '#' || data[i] == '&' || data[i] == '\0')
		{
			ln = i - p;
			Ql_strncpy(ss, &data[p], ln);
			ss[ln] = 0;
			fn = Ql_strchr(ss, '-');
			if(fn)
			{
				count = fn - ss;
				Ql_strncpy(lat, ss, count);
				lat[count] = 0;
				Ql_sscanf(lat, "%lf", &f);
				VTSData.GeoLatLng[index].Latitude[n] = f;
				fn++;
				Ql_strcpy(lat, fn);
				Ql_sscanf(lat, "%lf", &f);
				VTSData.GeoLatLng[index].Longitude[n] = f;
				n++;
			}
			p = i + 1;
		}
	}

	return id;
}

static int FindGeofenceSlot(int gfId)
{
	for(int i = 0; i < 10; i++)
	{
		if(VTSData.GeoLatLng[i].ID == gfId)
			return i;
	}

	for(int i = 0; i < 10; i++)
	{
		if(VTSData.GeoLatLng[i].ID == 0)
			return i;
	}

	for(int i = 0; i < 9; i++)
	{
		VTSData.GeoLatLng[i] = VTSData.GeoLatLng[i + 1];
		GeoState[i] = GeoState[i + 1];
	}

	VTSData.GeoLatLng[9].ID = 0;
	VTSData.GeoLatLng[9].InOut = 0;
	VTSData.GeoLatLng[9].AlertInOut = 0;
	GeoState[9] = GEO_STATE_OUTSIDE;
	for(int i = 0; i < 10; i++)
	{
		VTSData.GeoLatLng[9].Latitude[i] = 0.0F;
		VTSData.GeoLatLng[9].Longitude[i] = 0.0F;
	}

	return 9;
}

static void ClearGeofenceSlot(int index)
{
	if(index < 0 || index >= 10)
		return;

	VTSData.GeoLatLng[index].ID = 0;
	VTSData.GeoLatLng[index].InOut = 0;
	VTSData.GeoLatLng[index].AlertInOut = 0;
	GeoState[index] = GEO_STATE_OUTSIDE;
	for(int j = 0; j < 10; j++)
	{
		VTSData.GeoLatLng[index].Latitude[j] = 0.0F;
		VTSData.GeoLatLng[index].Longitude[j] = 0.0F;
	}
}

void DecodeGeofence(char *data)
{
	char *fn;
	char ss[200] = {0};
	char *start;
	char *end;
	int gfCount = 0;

	if(Ql_strstr(data, "GF:CLR"))
	{
		ClearGeofence();
		UpdateConfigInFlash();
#ifdef PROTO_CDAC
		VAlert[CONF_CHANGE_ALERT].Enable = 1;
		Ql_strcpy(VAlert[CONF_CHANGE_ALERT].Header, "ACK");
		Ql_memset(VAlert[CONF_CHANGE_ALERT].ACK, 0x00, sizeof(VAlert[CONF_CHANGE_ALERT].ACK));
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "GF:Cleared*");
		VAlert[CONF_CHANGE_ALERT].WithACK = 1;
		AddAlert(CONF_CHANGE_ALERT);
#endif
		return;
	}

	fn = Ql_strstr(data, "GF:");
	if(!fn)
		return;

	start = fn + 3;
#ifdef PROTO_CDAC
	VAlert[CONF_CHANGE_ALERT].Enable = 1;
	Ql_strcpy(VAlert[CONF_CHANGE_ALERT].Header, "ACK");
	Ql_memset(VAlert[CONF_CHANGE_ALERT].ACK, 0x00, sizeof(VAlert[CONF_CHANGE_ALERT].ACK));
	SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "GF:");
#endif

	while(*start && gfCount < 10)
	{
		int len;
		int slotIndex;
		int id;
		char *dashPos;
		int gfId = 0;

		end = Ql_strchr(start, '&');
		len = end ? (int)(end - start) : (int)Ql_strlen(start);
		if(len <= 0 || len >= (int)sizeof(ss))
		{
			if(!end)
				break;
			start = end + 1;
			continue;
		}

		Ql_memset(ss, 0x00, sizeof(ss));
		Ql_strncpy(ss, start, len);
		ss[len] = 0;
		dashPos = Ql_strchr(ss, '-');
		if(dashPos)
		{
			char idStr[10] = {0};
			int idLen = (int)(dashPos - ss);
			if(idLen > 0 && idLen < (int)sizeof(idStr))
			{
				Ql_strncpy(idStr, ss, idLen);
				idStr[idLen] = 0;
				gfId = atoi(idStr);
			}
		}

		slotIndex = FindGeofenceSlot(gfId);
		ClearGeofenceSlot(slotIndex);
		id = StoreGeoData(ss, slotIndex);
		if(id > 0)
		{
#ifdef PROTO_CDAC
			char cc[12];
			Ql_sprintf(cc, "%d", id);
			SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, cc);
			if(end && *(end + 1))
				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "#");
#endif
			gfCount++;
		}

		if(!end)
			break;
		start = end + 1;
	}

#ifdef PROTO_CDAC
	if(gfCount > 0)
	{
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "*");
		VAlert[CONF_CHANGE_ALERT].WithACK = 1;
		AddAlert(CONF_CHANGE_ALERT);
		IsPacketReady.IsNormalPacket = 1;
	}
#endif

	UpdateConfigInFlash();
}

void UpdateGeoFence(void)
{
	if(!GPS.GPSFix)
		return;
	GeoStatus(GPS.Latitude, GPS.Longitude);
}

#ifdef PROTO_CDAC
void GetActiveGeoID(char *buff)
{
	int count = 0;

	for(int i = 0; i < 10; i++)
	{
		if(VTSData.GeoLatLng[i].ID != 0)
		{
			char cc[12];
			if(count > 0)
				InsertChar(buff, '#');
			Ql_sprintf(cc, "%d", VTSData.GeoLatLng[i].ID);
			Ql_strcat(buff, cc);
			count++;
		}
	}

	if(count == 0)
		Ql_strcpy(buff, "None");
}
#endif

#if defined(PROTO_MAHARASHTRA1) || defined(PROTO_OG)
uint8_t DecodeGeofenceSR(char *data)
{
	int i;
	uint8_t pointCount;
	uint8_t index;

	i = GetValueFromData(data, "PGF", '#', 0, ',', tlat[0]);
	if(!i)
		return 0;
	if(Ql_strlen(tlat[0]) > 12 || Ql_strlen(tlat[0]) < 5)
		return 0;
	LOGData(TAG_GEO, "SR GeoFence Decode 0 %s", tlat[0]);

	i = GetValueFromData(data, "PGF", ',', 1, ',', tlng[0]);
	if(!i)
		return 0;
	if(Ql_strlen(tlng[0]) > 12 || Ql_strlen(tlng[0]) < 5)
		return 0;
	LOGData(TAG_GEO, "SR GeoFence Decode 1 %s", tlng[0]);

	index = 2;
	pointCount = 1;
	for(int j = 1; j < 10; j++)
	{
		i = GetValueFromData(data, "PGF", ',', index, ',', tlat[j]);
		if(!i)
			break;
		if(Ql_strlen(tlat[j]) > 12 || Ql_strlen(tlat[j]) < 5)
			break;
		LOGData(TAG_GEO, "SR GeoFence Decode %d %s", index, tlat[j]);
		index++;

		i = GetValueFromData(data, "PGF", ',', index, ',', tlng[j]);
		if(!i)
		{
			i = GetValueFromData(data, "PGF", ',', index, ';', tlng[j]);
			if(!i)
				return 0;
			LOGData(TAG_GEO, "SR GeoFence Decode %d %s", index, tlng[j]);
			index++;
			pointCount++;
			break;
		}

		if(Ql_strlen(tlng[j]) > 12 || Ql_strlen(tlng[j]) < 5)
			break;

		LOGData(TAG_GEO, "SR GeoFence Decode %d %s", index, tlng[j]);
		index++;
		pointCount++;
	}

	if(pointCount < 3)
		return 0;

	for(int k = 0; k < pointCount; k++)
	{
		Ql_sscanf(tlat[k], "%lf", &VTSData.GeoLatLng[0].Latitude[k]);
		Ql_sscanf(tlng[k], "%lf", &VTSData.GeoLatLng[0].Longitude[k]);
	}
	VTSData.GeoLatLng[0].ID = 2233;
	VTSData.GeoLatLng[0].InOut = GEO_TRIGGER_IN | GEO_TRIGGER_OUT;
	UpdateConfigInFlash();
	return 1;
}
#else
uint8_t DecodeGeofenceSR(char *data)
{
	(void)data;
	return 0;
}
#endif