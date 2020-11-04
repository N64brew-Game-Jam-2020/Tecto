#include <audio.h>
#include <ultra64.h>
#include <malloc.h>
#include <mem.h>

#define SINE_FREQ 440

s16 audioBuffers[NUM_AUDIO_BUFFERS][AUDIO_BUFFER_LEN] __attribute__((aligned (64)));

OSMesgQueue audioMsgQueue;
OSMesg audioMsg;

u16 curAudioBuffer = 0;
u32 audioPhaseAngle = 0;
s32 realSampleRate;

OSMesgQueue audioDmaMesgQueue;

OSIoMesg audioDmaIoMessage;
OSMesg audioDmaMessage;

u8* song;

extern u8 _songSegmentRomStart[], _songSegmentRomEnd[];

void audioInit(void)
{
    int err;

    osCreateMesgQueue(&audioMsgQueue, &audioMsg, 1);
    osSetEventMesg(OS_EVENT_AI, &audioMsgQueue, NULL);

    // Set up a zero buffer to prevent any audio pop at startup
    bzero(audioBuffers[2], AUDIO_BUFFER_LEN * sizeof(u16));
    osAiSetNextBuffer(&audioBuffers[2], AUDIO_BUFFER_LEN * sizeof(u16));

    // Set up the audio frequency
    realSampleRate = osAiSetFrequency(SAMPLE_RATE);
    

    // Create message queue for DMA reads/writes
    // osCreateMesgQueue(&audioDmaMesgQueue, &audioDmaMessage, 1);

    // decoder = opus_decoder_create(SAMPLE_RATE, 2, &err);

    // song = mt_malloc((u32)_songSegmentRomEnd - (u32)_songSegmentRomStart);
    
    // // Invalidate the data cache for the region being DMA'd to
    // osInvalDCache(song, (u32)_songSegmentRomEnd - (u32)_songSegmentRomStart); 

    // // Set up the intro segment DMA
    // audioDmaIoMessage.hdr.pri = OS_MESG_PRI_NORMAL;
    // audioDmaIoMessage.hdr.retQueue = &audioDmaMesgQueue;
    // audioDmaIoMessage.dramAddr = song;
    // audioDmaIoMessage.devAddr = (u32)_songSegmentRomStart;
    // audioDmaIoMessage.size = (u32)_songSegmentRomEnd - (u32)_songSegmentRomStart;

    // // Start the DMA
    // osEPiStartDma(g_romHandle, &audioDmaIoMessage, OS_READ);

    // // Wait for the DMA to complete
    // osRecvMesg(&audioDmaMesgQueue, NULL, OS_MESG_BLOCK);
}

void fillAudioBuffer(u32 bufferIndex)
{
    s16 *audioPtr = &audioBuffers[bufferIndex][0];
    s16 *endPtr = &audioBuffers[bufferIndex][AUDIO_BUFFER_LEN];

    do
    {
        audioPtr[0] = audioPtr[1] = sins(audioPhaseAngle) / 4; // Left
        audioPtr += 2;
        // *audioPtr++ = sins(2 * audioPhaseAngle) / 4; // Right
        audioPhaseAngle += (0x10000L * SINE_FREQ / SAMPLE_RATE);
    }
    while (audioPtr != endPtr);
}

void audioThreadFunc(__attribute__((unused)) void *arg)
{
    audioInit();

    // Set up the first audio buffer
    fillAudioBuffer(curAudioBuffer);
    // Clear any pending messages from the queue
    while (!MQ_IS_EMPTY(&audioMsgQueue))
    {
        osRecvMesg(&audioMsgQueue, NULL, OS_MESG_BLOCK);
    }
    while (1)
    {
        // Initiate the current buffer's DMA
        osAiSetNextBuffer(&audioBuffers[curAudioBuffer], AUDIO_BUFFER_LEN * sizeof(u16));

        // Set up the next buffer
        curAudioBuffer = (curAudioBuffer + 1) % NUM_AUDIO_BUFFERS;
        fillAudioBuffer(curAudioBuffer);
        
        // Wait for the current buffer to run out
        osRecvMesg(&audioMsgQueue, NULL, OS_MESG_BLOCK);
    }
}
