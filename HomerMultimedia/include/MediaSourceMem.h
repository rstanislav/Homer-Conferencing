/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Name:    MediaSourceMem.h
 * Purpose: ffmpeg based memory video source
 * Author:  Thomas Volkert
 * Since:   2011-05-05
 * Version: $Id$
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_MEM_
#define _MULTIMEDIA_MEDIA_SOURCE_MEM_

#include <Header_Ffmpeg.h>
#include <MediaSource.h>
#include <HBCondition.h>
#include <RTP.h>

#include <string>
#include <vector>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSMEM_DEBUG_PACKETS

// maximum packet size, including encoded data and RTP/TS
#define MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE          256*1024 // 256 kB
#define MEDIA_SOURCE_MEM_PACKET_BUFFER_SIZE              	 16*1024 // 16 kB

#define MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT 	             256 // 256 entries -> 4 MB

///////////////////////////////////////////////////////////////////////////////

struct MediaInputQueueEntry
{
	char	*Data;
	int		Size;
};

///////////////////////////////////////////////////////////////////////////////

class MediaSourceMem :
    public MediaSource, public RTP
{
public:
    /// The default constructor
    MediaSourceMem(bool pRtpActivated = true);
    /// The destructor.
    virtual ~MediaSourceMem();

    virtual bool SetInputStreamPreferences(std::string pStreamCodec, bool pDoReset = false, bool pRtpActivated = true);

    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

    virtual void AddDataInput(void* pPacketBuffer, int pPacketSize);

protected:
    static int ReceivePacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize);
    virtual void ReceiveFragment(char *pData, ssize_t &pDataSize);

    unsigned long       mPacketNumber;
    enum CodecID        mStreamCodecId;
    char                mStreamPacketBuffer[MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE];
    char                mPacketBuffer[MEDIA_SOURCE_MEM_PACKET_BUFFER_SIZE];
    bool                mRtpActivated;
    bool                mOpenInputStream;
    int                 mWrappingHeaderSize;
    MediaInputQueueEntry mQueue[MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT];
	int					mQueueWritePtr, mQueueReadPtr, mQueueSize;
    Mutex				mQueueMutex;
    Condition			mDataInputCondition;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif