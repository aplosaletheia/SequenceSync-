#define NUM_BUCKETS 100
#define SHORT_TIME_PERIOD 0.2f // sec
#define NUMBER_OF_TOP_FREQUENCIES 10
#define BASE_FREQUENCY 100.f //hz
#define MAX_FREQ 10000
#define HASH_INTERVAL 5  //always smaller than the clip
#define SCATTER 10
#define OFFSET 30
#define FILTER1_MIN_VOTES 3
#define MAX_NAME_SIZE 100

// for miniaudio capture
#define MA_SAMPLE_RATE 44100 //hz
#define MA_CHANNELS 2
#define MA_SAMPLE_TYPE int16_t