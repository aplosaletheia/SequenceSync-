#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "config.h"
#include "miniaudio.h"

typedef struct
{
    FILE* wavFile;
    char name[MAX_NAME_SIZE + 1]; // '\0'
    char command; // either add to database (a) or get recommendations (r)
} input_s;

typedef struct
{
    //directly from file header

    unsigned int channels;
    unsigned int sampleRate;
    unsigned int byteRate;
    unsigned int dataSize; //in bytes

    //additional info
    unsigned int sampleCount;
    float duration;
    int16_t* samples;
} wavInfo_s;

typedef struct
{
    int* data;
    size_t rows;
    size_t cols;
} ampBand_s;

//for now we take it so that the sampleRate and byterate are same across all songs and clips
typedef struct
{
    size_t audioId;
    char name[101];
    ampBand_s ampBandclubbed; //flag for stop comparing is 0
    ampBand_s ampBandFilter2;
    ampBand_s ampBandFull; // ignore the last column
} audioInfo_s;

typedef struct
{
    size_t numIds;
    audioInfo_s* audioInfos;
    size_t reserved;
} audioCat_s;

typedef struct
{
    size_t numIds;
    size_t* audioIds;
} audioSet_s;

typedef struct entry_s
{
    size_t audioId;
    size_t row;
    size_t col;
    struct entry_s* next;
} entry_s;

typedef struct
{
    size_t numBuckets;
    entry_s** buckets;
} hashIndex_s;

typedef struct
{
    size_t num;
    size_t* vals; // row 0 col 0-whatever then row 0 col whatever + whatever
} clipHashVals_s;


// function declarations
int initHashTable(hashIndex_s*);
audioCat_s bootDatabase(hashIndex_s*);
input_s getInput(); //ND
wavInfo_s wavDecoder(FILE*);
void getSamples(FILE*, int16_t*);
ampBand_s fullAmpBand(const wavInfo_s*);
ampBand_s clubAmpBand(ampBand_s);
int addToDatabase(audioInfo_s);
int addToHashTable(audioInfo_s, hashIndex_s);
int appendHashEntry(entry_s**, entry_s*);
int hashClip(clipHashVals_s*, ampBand_s);
int filter1(audioSet_s*, hashIndex_s, clipHashVals_s);
int filter2(audioSet_s*, audioCat_s, audioInfo_s);

//miniaudio stuff

typedef struct
{
    size_t count;
    size_t reserved;
    MA_SAMPLE_TYPE* data;
} clipSamples_s

clipSamples_s startRecording(); //arg depends on miniaudio
