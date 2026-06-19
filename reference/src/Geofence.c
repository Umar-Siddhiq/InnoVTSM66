

#include "Geofence.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CHECK_MASK(var,mask)   ((var&mask)==mask)
char tlat[10][20],tlng[10][20];
float myPts[10][2];
uint8_t GeoState[10];
static uint8_t GeoStateInitialized = 0;  // Flag to skip alerts on first position update
extern VTSTypedef VTSData;
char GFAck[50];

float ratof(char *arr)
{
  float val = 0;
  int afterdot=0;
  float scale=1;
  int neg = 0; 

  if (*arr == '-') {
    arr++;
    neg = 1;
  }
  while (*arr) {
    if (afterdot) {
      scale = scale/10;
      val = val + (*arr-'0')*scale;
    } else {
      if (*arr == '.') 
    afterdot++;
      else
    val = val * 10.0 + (*arr - '0');
    }
    arr++;
  }
  if(neg) return -val;
  else    return  val;
}

void ClearGeofence(void)
{
	int i, j;
	for(i=0;i<10;i++)
	{
		VTSData.GeoLatLng[i].ID=0;
		VTSData.GeoLatLng[i].InOut=0;
		VTSData.GeoLatLng[i].AlertInOut=0;
		GeoState[i] = GEO_STATE_OUTSIDE;  // Reset state to prevent false alerts
		for(j=0;j<10;j++)
		{
			VTSData.GeoLatLng[i].Latitude[j]=0.0F;
			VTSData.GeoLatLng[i].Longitude[j]=0.0F;  // Fixed: was [i], should be [j]
		}
	}
}

/**
 * @brief Initialize GeoState array after boot
 * Call this after loading config from flash to ensure geofence state tracking works
 * Sets all states to OUTSIDE so first position update will properly detect IN transitions
 */
void InitGeoState(void)
{
	for(int i = 0; i < 10; i++)
	{
		GeoState[i] = GEO_STATE_NONE;  // Use NONE so first GPS fix determines actual state
	}
	GeoStateInitialized = 0;  // Reset flag - first position update will set actual states without alerts
	nwy_dbg_log("GeoState reset - will initialize from GPS on first fix");
}

uint8_t CheckIfInside(void)
{
	for(int i=0;i<10;i++)
	{
		if(GeoState[i] == GEO_STATE_INSIDE)
			return 1;
	}
	return 0;
}

// Returns the GF ID of the first geofence the device is inside, or 0 if none
int GetInsideGeofenceID(void)
{
	for(int i=0;i<10;i++)
	{
		if(GeoState[i] == GEO_STATE_INSIDE)
			return VTSData.GeoLatLng[i].ID;
	}
	return 0;
}

int CheckGeoFence(double X, double Y, int Fence)
{
	int sides = 0;//No of Points
	int j;
	int pointStatus = 0;
	int vl=Fence;
	int n;
	
	// First, count the sides and copy points
	for(n=0;n<10;n++)
	{
		if(VTSData.GeoLatLng[vl].Latitude[n]==0)
			break;
		sides++;
		myPts[n][0]=VTSData.GeoLatLng[vl].Latitude[n];
		myPts[n][1]=VTSData.GeoLatLng[vl].Longitude[n];
	}
	
	// Need at least 3 points for a valid polygon
	if(sides < 3)
		return 0;
	
	// Initialize j to last point index for polygon wraparound
	j = sides - 1;
	
	// Ray casting algorithm for point-in-polygon
	for (int i = 0; i < sides; i++)
	{
		if ((myPts[i][1] < Y && myPts[j][1] >= Y) || (myPts[j][1] < Y && myPts[i][1] >= Y))
		{
			if (myPts[i][0] + (Y - myPts[i][1]) / (myPts[j][1] - myPts[i][1]) * (myPts[j][0] - myPts[i][0]) < X)
			{
				pointStatus = !pointStatus;
			}
		}
		j = i;
	}
	return pointStatus;
}

void GeoStatus(double lat, double lng)
{
	int i;
	int rt=0;
	for(i =0; i< 10; i++)
	{   
		#ifndef PROTO_CDAC
        if(VTSData.GeoLatLng[i].InOut == 0)
            continue;
    
		rt=CheckGeoFence(lat,lng,i);
		//VTSData.GeoLatLng[i].AlertInOut=0;
		if(rt)
		{
			if(GeoState[i] == GEO_STATE_OUTSIDE)
			{
				GeoState[i] = GEO_STATE_INSIDE;
				VAlert[GFOUT_ALERT].Enable=0;
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut,GEO_TRIGGER_IN))
				{
					
					if(VAlert[GFIN_ALERT].Enable==0)
					{
						VAlert[GFIN_ALERT].Enable=1;
						VAlert[GFIN_ALERT].IsSMS=1;
						return;
					}
				}
			}
			else
				GeoState[i] = GEO_STATE_INSIDE;
			//}
		}
		else
		{
			if(GeoState[i] == GEO_STATE_INSIDE)
			{
				GeoState[i] = GEO_STATE_OUTSIDE;
				VAlert[GFIN_ALERT].Enable=0;
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut,GEO_TRIGGER_OUT))
				{
					if(!VAlert[GFOUT_ALERT].Enable)
					{
						VAlert[GFOUT_ALERT].Enable=1;
						VAlert[GFOUT_ALERT].IsSMS=1;
						
						return;
					}
				}
				
			}
			GeoState[i] = GEO_STATE_OUTSIDE;
		}	

		#else
		// CDAC Protocol geofence handling
		// Skip if geofence not configured (InOut == 0 means no alert type set)
		if(VTSData.GeoLatLng[i].InOut == 0)
            continue;
		
		// Check if ID is valid (must be non-zero)
		if(VTSData.GeoLatLng[i].ID == 0)
			continue;
		
		rt=CheckGeoFence(lat,lng,i);
		
		// First run after boot: just set initial state without triggering alerts
		if(!GeoStateInitialized)
		{
			GeoState[i] = rt ? GEO_STATE_INSIDE : GEO_STATE_OUTSIDE;
			nwy_dbg_log("GF[%d] ID=%d: Initial state = %s", 
				i, VTSData.GeoLatLng[i].ID, rt ? "INSIDE" : "OUTSIDE");
			continue;  // Skip alert logic on first run
		}
		
		if(rt)
		{
			// Currently INSIDE this geofence
			if(GeoState[i] == GEO_STATE_OUTSIDE)
			{
				// Transition: OUTSIDE -> INSIDE
				nwy_dbg_log("GF[%d] ID=%d: OUTSIDE->INSIDE (AlertType=%d)", 
					i, VTSData.GeoLatLng[i].ID, VTSData.GeoLatLng[i].InOut);
				GeoState[i] = GEO_STATE_INSIDE;
				
				// Check if IN alert is enabled for this geofence (Type 1 or 3)
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut, GEO_TRIGGER_IN))
				{
					// Only trigger if not already pending
					if(VAlert[GFIN_ALERT].Enable == 0)
					{
						VAlert[GFIN_ALERT].Enable = 1;
						VAlert[GFIN_ALERT].WithACK = 1;
						memset(VAlert[GFIN_ALERT].ACK, 0x00, 17);
						sprintf(VAlert[GFIN_ALERT].ACK, "%05d", VTSData.GeoLatLng[i].ID);
						AddAlert(GFIN_ALERT);
						nwy_dbg_log("GFIN Alert triggered for ID=%d", VTSData.GeoLatLng[i].ID);
					}
				}
				else
				{
					nwy_dbg_log("GF[%d] IN event but IN alert not enabled (type=%d)", 
						i, VTSData.GeoLatLng[i].InOut);
				}
				// Don't return - continue checking other geofences
			}
		}
		else
		{
			// Currently OUTSIDE this geofence
			if(GeoState[i] == GEO_STATE_INSIDE)
			{
				// Transition: INSIDE -> OUTSIDE
				nwy_dbg_log("GF[%d] ID=%d: INSIDE->OUTSIDE (AlertType=%d)", 
					i, VTSData.GeoLatLng[i].ID, VTSData.GeoLatLng[i].InOut);
				GeoState[i] = GEO_STATE_OUTSIDE;

				// DSL replaces SL on geo exit REGARDLESS of alert type (per spec)
				// This happens for alert type 1, 2, or 3
				nwy_dbg_log("Geo EXIT: Restoring SL from DSL (%.1f -> %.1f)", 
					VTSData.VehicleData.OverSpeed, VTSData.VehicleData.DefaultSpeed);
				VTSData.VehicleData.OverSpeed = VTSData.VehicleData.DefaultSpeed;
				UpdateConfigInFlash();
				
				// Check if OUT alert is enabled for this geofence (Type 2 or 3)
				if(CHECK_MASK(VTSData.GeoLatLng[i].InOut, GEO_TRIGGER_OUT))
				{
					// Only trigger if not already pending
					if(VAlert[GFOUT_ALERT].Enable == 0)
					{
						VAlert[GFOUT_ALERT].Enable = 1;
						VAlert[GFOUT_ALERT].WithACK = 1;
						memset(VAlert[GFOUT_ALERT].ACK, 0x00, 17);
						sprintf(VAlert[GFOUT_ALERT].ACK, "%05d", VTSData.GeoLatLng[i].ID);
						AddAlert(GFOUT_ALERT);
						nwy_dbg_log("GFOUT Alert triggered for ID=%d", VTSData.GeoLatLng[i].ID);
					}
				}
				else
				{
					nwy_dbg_log("GF[%d] OUT event but OUT alert not enabled (type=%d)", 
						i, VTSData.GeoLatLng[i].InOut);
				}
				// Don't return - continue checking other geofences
			}
		}
		#endif
	}
	
	// After first complete pass, mark as initialized so subsequent calls trigger alerts
	if(!GeoStateInitialized)
	{
		GeoStateInitialized = 1;
		nwy_dbg_log("GeoState initialization complete - alerts now enabled");
	}
}


int StoreGeoData(char* data, int index)
{
	char* fn;
	int ln,count, i,j,n,p, id;
	char ss[100];
	char lat[15];
	double f;
	fn=strchr(data,'-');
    if(!fn)
    {
        nwy_dbg_log("Geo Parse error -1");
        return 0;
    }
    nwy_dbg_log("Parsing Geo[%d] data: %s",index,data);
	ln=fn-data;
	strncpy(ss,data,ln);
	ss[ln]=0;
	ln=atoi(ss);
	VTSData.GeoLatLng[index].AlertInOut=0;
	VTSData.GeoLatLng[index].ID=ln;
	id = ln;
	fn++;
	VTSData.GeoLatLng[index].InOut=fn[0] - '0';
    nwy_dbg_log("Geoloc %d : ID: %d, InOut: %d",index,VTSData.GeoLatLng[index].ID,VTSData.GeoLatLng[index].InOut);
	count=0;
	fn=fn+2;  // Skip type and '-' to point to first lat
	p = fn - data;  // Start position for parsing lat-lng pairs
	j=strlen(data)+1;
	n=0;
	for(i=p;i<j;i++)
	{
		if((data[i]=='#') || (data[i]=='&') || (data[i]=='\0' && data[i]!='&'))
		{
			ln=i-p;
			strncpy(ss,&data[p],ln);			// "22341-2-008.510081-076.960810#008.510654-076.961336#008.510145-076.961915#008.509651-076.961084&";
			ss[ln]=0;
            nwy_dbg_log("%c found, i %d, len %d, data: %s",data[i],i,ln,ss);
			fn=strchr(ss,'-');
			if(fn)
			{
				count=fn-ss;
				strncpy(lat,ss,count);
				//f=atof(lat);
                sscanf(lat,"%lf",&f);
				VTSData.GeoLatLng[index].Latitude[n]=f;
				fn++;
				strcpy(lat,fn);
				//f=ratof(lat);
                sscanf(lat,"%lf",&f);
				VTSData.GeoLatLng[index].Longitude[n]=f;
                nwy_dbg_log("Geoloc %d : Lat[%d]: %.6f, Long[%d]: %.6f",index,n,VTSData.GeoLatLng[index].Latitude[n],n,VTSData.GeoLatLng[index].Longitude[n]);
				n++;
                
			}
			count=i;
			p=i+1;
		}
	}
	return id;
}

/**
 * @brief Find a slot index for a geofence ID
 * First checks if the ID already exists (to update it)
 * Then finds an empty slot (ID=0)
 * If all slots are full, uses ring buffer (overwrites oldest = slot 0, shifts others)
 * @param gfId The geofence ID to find/allocate slot for
 * @return Slot index (0-9)
 */
static int FindGeofenceSlot(int gfId)
{
	int i;
	int emptySlot = -1;
	
	// First pass: check if this ID already exists (update case)
	for(i = 0; i < 10; i++) {
		if(VTSData.GeoLatLng[i].ID == gfId) {
			nwy_dbg_log("GF slot %d: updating existing ID %d", i, gfId);
			return i;
		}
	}
	
	// Second pass: find first empty slot
	for(i = 0; i < 10; i++) {
		if(VTSData.GeoLatLng[i].ID == 0) {
			nwy_dbg_log("GF slot %d: using empty slot for ID %d", i, gfId);
			return i;
		}
	}
	
	// All slots full - use ring buffer (overwrite slot 0, shift others)
	nwy_dbg_log("GF slots full, ring buffer: overwriting oldest for ID %d", gfId);
	
	// Shift all geofences down by 1 (slot 1->0, 2->1, etc.)
	for(i = 0; i < 9; i++) {
		VTSData.GeoLatLng[i] = VTSData.GeoLatLng[i+1];
		GeoState[i] = GeoState[i+1];
	}
	
	// Clear the last slot for the new geofence
	VTSData.GeoLatLng[9].ID = 0;
	VTSData.GeoLatLng[9].InOut = 0;
	VTSData.GeoLatLng[9].AlertInOut = 0;
	GeoState[9] = GEO_STATE_OUTSIDE;
	for(i = 0; i < 10; i++) {
		VTSData.GeoLatLng[9].Latitude[i] = 0.0F;
		VTSData.GeoLatLng[9].Longitude[i] = 0.0F;
	}
	
	return 9;  // New geofence goes in last slot
}

/**
 * @brief Clear a single geofence slot
 */
static void ClearGeofenceSlot(int index)
{
	int j;
	if(index < 0 || index >= 10) return;
	
	VTSData.GeoLatLng[index].ID = 0;
	VTSData.GeoLatLng[index].InOut = 0;
	VTSData.GeoLatLng[index].AlertInOut = 0;
	GeoState[index] = GEO_STATE_OUTSIDE;
	for(j = 0; j < 10; j++) {
		VTSData.GeoLatLng[index].Latitude[j] = 0.0F;
		VTSData.GeoLatLng[index].Longitude[j] = 0.0F;
	}
}

void DecodeGeofence(char* data) // SET GF:12345-1-008.510081-076.960810#008.510654-076.961336&67890-3-...&
{
	char* fn;
	char ss[200]={0};
	int id;
	int gfCount = 0;
	int slotIndex;
	
	// Note: No ClearGeofence() here - ring buffer method adds to existing
	
	if(strstr(data,"GF:CLR"))
	{
		ClearGeofence();  // Only clear all when explicitly requested
		nwy_dbg_log("Geofence Cleared!");
		UpdateConfigInFlash();  // Save cleared geofences to flash
		#ifdef PROTO_CDAC
		VAlert[CONF_CHANGE_ALERT].Enable=1;
		// ACK header for OTA Parameter Acknowledgment per CDAC spec 6.6
		strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
		memset(VAlert[CONF_CHANGE_ALERT].ACK,0x00,100);
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "GF:Cleared*");
		VAlert[CONF_CHANGE_ALERT].WithACK=1;
		AddAlert(CONF_CHANGE_ALERT);
		#endif
		return;
	}
	
	fn = strstr(data,"GF:");
	if(!fn)
		return;
	
	fn = fn + 3;  // Skip "GF:"
	nwy_dbg_log("Parsing geofences: %s", fn);
	
	#ifdef PROTO_CDAC
	VAlert[CONF_CHANGE_ALERT].Enable=1;
	strcpy(VAlert[CONF_CHANGE_ALERT].Header,"ACK");
	memset(VAlert[CONF_CHANGE_ALERT].ACK,0x00,100);
	SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "GF:");
	#endif
	
	// Parse each geofence separated by '&'
	// Format: id-type-lat-lng#lat-lng#...&id-type-lat-lng#...&
	char* start = fn;
	char* end;
	
	while(*start && gfCount < 10)
	{
		// Find the end of this geofence (next '&' or end of string)
		end = strchr(start, '&');
		
		int len;
		if(end) {
			len = end - start;
		} else {
			len = strlen(start);
			if(len == 0) break;
		}
		
		// Copy this geofence data
		if(len > 0 && len < sizeof(ss)-1) {
			memset(ss, 0, sizeof(ss));
			strncpy(ss, start, len);
			ss[len] = '\0';
			
			// Extract geofence ID first to find appropriate slot
			char* dashPos = strchr(ss, '-');
			int gfId = 0;
			if(dashPos) {
				char idStr[10] = {0};
				int idLen = dashPos - ss;
				if(idLen > 0 && idLen < 10) {
					strncpy(idStr, ss, idLen);
					gfId = atoi(idStr);
				}
			}
			
			// Find slot using ring buffer method
			slotIndex = FindGeofenceSlot(gfId);
			nwy_dbg_log("Geofence ID %d -> slot %d: %s", gfId, slotIndex, ss);
			
			// Clear the slot before storing new data
			ClearGeofenceSlot(slotIndex);
			
			id = StoreGeoData(ss, slotIndex);
			
			if(id > 0) {
				#ifdef PROTO_CDAC
				char cc[12];
				sprintf(cc, "%d", id);
				SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, cc);
				if(end && *(end+1)) {  // More geofences to come
					SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "#");
				}
				#endif
				gfCount++;
			}
		}
		
		// Move to next geofence
		if(end) {
			start = end + 1;
		} else {
			break;
		}
	}
	
	nwy_dbg_log("Total geofences parsed: %d", gfCount);
	
	#ifdef PROTO_CDAC
	if(gfCount > 0) {
		SafeAppendACK(VAlert[CONF_CHANGE_ALERT].ACK, "*");
		VAlert[CONF_CHANGE_ALERT].WithACK=1;
		AddAlert(CONF_CHANGE_ALERT);
		IsPacketReady.IsNormalPacket=1;
		nwy_dbg_log("GF ACK: %s", VAlert[CONF_CHANGE_ALERT].ACK);
	}
	#endif
	
	UpdateConfigInFlash();  // Save geofences to flash
}

void UpdateGeoFence(void)
{
	if(!GPS.GPSFix)
		return;
	GeoStatus(GPS.Latitude,GPS.Longitude);
	
	
}
#ifdef PROTO_CDAC
void GetActiveGeoID(char* buff)
{
	int count = 0;

	for(int i = 0; i < 10; i++)
	{
		if(VTSData.GeoLatLng[i].ID != 0)
		{
			// Add separator before ID (except for first one)
			if(count > 0)
				InsertChar(buff, '#');
			
			char cc[12];
			sprintf(cc, "%d", VTSData.GeoLatLng[i].ID);  // Fixed: was using [count], should be [i]
			strcat(buff, cc);
			count++;
		}
	}
	
	// If no geofences, return "None"
	if(count == 0)
		strcpy(buff, "None");
}
#endif
#ifdef PROTO_MAHARASHTRA1
uint8_t DecodeGeofenceSR(char *data)
{
	int i;
	uint8_t pointCount;
	uint8_t index;

	i = GetValueFromData(data,"PGF",'#',0,',',tlat[0]);
	if(!i)
		return 0;
	if(strlen(tlat[0]) > 12 || strlen(tlat[0]) < 5)
		return 0;
	nwy_dbg_log("SR GeoFence Decode 0 %s",tlat[0]);
	i = GetValueFromData(data,"PGF",',',1,',',tlng[0]);
	if(!i)
		return 0;
	if(strlen(tlng[0]) > 12 || strlen(tlng[0]) < 5)
		return 0;
	nwy_dbg_log("SR GeoFence Decode 1 %s",tlng[0]);
	index = 2;
	pointCount=1;
	for(int j = 1; j < 10; j++)
	{
		i = GetValueFromData(data,"PGF",',',index,',',tlat[j]);
		if(!i)
			break;
		if(strlen(tlat[j]) > 12 || strlen(tlat[j]) < 5)
			break;
		nwy_dbg_log("SR GeoFence Decode %d %s",index,tlat[j]);
		index++;

		i = GetValueFromData(data,"PGF",',',index,',',tlng[j]);
		if(!i)
		{
			i = GetValueFromData(data,"PGF",',',index,';',tlng[j]);
			if(!i)
				return 0;
			nwy_dbg_log("SR GeoFence Decode %d %s",index,tlng[j]);
			index++;
			pointCount++;
			break;
		}
		
		if(strlen(tlng[j]) > 12 || strlen(tlng[j]) < 5)
			break;

		nwy_dbg_log("SR GeoFence Decode %d %s",index,tlng[j]);
		index++;
		pointCount++;
	}
	if(pointCount < 3)
		return 0;

	nwy_dbg_log("SR GeoFence Decode Complete, points : %d",pointCount);
	for(int k = 0; k < pointCount; k++)
	{
		sscanf(tlat[k],"%lf",&VTSData.GeoLatLng[0].Latitude[k]);
		sscanf(tlng[k],"%lf",&VTSData.GeoLatLng[0].Longitude[k]);
	}
	VTSData.GeoLatLng[0].ID = 2233;
	VTSData.GeoLatLng[0].InOut = GEO_TRIGGER_IN | GEO_TRIGGER_OUT;
	UpdateConfigInFlash();
	return 1;

}

#endif



