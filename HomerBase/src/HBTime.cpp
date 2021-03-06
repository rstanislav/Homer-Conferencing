/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: Implementation of wrapper for os independent time handling
 * Author:  Thomas Volkert
 * Since:   2010-09-23
 */

#include <Logger.h>
#include <HBTime.h>

#include <errno.h>
#include <time.h>

#if defined(LINUX) || defined(APPLE) || defined(BSD)
#include <sys/time.h>
#endif

#include <Header_Windows.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Time::Time()
{
    mTimeStamp = 0;
}

Time::~Time()
{
}

///////////////////////////////////////////////////////////////////////////////

Time& Time::operator=(const Time &pTime)
{
	// only do assignment if pTime is a different object from this
	if (this != &pTime)
	{
		mTimeStamp = pTime.mTimeStamp;
	}
	return *this;
}

bool Time::ValidTimeStamp()
{
    return (mTimeStamp != 0);
}

void Time::InvalidateTimeStamp()
{
    mTimeStamp = 0;
}

int64_t Time::GetTimeStamp()
{
	int64_t tResult = 0;
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		struct timeval tTimeVal;
		gettimeofday(&tTimeVal, 0);
		tResult = (int64_t)1000 * 1000 * tTimeVal.tv_sec + tTimeVal.tv_usec;
	#endif

	#ifdef WIN32
		struct _timeb tTimeVal;
		#ifdef __MINGW32__
			_ftime(&tTimeVal);
		#else
			_ftime64_s(&tTimeVal);
		#endif
		tResult = (int64_t)1000 * 1000 * tTimeVal.time + tTimeVal.millitm * 1000;
	#endif

	return tResult;
}

int64_t Time::UpdateTimeStamp()
{
	mTimeStamp = GetTimeStamp();
    return mTimeStamp;
}

int64_t Time::TimeDiffInUSecs(Time *pTime)
{
    return ((int64_t)(mTimeStamp - pTime->mTimeStamp));
}

bool Time::GetNow(int *pDay, int *pMonth, int *pYear, int *pHour, int *pMin, int *pSec)
{
    bool tResult = false;
    time_t tTimeStamp = GetTimeStamp() / 1000000;
    struct tm tLocalTimeData;

    #if defined(LINUX) || defined(APPLE) || defined(BSD)
        struct tm* tLocalTime = localtime_r(&tTimeStamp, &tLocalTimeData);
        if (tLocalTime == NULL)
            LOGEX(Time, LOG_ERROR, "Error while determining current time");
        else
            tResult = true;
    #endif
    #ifdef WIN32
		#ifdef __MINGW32__
			struct tm* tTm = localtime(&tTimeStamp);
			tLocalTimeData.tm_mday = tTm->tm_mday;
			tLocalTimeData.tm_mon = tTm->tm_mon;
			tLocalTimeData.tm_year = tTm->tm_year;
			tLocalTimeData.tm_hour = tTm->tm_hour;
			tLocalTimeData.tm_min = tTm->tm_min;
			tLocalTimeData.tm_sec = tTm->tm_sec;
		#else
	        errno_t tErr = localtime_s(&tLocalTimeData, &tTimeStamp);
	        if (tErr != 0)
	            LOGEX(Time, LOG_ERROR, "Error while determining current time");
	        else
	            tResult = true;
		#endif
    #endif
    if (pDay)
        *pDay = tLocalTimeData.tm_mday;
    if (pMonth)
        *pMonth = tLocalTimeData.tm_mon + 1;
    if (pYear)
        *pYear = tLocalTimeData.tm_year + 1900;
    if (pHour)
        *pHour = tLocalTimeData.tm_hour;
    if (pMin)
        *pMin = tLocalTimeData.tm_min;
    if (pSec)
        *pSec = tLocalTimeData.tm_sec;

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
