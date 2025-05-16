/*seems this code originated from https://github.com/sp6q/maidenhead */
#include "maidenhead.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

char letterize(int x) {
	if (x<24)
    return (char) x + 65;  
	else
	return (char) 23 + 65; /*KC3LBR 07/23/24   an alternate/redundant fix to the one below, this clamps the returned characters at 'X' or lower. The original code sometimes returned a Y for 5th or 6 char, which is invalid*/
}

char* get_mh(double lat, double lon, int size) {
    static char locator[11];
    double LON_F[]={20,2.0,0.0833333333,0.008333333,0.0003472222222222}; /*KC3LBR 07/23/24   increased resolution of 1/12 constant to prevent problems*/
    double LAT_F[]={10,1.0,0.0416666667,0.004166666,0.0001736111111111}; /*KC3LBR 07/23/24   increased resolution of 1/24 constant to prevent problems*/
    int i;
    lon += 180;
    lat += 90;

    if (size <= 0 || size > 10) size = 6;
    size/=2; size*=2;

    for (i = 0; i < size/2; i++){             //0,1,2
        if (i % 2 == 1) {                      //odd i's         
            locator[i*2] = (char) (lon/LON_F[i] + '0');         //       2,3
            locator[i*2+1] = (char) (lat/LAT_F[i] + '0');
        } else {							   //even i's        
			if (i==0)
			{
				locator[i*2] = letterize((int) (lon/LON_F[i]));     //0,1            4,5
				locator[i*2+1] = letterize((int) (lat/LAT_F[i]));
			}
				else      //The last 2 chars of a 6 char grid must be lowercase??,prolly makes no difference
		    {
				locator[i*2] = letterize((int) (lon/LON_F[i]));     //0,1            4,5
				locator[i*2+1] = letterize((int) (lat/LAT_F[i]));
			}      //technically all chars should be uppercase, these previous 2 lines not needed
			//april 2 2024 removed the +32 from previous 2 lines, YEs, they should ALWAYS be uppercase. otherwise casued problems with U4B code
					
        }
        lon = fmod(lon, LON_F[i]);
        lat = fmod(lat, LAT_F[i]);
    }
    locator[i*2]=0;
    return locator;
}

char* complete_mh(char* locator) {
    static char locator2[11] = "LL55LL55LL";
    int len = strlen(locator);
    if (len >= 10) return locator;
    memcpy(locator2, locator, strlen(locator));
    return locator2;
}

double mh2lon(char* locator) {
    double field, square, subsquare, extsquare, precsquare;
    int len = strlen(locator);
    if (len > 10) return 0;
    if (len < 10) locator = complete_mh(locator);
    field      = (locator[0] - 'A') * 20.0;
    square     = (locator[2] - '0') * 2.0;
    subsquare  = (locator[4] - 'A') / 12.0;
    extsquare  = (locator[6] - '0') / 120.0;
    precsquare = (locator[8] - 'A') / 2880.0;
    return field + square + subsquare + extsquare + precsquare - 180;
}

double mh2lat(char* locator) {
    double field, square, subsquare, extsquare, precsquare;
    int len = strlen(locator);
    if (len > 10) return 0;
    if (len < 10) locator = complete_mh(locator);
    field      = (locator[1] - 'A') * 10.0;
    square     = (locator[3] - '0') * 1.0;
    subsquare  = (locator[5] - 'A') / 24.0;
    extsquare  = (locator[7] - '0') / 240.0;
    precsquare = (locator[9] - 'A') / 5760.0;
    return field + square + subsquare + extsquare + precsquare - 90;
}

#ifdef __cplusplus
}
#endif
