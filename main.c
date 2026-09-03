#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <winnt.h>
#define _USE_MATH_DEFINES
#include <math.h>
#define MINIAUDIO_IMPLEMENTATION
#include "main.h"
#include "config.h"

clipSamples_s clipSamples = {0, 5*MA_SAMPLE_RATE*SHORT_TIME_PERIOD_MS/1000, 0};
audioInfo_s clipInfo = {MA_CHANNELS, MA_SAMPLE_RATE, 0, 0, 0};


int appendToClipSamples(clipSamples_s* clipSamples, MA_SAMPLE_TYPE* vals, size_t numVals)
{
    if (clipSamples->reserved + numVals <= clipSamples->count)
    {
        clipSamples->reserved += 5*MA_SAMPLE_RATE*SHORT_TIME_PERIOD_MS/1000;
        clipSamples->data = realloc(clipSamples->data, clipSamples->reserved*sizeof(*clipSamples->data));
    }
    memcpy(clipSamples->data, vals, numVals);
    clipSamples->count += 1;
}


int main()
{
    LARGE_INTEGER clockFreq, start, end;
    QueryPerformanceFrequency(&clockFreq);
    printf("clock frequency - %llihz\n", clockFreq.QuadPart);

    input_s input = getInput();

    audioData_s audioData;

    //add to database
    if (input.command == 'a')
    {
        printf("decoding .wav file...");
        QueryPerformanceCounter(&start);
        audioInfo_s audioInfo = wavDecoder(input.wavFile); //will alloc memory on heap
        QueryPerformanceCounter(&end);
        printf("done(%llims)\n", ((end.QuadPart - start.QuadPart)*1000 + clockFreq.QuadPart - 1)/ clockFreq.QuadPart);

        strcpy(audioData.name, input.name);

        printf("processing audio data...");
        QueryPerformanceCounter(&start);
        audioData.ampBandFull = fullAmpBand(&audioInfo);
        audioData.ampBandclubbed = clubAmpBand(audioData.ampBandFull);
        QueryPerformanceCounter(&end);
        printf("done(%llims)\n", ((end.QuadPart - start.QuadPart)*1000 + clockFreq.QuadPart - 1) / clockFreq.QuadPart);
        
        printf("adding to database...");
        QueryPerformanceCounter(&start);
        if (addToDatabase(audioData) == 0)
        {
            printf("done");
        }
        QueryPerformanceCounter(&end);
        printf("(%llims)\n", ((end.QuadPart - start.QuadPart)*1000 + clockFreq.QuadPart - 1) / clockFreq.QuadPart);
    }
    //give recommendations
    else if (input.command == 'r')
    {
        hashIndex_s hashIndex;
        hashIndex.numBuckets = NUM_BUCKETS;
        initHashTable(&hashIndex);
        printf("booting up database...\n");
        audioCat_s catalogue = bootDatabase(&hashIndex);

        
        //setting up audio capture device
        ma_device audioCaptureDevice;
        clipSamples.data = realloc(clipSamples.data, clipSamples.reserved*sizeof(*clipSamples.data));
        clipInfo.samples = clipSamples.data;
        initAudioCaptureDevice(&audioCaptureDevice);
        
        //hmm, 1 loop not guaranteed to be only 1 sec but in this case its better this way since even if there is some lag, no sudden jumps whout informing user
        printf("\n**To stop recording, enter 'e' in the terminal**\n");
        printf("recording begins in...");
        for(size_t i = 3; i > 0; i--)
        {
            printf("%zus...", i);
            Sleep(1000);
        }
        ma_device_start(&audioCaptureDevice);
        printf("recording...\n");
        if (getchar() == 'e')
        {
            ma_device_uninit(&audioCaptureDevice);
        }
        printf("done\n");

        audioData_s clipData;

        QueryPerformanceCounter(&start);
        clipData.ampBandFull = fullAmpBand(&clipInfo);
        clipData.ampBandclubbed = clubAmpBand(clipData.ampBandFull);
        QueryPerformanceCounter(&end);
        printf("(%llims)\n", ((end.QuadPart - start.QuadPart)*1000 + clockFreq.QuadPart - 1) / clockFreq.QuadPart);
        
        clipHashVals_s clipHashVals;
        hashClip(&clipHashVals, clipData.ampBandclubbed);
        
        audioSet_s filteredAudios = {0, NULL};

        filter1(&filteredAudios, hashIndex, clipHashVals);
        for (size_t i = 0; i < filteredAudios.numIds; i++)
        {
            printf("%s, ", catalogue.audioDatas[filteredAudios.audioIds[i]].name);
        }
    }

}



//function definitions

//called from a different thread whenevr sufficent data conditions are met
void audioReceiverCallback(ma_device* pDevice, void* pOutput, const void* pInput /*data from the capture card*/, ma_uint32 frameCount)
{
    appendToClipSamples(&clipSamples, (float*)pInput, frameCount);
    clipInfo.sampleCount += frameCount;
    clipInfo.duration = (float)clipInfo.sampleCount / clipInfo.sampleRate;
}


int initAudioCaptureDevice(ma_device* pDevice)
{
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.capture.format = ma_format_f32; //float
    config.capture.channels = 2;
    config.sampleRate = MA_SAMPLE_RATE;
    config.dataCallback = audioReceiverCallback;
    config.periodSizeInMilliseconds = SHORT_TIME_PERIOD_MS;

    if (ma_device_init(NULL, &config, pDevice) != MA_SUCCESS) 
    {
        return -1;  // Failed to initialize the device.
    }
}


int appentToCat(audioCat_s *audioCat, audioData_s audioData)
{
    if (audioCat->numIds < audioCat->reserved)
    {
        audioCat->reserved *= 2;
        audioCat->audioDatas = realloc(audioCat->audioDatas, audioCat->reserved*sizeof(audioData_s));
    }
    audioCat->audioDatas[audioCat->numIds] = audioData;
    audioCat->numIds++;
}

audioCat_s bootDatabase(hashIndex_s* hashIndex)
{
    audioCat_s catalogue;
    catalogue.numIds = 0;
    catalogue.reserved = 100;
    catalogue.audioDatas = malloc(catalogue.reserved*sizeof(*catalogue.audioDatas));

    size_t maxId = 0;
    FILE* bin = fopen("database.bin", "rb");
    if (bin == NULL)
    {
        printf("no data file, making a file in the same folder as this...\n");
            bin = fopen("database.bin", "wb");
            if (bin != NULL)
            {
                fclose(bin);
            }
            else
            {
                printf("couldnt successfully make and open file..");
            }
            return catalogue;
    }

    while (1)
    {
        audioData_s audioData;

        if (fread(&audioData.audioId, sizeof(audioData.audioId), 1, bin) != 1) break; // clean EOF
        if (fread(audioData.name, sizeof(char), sizeof(audioData.name), bin) != sizeof(audioData.name)) break;

        if (fread(&audioData.ampBandclubbed.rows, sizeof(size_t), 1, bin) != 1) break;
        if (fread(&audioData.ampBandclubbed.cols, sizeof(size_t), 1, bin) != 1) break;
        size_t clubbedCount = audioData.ampBandclubbed.rows * audioData.ampBandclubbed.cols;
        audioData.ampBandclubbed.data = malloc(clubbedCount * sizeof(int));
        if (fread(audioData.ampBandclubbed.data, sizeof(int), clubbedCount, bin) != clubbedCount) break;

        if (fread(&audioData.ampBandFull.rows, sizeof(size_t), 1, bin) != 1) break;
        if (fread(&audioData.ampBandFull.cols, sizeof(size_t), 1, bin) != 1) break;
        size_t fullCount = audioData.ampBandFull.rows * audioData.ampBandFull.cols;
        audioData.ampBandFull.data = malloc(fullCount * sizeof(int));
        if (fread(audioData.ampBandFull.data, sizeof(int), fullCount, bin) != fullCount) break;

        audioData.ampBandFilter2.data = NULL;
        audioData.ampBandFilter2.rows = 0;
        audioData.ampBandFilter2.cols = 0;

        appentToCat(&catalogue, audioData);
        addToHashTable(audioData, *hashIndex);

        if (audioData.audioId > maxId) maxId = audioData.audioId;
    }

    fclose(bin);
    return catalogue;
}


int initHashTable(hashIndex_s* hashIndex)
{
    hashIndex->buckets = calloc(hashIndex->numBuckets, sizeof(entry_s*));
}

input_s getInput()
{
    input_s result;
    printf("a - add to database, r - get recommendations\n");
    result.command = getchar();
    while (getchar() != '\n')
    {}
    if (result.command == 'r')
    {
        result.wavFile = NULL;

    }
    else if(result.command == 'a')
    {
        printf("please enter file name (ignore the .wav) max leng - 100char\n");
        scanf("\n %s", result.name);
        char fileName[MAX_NAME_SIZE + 4 + 1] = {0};
        memcpy(fileName, result.name, strlen(result.name));
        //can add option for various types of files (if I get the decoder for them)
        strcat(fileName, ".wav");
        result.wavFile = fopen(fileName, "rb");
        if (result.wavFile == NULL)
        {
            printf("failed to open file\n");
            exit(1);
        }
    }
    else
    {
        printf("invalid command\n");
        exit(1);
    }
    return result;
}

uint32_t readLE32(const unsigned char* b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

uint16_t readLE16(const unsigned char* b)
{
    return (uint16_t)(b[0] | (b[1] << 8));
}

audioInfo_s wavDecoder(FILE* wavFile)
{
    audioInfo_s audioInfo;
    memset(&audioInfo, 0, sizeof(audioInfo));

    unsigned char header[12];
    if (fread(header, 1, 12, wavFile) != 12 ||
        memcmp(header, "RIFF", 4) != 0 ||
        memcmp(header + 8, "WAVE", 4) != 0)
    {
        printf("not a valid wav file\n");
        exit(1);
    }

    int haveFmt = 0;
    int haveData = 0;
    unsigned char chunkHeader[8];
    uint32_t chunkSize;

    // walk chunks until we've found both fmt and data (or hit EOF)
    while (fread(chunkHeader, 1, 8, wavFile) == 8)
    {
        char chunkId[5] = {0};
        memcpy(chunkId, chunkHeader, 4);
        chunkSize = readLE32(chunkHeader + 4);
        audioInfo.sampleCount = chunkSize / sizeof(int16_t); // total samples across all channels
        audioInfo.duration = (float)audioInfo.sampleCount / (float)audioInfo.sampleRate; // seconds
        
        if (memcmp(chunkId, "fmt ", 4) == 0)
        {
            unsigned char fmtBuf[16];
            if (chunkSize < 16 || fread(fmtBuf, 1, 16, wavFile) != 16)
            {
                printf("bad fmt chunk\n");
                exit(1);
            }
            uint16_t audioFormat = readLE16(fmtBuf + 0);
            if (audioFormat != 1) // 1 = PCM
            {
                printf("only uncompressed PCM wav is supported (got format %u)\n", audioFormat);
                exit(1);
            }
            audioInfo.channels   = readLE16(fmtBuf + 2);
            audioInfo.sampleRate = readLE32(fmtBuf + 4);
            uint16_t bitsPerSample = readLE16(fmtBuf + 14);
            if (bitsPerSample != 16)
            {
                printf("only 16-bit PCM wav is supported (got %u bits)\n", bitsPerSample);
                exit(1);
            }
            // fmt chunk may be padded beyond 16 bytes; skip any extra
            if (chunkSize > 16)
            {
                fseek(wavFile, chunkSize - 16, SEEK_CUR);
            }
            haveFmt = 1;
        }
        else if (memcmp(chunkId, "data", 4) == 0)
        {
            
            audioInfo.samples = malloc(audioInfo.sampleCount*sizeof(*audioInfo.samples)); // chunkSize is int16_t elements in byte
            int16_t* temp = malloc(chunkSize);
            if (audioInfo.samples == NULL)
            {
                printf("malloc failed for %ui bytes of sample data\n", chunkSize);
                exit(1);
            }
            if (fread(temp, 1, chunkSize, wavFile) != chunkSize)
            {
                printf("failed to read expected %ui bytes of sample data\n", chunkSize);
                exit(1);
            }
            for (size_t i = 0; i + 1 < audioInfo.sampleCount; i += 2)
            {
                audioInfo.samples[i/2] = (float)(temp[i] + temp[i+1]) / 2;
            }
            free(temp);
            haveData = 1;
            break; // we have what we need; stop walking chunks
        }
        else
        {
            // unknown/irrelevant chunk (LIST, fact, etc.) - skip it
            fseek(wavFile, chunkSize, SEEK_CUR);
        }

        // chunks are word-aligned; skip one pad byte if chunkSize is odd
        if (chunkSize % 2 != 0)
        {
            fseek(wavFile, 1, SEEK_CUR);
        }
    }

    if (!haveFmt || !haveData)
    {
        printf("wav file missing fmt or data chunk\n");
        exit(1);
    }


    return audioInfo;
}



int shortFourier(const audioInfo_s*, float*, size_t, size_t);
int convertToNotes(float, float*, size_t, int*);
int checkForSameFreq(int*, int*, size_t,  bool*); // matches the frequency indices in c with that of c-1
int ascendingOrder(int*, size_t, bool*);


ampBand_s fullAmpBand(const audioInfo_s* audioInfo)
{
    //printf("0   ");
    ampBand_s ampBand;
    ampBand.cols = (size_t)((audioInfo->duration + SHORT_TIME_PERIOD - 0.01) / SHORT_TIME_PERIOD);
    ampBand.rows = NUMBER_OF_TOP_FREQUENCIES; //not be remain a compile const maybe
    ampBand.data = malloc((ampBand.cols*ampBand.rows)*sizeof(*ampBand.data));
    float freq[ampBand.rows];
    bool commonIndicesFlags[ampBand.rows];
    memset(commonIndicesFlags, 0, ampBand.rows*sizeof(*commonIndicesFlags));
    //printf("1   ");
    for (size_t c = 0; c < ampBand.cols; c++)
    {
        shortFourier(audioInfo, freq, ampBand.rows, c);
        //printf("stft   ");
        convertToNotes(BASE_FREQUENCY, freq, ampBand.rows, (ampBand.data + c*ampBand.rows));
        //printf("convet   ");
        if (c > 0)
        {
            checkForSameFreq((ampBand.data + (c)*ampBand.rows), (ampBand.data + (c-1)*ampBand.rows), ampBand.rows, commonIndicesFlags);
        }
        //printf("same check   ");
        ascendingOrder(ampBand.data + c*ampBand.rows, ampBand.rows, commonIndicesFlags);
        //printf("sorting   ");
        if (c > 0)
        {
            for (size_t i = 0; i < ampBand.rows; i++)
            {
                ampBand.data[(c-1)*ampBand.rows + i] = ampBand.data[c*ampBand.rows + i] - ampBand.data[(c-1)*ampBand.rows + i];
            }
        }
        //printf("difference   %zu\n", c);
    }
    //printf("stage 1 done...");
    return ampBand;
}
//writes the top 'numOfTopFreq' frequencies (not their amplitudes just the frequency in hz)
int shortFourier(const audioInfo_s* audioInfo, float* pWrite, size_t numOfTopFreq, size_t col)
{
    float temp[2][numOfTopFreq]; //first col is freq
    memset(temp, 0, sizeof(temp));
    size_t totalSamples = (size_t)(audioInfo->sampleRate*SHORT_TIME_PERIOD);
    size_t windowStart = (size_t)(col*SHORT_TIME_PERIOD*audioInfo->sampleRate);

    for (float freq = BASE_FREQUENCY; freq < MAX_FREQ; freq += 1 / SHORT_TIME_PERIOD)
    {
        float amp = 0;
        float x = 0;
        float y = 0;

        float delta = (2*M_PI*freq)/audioInfo->sampleRate; //increase per sample
        float deltaSin = sinf(delta);
        float deltaCos = cosf(delta);
        float currSin = 0;
        float currCos = 1;
        for (size_t i = 0; i < totalSamples; i++)
        {
            x += audioInfo->samples[windowStart + i]*currSin;
            y += audioInfo->samples[windowStart + i]*currCos;
            float sinTemp = currSin;
            currSin = currSin*deltaCos + currCos*deltaSin;
            currCos = currCos*deltaCos - sinTemp*deltaSin;
        }
        amp = sqrtf(x*x + y*y) / freq;
        for (size_t i = 0; i < numOfTopFreq; i++)
        {
            if(amp > temp[1][i])
            {
                for (size_t j = numOfTopFreq - 1; j > i; j += -1)
                {
                    temp[1][j] = temp[1][j-1];
                    temp[0][j] = temp[0][j-1];
                }
                temp[1][i] = amp;
                temp[0][i] = freq;
                break;
            }
        }
    }
    for (size_t i = 0; i < numOfTopFreq; i++)
    {
        pWrite[i] = temp[0][i];
    }

    return 0;
}

int convertToNotes(float baseFreq, float* freqData, size_t eleCount, int* wNotes)
{
    for (size_t i = 0; i < eleCount; i++)
    {
        wNotes[i] = roundf(12 * log2f(freqData[i] / baseFreq)); //gives us the half steps from base note
    }
}

int checkForSameFreq(int* curr, int* prev, size_t eleCount,  bool* commonIndicesFlags)
{
    for (size_t i = 0; i < eleCount; i++)
    {
        for (size_t j = 0; j < eleCount; j++)
        {
            if (prev[i] == curr[j])
            {
                int temp = curr[i];
                curr[i] = curr[j];
                curr[j] = temp;
                commonIndicesFlags[i] = true;
                break;
            }
            commonIndicesFlags[j] = false;
        }
    }
}

int ascendingOrder(int* data, size_t eleCount, bool* commonIndicesFlags)
{
    for (size_t i = 0; i < eleCount; i++)
    {
        if (commonIndicesFlags[i] == true)
        {
            continue;
        }
        for (size_t j = i+1; j < eleCount; j++)
        {
            if (commonIndicesFlags[j] == true)
            {
                continue;
            }
            if (data[j] < data[i])
            {
                float temp = data[i];
                data[i] = data[j];
                data[j] = temp;
            }
        }
    }
}

ampBand_s clubAmpBand(ampBand_s ampBandFull)
{
    ampBand_s clubbed;
    clubbed.rows = ampBandFull.rows;
    clubbed.cols = 0;
    int* temp = calloc(ampBandFull.cols*ampBandFull.rows, sizeof(*clubbed.data));
    for (size_t r = 0; r < clubbed.rows; r++)
    {
        size_t cols = 0;
        for (size_t i = 0; i < ampBandFull.cols - 1; i++)
        {
            if(abs(ampBandFull.data[r + i*ampBandFull.rows] + temp[cols*ampBandFull.rows + r]) >= abs(temp[r + cols*ampBandFull.rows]))
            {
                temp[r + cols*ampBandFull.rows] = ampBandFull.data[r + i*ampBandFull.rows] + temp[r + cols*ampBandFull.rows];
            }
            else
            {
                cols += 1;
                temp[r + cols*ampBandFull.rows] = ampBandFull.data[r + i*ampBandFull.rows];
            }
        }
        if (cols + 1 > clubbed.cols)
        {
            clubbed.cols = cols + 1;
        }
    }
    clubbed.data = malloc(clubbed.cols*clubbed.rows*sizeof(*clubbed.data));
    for (size_t c = 0; c < clubbed.cols; c++)
    {
        memcpy(clubbed.data + c*clubbed.rows, (temp + c*ampBandFull.rows), clubbed.rows*sizeof(*clubbed.data));
    }
    free(temp);
    return clubbed;
}


int appendToHash(size_t, size_t, size_t, size_t, hashIndex_s);

int addToHashTable(audioData_s audioData,  hashIndex_s hashIndex)
{
    for (size_t r = 0; r < audioData.ampBandclubbed.rows; r++)
    {
        for (size_t c = 0; c < audioData.ampBandclubbed.cols - HASH_INTERVAL + 1; c++)
        {
            size_t hashValue = 0;
            if (audioData.ampBandclubbed.data[c*audioData.ampBandclubbed.rows + r] == 0)
            {
                break;
            }
            for (size_t i = 0; i < HASH_INTERVAL; i++)
            {
                hashValue = hashValue*SCATTER + audioData.ampBandclubbed.data[(c + i)*audioData.ampBandclubbed.rows + r] + OFFSET;
            }
            appendToHash(audioData.audioId, r, c, hashValue, hashIndex);
        }
    }
}

int appendToHash(size_t audioId, size_t row, size_t col, size_t hashvalue, hashIndex_s hashIndex)
{
    size_t position = hashvalue % hashIndex.numBuckets;
    entry_s* pEntry= malloc(sizeof(entry_s));
    pEntry->audioId = audioId;
    pEntry->col = col;
    pEntry->row = row;
    appendHashEntry(&hashIndex.buckets[position], pEntry);
}

int appendHashEntry(entry_s** pexisting, entry_s* toAppend)
{
    if (*pexisting == NULL)
    {
        *pexisting = toAppend;
    }
    else
    {
        entry_s* temp = *pexisting;
        for (;temp->next != NULL; temp = temp->next)
        {}
        temp->next = toAppend;
    }
    toAppend->next = NULL;
}

int hashClip(clipHashVals_s* clipHashVals, ampBand_s clubbedAmpBand)
{
    size_t divisions = (size_t)(clubbedAmpBand.cols / HASH_INTERVAL); //divs in one row
    clipHashVals->num = divisions*clubbedAmpBand.rows;
    clipHashVals->vals = malloc(clipHashVals->num*sizeof(*clipHashVals->vals));
    for (size_t i = 0; i < clubbedAmpBand.rows; i++)
    {
        size_t hashVal = 0;
        for (size_t j = 0; j < divisions; j += HASH_INTERVAL)
        {
            for (size_t k = 0 ; k < HASH_INTERVAL; k++)
            {
                hashVal = hashVal*SCATTER + clubbedAmpBand.data[(j + k)*clubbedAmpBand.rows + i] + OFFSET;
            }
            clipHashVals->vals[i*divisions + j] = hashVal % NUM_BUCKETS;
        }
    }
}

int addToDatabase(audioData_s audioData)
{
    FILE* bin = fopen("database.bin", "ab");
        if (bin == NULL)
        {
            printf("failed to open database file\n");
            return -1;
        }

        fwrite(&audioData.audioId, sizeof(audioData.audioId), 1, bin);
        fwrite(audioData.name, sizeof(char), sizeof(audioData.name), bin);

        fwrite(&audioData.ampBandclubbed.rows, sizeof(size_t), 1, bin);
        fwrite(&audioData.ampBandclubbed.cols, sizeof(size_t), 1, bin);
        fwrite(audioData.ampBandclubbed.data, sizeof(int),
               audioData.ampBandclubbed.rows * audioData.ampBandclubbed.cols, bin);

        fwrite(&audioData.ampBandFull.rows, sizeof(size_t), 1, bin);
        fwrite(&audioData.ampBandFull.cols, sizeof(size_t), 1, bin);
        fwrite(audioData.ampBandFull.data, sizeof(int),
               audioData.ampBandFull.rows * audioData.ampBandFull.cols, bin);

        fclose(bin);
        return 0;
}

typedef struct
{
    size_t audioId;
    size_t votes;
} vote_s;

int filter1(audioSet_s* pWriteAudioId, hashIndex_s hashIndex, clipHashVals_s clipHashVals)
{
    vote_s* votes = NULL;
    size_t voteCount = 0;

    for (size_t i = 0; i < clipHashVals.num; i++)
    {
        size_t bucket = clipHashVals.vals[i];
        for (entry_s* e = hashIndex.buckets[bucket]; e != NULL; e = e->next)
        {
            bool found = false;
            for (size_t v = 0; v < voteCount; v++)
            {
                if (votes[v].audioId == e->audioId)
                {
                    votes[v].votes++;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                vote_s* grown = realloc(votes, (voteCount + 1) * sizeof(*votes));
                if (!grown) { free(votes); free(clipHashVals.vals); return -1; }
                votes = grown;
                votes[voteCount].audioId = e->audioId;
                votes[voteCount].votes = 1;
                voteCount++;
            }
        }
    }
    free(clipHashVals.vals);

    pWriteAudioId->numIds = 0;
    pWriteAudioId->audioIds = NULL;
    for (size_t v = 0; v < voteCount; v++)
    {
        if (votes[v].votes >= FILTER1_MIN_VOTES)
        {
            size_t* grown = realloc(pWriteAudioId->audioIds, (pWriteAudioId->numIds + 1) * sizeof(size_t));
            if (!grown) { free(votes); return -1; }
            pWriteAudioId->audioIds = grown;
            pWriteAudioId->audioIds[pWriteAudioId->numIds++] = votes[v].audioId;
        }
    }
    free(votes);
    return 0;
}


int filter2(audioSet_s* audioSet, audioCat_s catalogue, audioData_s clipInfo)
{
    return 0;
}
