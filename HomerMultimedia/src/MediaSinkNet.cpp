/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a network based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2009-12-27
 */

#include <Header_Ffmpeg.h>
#include <ProcessStatisticService.h>
#include <MediaSinkNet.h>
#include <MediaSinkMem.h>
#include <MediaSourceMem.h>
#include <MediaSourceNet.h>
#include <PacketStatistic.h>
#include <RTP.h>
#include <HBSocket.h>
#include <HBTime.h>
#include <Logger.h>
#include <Berkeley/SocketName.h>
#include <RequirementTargetPort.h>

#include <string>

#include <Requirements.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////
void MediaSinkNet::BasicInit(string pTargetHost, unsigned int pTargetPort)
{
	mStreamFragmentCopyBuffer = NULL;
    mNAPIDataSocket = NULL;
    mDataSocket = NULL;
    mBrokenPipe = false;
    mMaxNetworkPacketSize = 1280;
    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;
}

MediaSinkNet::MediaSinkNet(string pTarget, Requirements *pTransportRequirements, enum MediaSinkType pType, bool pRtpActivated):
    MediaSinkMem("memory", pType, pRtpActivated)
{
    BasicInit(pTarget, 0);
    mNAPIUsed = true;

    // get target port
    unsigned int tTargetPort = 0;
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pTransportRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        tTargetPort = tRequPort->getPort();

    }else
    {
        LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
        tTargetPort = 0;
    }
    mTargetPort = tTargetPort;

    // get target host
    mTargetHost = pTarget;

    // get transport type
    mStreamedTransport = pTransportRequirements->contains(pTransportRequirements->contains(RequirementTransmitStream::type()));
    enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (pTransportRequirements->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
    enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(pTarget)) ? SOCKET_IPv6 : SOCKET_IPv4;
    if (mStreamedTransport)
    	mStreamFragmentCopyBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

    // call NAPI
    if (mTargetHost != "")
    {
        LOG(LOG_VERBOSE, "Remote media sink at: %s<%d>%s", mTargetHost.c_str(), tTargetPort, mRtpActivated ? "(RTP)" : "");

        // finally associate to the target server/service
        Name tName(pTarget);
        mNAPIDataSocket = NAPI.connect(&tName, pTransportRequirements); //new Socket(IS_IPV6_ADDRESS(pTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, pSocketType);
    }

    switch(pType)
    {
        case MEDIA_SINK_VIDEO:
            ClassifyStream(DATA_TYPE_VIDEO, tTransportType, tNetworkType);
            break;
        case MEDIA_SINK_AUDIO:
            ClassifyStream(DATA_TYPE_AUDIO, tTransportType, tNetworkType);
            break;
        default:
            LOG(LOG_ERROR, "Undefined media type");
            break;
    }

    mMediaId = CreateId(mTargetHost, toString(mTargetPort), tTransportType, pRtpActivated);
    AssignStreamName("NET-OUT: " + mMediaId);

    StartSender();
}

MediaSinkNet::MediaSinkNet(string pTargetHost, unsigned int pTargetPort, Socket* pLocalSocket, enum MediaSinkType pType, bool pRtpActivated):
	MediaSinkMem("memory", pType, pRtpActivated)
{
    BasicInit(pTargetHost, pTargetPort);
    mDataSocket = pLocalSocket;
    mNAPIUsed = false;
    enum TransportType tTransportType = SOCKET_RAW;
    enum NetworkType tNetworkType = SOCKET_RAWNET;

    // get transport type
    mStreamedTransport = (tTransportType == SOCKET_TCP);
    if (mStreamedTransport)
        mStreamFragmentCopyBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

    LOG(LOG_VERBOSE, "Remote media sink at: %s<%d>%s", pTargetHost.c_str(), pTargetPort, mRtpActivated ? "(RTP)" : "");

    if (mDataSocket != NULL)
    {
        tTransportType = mDataSocket->GetTransportType();
        tNetworkType = mDataSocket->GetNetworkType();

        // define QoS settings
        QoSSettings tQoSSettings;
        switch(pType)
        {
            case MEDIA_SINK_VIDEO:
                ClassifyStream(DATA_TYPE_VIDEO, tTransportType, tNetworkType);
                tQoSSettings.DataRate = 20;
                tQoSSettings.Delay = 250;
                tQoSSettings.Features = QOS_FEATURE_NONE;
                break;
            case MEDIA_SINK_AUDIO:
                ClassifyStream(DATA_TYPE_AUDIO, tTransportType, tNetworkType);
                tQoSSettings.DataRate = 8;
                tQoSSettings.Delay = 100;
                tQoSSettings.Features = QOS_FEATURE_NONE;
                break;
            default:
                LOG(LOG_ERROR, "Undefined media type");
                break;
        }
        mDataSocket->SetQoS(tQoSSettings);
    }

    mMediaId = CreateId(pTargetHost, toString(pTargetPort), tTransportType, pRtpActivated);
    AssignStreamName("NET-OUT: " + mMediaId);

    StartSender();
}

MediaSinkNet::~MediaSinkNet()
{
	StopSender();

	if(mNAPIUsed)
    {
		if (mNAPIDataSocket != NULL)
			delete mNAPIDataSocket;
    }else
    {
    	//HINT: socket object has to be deleted outside
    }
    free(mStreamFragmentCopyBuffer);
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void MediaSinkNet::ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream, bool pIsKeyFrame)
{
    // save maximum network packet size to use it later within SendPacket() function
    if ((mMaxNetworkPacketSize == 0) && (pStream->codec->codec_id == CODEC_ID_H261))
        mMaxNetworkPacketSize = RTP::GetH261PayloadSizeMax() + RTP_HEADER_SIZE + 4 /* H.261 rtp payload header */;
    else
        mMaxNetworkPacketSize = pStream->codec->rtp_payload_size;

    // call ProcessPacket from mem based media sink
    MediaSinkMem::ProcessPacket(pPacketData, pPacketSize, pStream, pIsKeyFrame);
}

string MediaSinkNet::CreateId(string pHost, string pPort, enum TransportType pSocketTransportType, bool pRtpActivated)
{
    if (pSocketTransportType == SOCKET_TRANSPORT_TYPE_INVALID)
        return pHost + "<" + toString(pPort) + ">";
    else
        return pHost + "<" + toString(pPort) + ">(" + Socket::TransportType2String(pSocketTransportType) + (pRtpActivated ? "/RTP" : "") + ")";
}

void MediaSinkNet::StopProcessing()
{
	mSenderNeeded = false;
	MediaSinkMem::StopProcessing();
}

void MediaSinkNet::WriteFragment(char* pData, unsigned int pSize)
{
    if (mRtpActivated)
    {// RTP active
        MediaSinkMem::WriteFragment(pData, pSize);
    }else
    {// RTP inactive
        // HINT: we limit packet size to mMaxNetworkPacketSize if RTP is inactive
        int tFragmentCount = 1;
        tFragmentCount = (pSize + mMaxNetworkPacketSize -1) / mMaxNetworkPacketSize;
        #ifdef MSIN_DEBUG_PACKETS
            if (tFragmentCount > 1)
                LOG(LOG_WARN, "RTP is inactive and current packet of %d bytes is larger than limit of %d bytes per network packet, will split data into %d packets", pSize, mMaxNetworkPacketSize, tFragmentCount);
        #endif

        unsigned int tFragmentSize = pSize;
        char *tFragmentData = pData;
        while (tFragmentCount)
        {
            int64_t tTime = Time::GetTimeStamp();
            tFragmentSize = (unsigned int)(((int)pSize > mMaxNetworkPacketSize)? mMaxNetworkPacketSize : pSize);

            // for TCP add an additional fragment header in front of the codec data to be able to differentiate the fragments in a received TCP packet at receiver side
            if(mStreamedTransport)
            {
                if (MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE > TCP_FRAGMENT_HEADER_SIZE + tFragmentSize)
                {
                    TCPFragmentHeader *tHeader = (TCPFragmentHeader*)mStreamFragmentCopyBuffer;
                    memcpy(mStreamFragmentCopyBuffer + TCP_FRAGMENT_HEADER_SIZE, tFragmentData, tFragmentSize);
                    tHeader->FragmentSize = tFragmentSize;
                    tFragmentSize += TCP_FRAGMENT_HEADER_SIZE;
                    tFragmentData = mStreamFragmentCopyBuffer;
                }else
                {
                    LOG(LOG_ERROR, "TCP copy buffer is too small for data");
                }
            }

            int64_t tTime3 = Time::GetTimeStamp();
            #ifdef MSIN_DEBUG_TIMING
                int64_t tTime4 = Time::GetTimeStamp();
                LOG(LOG_VERBOSE, "       SendFragment::AnnouncePacket for a fragment of %u bytes took %ld us", tFragmentSize, tTime4 - tTime3);
            #endif
            MediaSinkMem::WriteFragment(tFragmentData, tFragmentSize);

            tFragmentData = tFragmentData + tFragmentSize;
            tFragmentCount--;
            #ifdef MSIN_DEBUG_TIMING
                int64_t tTime2 = Time::GetTimeStamp();
                LOG(LOG_VERBOSE, "       SendFragment::Loop for a fragment of %u bytes took %ld us", tFragmentSize, tTime2 - tTime);
            #endif
            if ((tFragmentData > (pData + pSize)) && (tFragmentCount))
            {
                LOG(LOG_ERROR, "Something went wrong, we have too many fragments and would read over the last byte of the fragment buffer");
                return;
            }
        }
    }
}

void MediaSinkNet::StartSender()
{
    LOG(LOG_VERBOSE, "Starting sender for target %s:%u", mTargetHost.c_str(), mTargetPort);

	mSenderNeeded = true;

	// start sender main loop
	StartThread();

    LOG(LOG_VERBOSE, "Sender for target %s:%u started", mTargetHost.c_str(), mTargetPort);
}

void MediaSinkNet::StopSender()
{
    int tSignalingRound = 0;
    char tTmp[4];

    LOG(LOG_VERBOSE, "Stopping sender");

    if (mSinkFifo != NULL)
    {
        // tell sender thread it isn't needed anymore
    	mSenderNeeded = false;

        // wait for termination of sender thread
        do
        {
            if(tSignalingRound > 0)
                LOG(LOG_WARN, "Signaling round %d to stop sender, system has high load", tSignalingRound);
            tSignalingRound++;

            // write fake data to awake sender thread as long as it still runs
            mSinkFifo->WriteFifo(tTmp, 0);
        }while(!StopThread(1000));
    }

    LOG(LOG_VERBOSE, "Encoder stopped");
}

void* MediaSinkNet::Run(void* pArgs)
{
    int tFifoEntry = 0;
    char *tBuffer;
    int tBufferSize;

    LOG(LOG_VERBOSE, "%s Stream relay for target %s:%u started", GetDataTypeStr().c_str(), mTargetHost.c_str(), mTargetPort);
    if (mNAPIUsed)
    {
        switch(GetDataType())
        {
            case DATA_TYPE_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-Relay(NAPI," + mCodec + ")");
                break;
            case DATA_TYPE_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Relay(NAPI," + mCodec + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }else
    {
        switch(GetDataType())
        {
            case MEDIA_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-Relay(NET," + mCodec + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Relay(NET," + mCodec + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }

    while(mSenderNeeded)
    {
    	if (mSinkFifo != NULL)
    	{
            tFifoEntry = mSinkFifo->ReadFifoExclusive(&tBuffer, tBufferSize);

            if ((tBufferSize > 0) && (mSenderNeeded))
            {
            	SendPacket(tBuffer, tBufferSize);
            }

            // release FIFO entry lock
            mSinkFifo->ReadFifoExclusiveFinished(tFifoEntry);

			if (tBufferSize == 0)
			{
				LOG(LOG_VERBOSE, "Zero byte %s packet in relay thread detected", GetDataTypeStr().c_str());
			}

			// is FIFO near overload situation?
            if (mSinkFifo->GetUsage() >= MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT - 4)
            {
                LOG(LOG_WARN, "Relay FIFO is near overload situation, deleting all stored frames");

                // delete all stored frames: it is a better for the encoding to have a gap instead of frames which have high picture differences
                mSinkFifo->ClearFifo();
            }
    	}else
        {
            LOG(LOG_VERBOSE, "Suspending the sender thread for 10 ms");
            Suspend(10 * 1000); // check every 1/100 seconds the state of the FIFO
        }
    }

    LOG(LOG_VERBOSE, "%s Socket-Listener for target %s:%u finished", GetDataTypeStr().c_str(), mTargetHost.c_str(), mTargetPort);

    return NULL;
}

void MediaSinkNet::SendPacket(char* pData, unsigned int pSize)
{
    if ((mTargetHost == "") || (mTargetPort == 0))
    {
        LOG(LOG_ERROR, "Remote network address invalid: %s:%u", mTargetHost.c_str(), mTargetPort);
        return;
    }
    if (mBrokenPipe)
    {
        LOG(LOG_VERBOSE, "Skipped fragment transmission because of broken pipe");
        return;
    }

    #ifdef MSIN_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Sending packet number %6ld at %p with size %4u(incl. %3u bytes header) to %s:%d", ++mPacketNumber, pData, pSize, RTP_HEADER_SIZE, mTargetHost.c_str(), mTargetPort);
//        for (int i = 0; i < 10; i++)
//            LOG(LOG_VERBOSE, "Packet data[%d] = %3d", i, pPacketData[i]);

        // if RTP activated then reparse the current packet and print the content
        if (mRtpActivated)
        {
            char *tFragmentData = pData;
            unsigned int tPacketSize = pSize;
            bool tIsLastFragment = false;
            bool tIsSenderReport = false;
            RtpParse(tFragmentData, tPacketSize, tIsLastFragment, tIsSenderReport, mIncomingAVStream->codec->codec_id, true);
        }
    #endif

    DoSendPacket(pData, pSize);
}

void MediaSinkNet::DoSendPacket(char* pData, unsigned int pSize)
{
    int64_t tTime = Time::GetTimeStamp();
	if(mNAPIUsed)
	{
		if (mNAPIDataSocket != NULL)
		{
			mNAPIDataSocket->write(pData, (int)pSize);
			if (mNAPIDataSocket->isClosed())
			{
				LOG(LOG_ERROR, "Error when sending data through NAPI connection to %s:%u, will skip further transmissions", mTargetHost.c_str(), mTargetPort);
				mBrokenPipe = true;
			}
		}
	}else
	{
		if (mDataSocket != NULL)
		{
			if (!mDataSocket->Send(mTargetHost, mTargetPort, pData, (ssize_t)pSize))
			{
				LOG(LOG_ERROR, "Error when sending data through %s socket to %s:%u, will skip further transmissions", GetTransportTypeStr().c_str(), mTargetHost.c_str(), mTargetPort);
				mBrokenPipe = true;
			}
		}
	}
    #ifdef MSIN_DEBUG_TIMING
        int64_t tTime2 = Time::GetTimeStamp();
        LOG(LOG_VERBOSE, "       sending a packet of %u bytes took %ld us", pSize, tTime2 - tTime);
    #endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
