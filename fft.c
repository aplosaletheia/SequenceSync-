/* The idea -
 *(im assuming we have samples enough to get a decently correct frequency domain) 
 *  We take the samples and divide them alternatively into some parts (lets take 2 for now)
 *  So what we have now are 2 arrays with odd and even indice of the original samples.
 *      We loose the ability to DIRECTLY get information of the 2nd half of
 *      the frequencies (the top half) from an individual array.
 *      If we are to apply fourier onto this directly then the amplitudes we will get corresponding to the 
 *      possible frequencies (the lower half) will have been influenced by the higher frequencies.
 *      But it is possible to regain that information since we have two sets of samples (more on this ahead)  
 * 
 * 
 *  the information about the lower half of the frequencies whould be the same in the fourier transforms of
 *  both the 'odd' and 'even' samples. Now to find the high frequencies that are disguised in them we will 
 *  subtract the two amplitudes and the remainder will be related to some higher frequencies.
 * 
 *  The higher frequency will be some frequency of a higher harmonic of that frequency.
 * 
 *  Since 'even' will have half of the information of the high frequency, it will give us lets say an increment
 *  in the amplitude of the lower frequency. In the 'odd' samples we will se a reduction in the amplitude of
 *  the same amount due to it being 180 degree out of phase.
 * 
 *  So yes the amplitude of every frequency in the lower half will be added from both sets and then /2.
 * 
 *  The remainder in the 1st bin will be the amplitude of the half + 1 bin, so for the mth bin it will
 *  be half (n/2) + m th bin.
 *  
 *  After some though I reacheched the conclusion that dividing into 2 every step is the best for us. 
 *  **TRUST ME I UNDERSTOOD THE WHOLE THING** [no I didn't, not cleanly, but got some sort of mathematical clarity 
 *  to some degree after talking to claude:)..It might be simple but I dont wanna think about it anymore since 
 *  I have gotten suffecient conformation that 2 and 3 are the best (closest to e)]
 * 
 *  I will not be going down to 2 samples per set and will choose the sample size that uses the cpu cashe the most
 *  efficiently and the overheads...idk the details yet so I'm keepinmg it parametric.
 */

#include "main.h"
#include "config.h"
#include <stddef.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


//will write the top NUMBER_OF_TOP_FREQUENCIES frequencies and their amplitudes 
int fft(const audioInfo_s* audioInfo, float* pWrite, size_t numOfTopFreq, size_t col)
{
    /*reorderingSamples*/
    float* temp = malloc(audioInfo->sampleCount*sizeof(temp));
    for (size_t i = 0; i < audioInfo->sampleCount; i++)
    {
        
    }

    free(temp);

    float* freqDomain = malloc(MAX_FREQ * FREQ_RESOLUTION *sizeof(freqDomain));
    
    for (size_t i = 0; (i+1)*DFT_SET_SIZE < audioInfo->sampleCount; i++) 
    {
        for (float freq = 1 / SHORT_TIME_PERIOD; freq < ((DFT_SET_SIZE / SHORT_TIME_PERIOD) / 2); freq += 1 / SHORT_TIME_PERIOD)
        {
            float amp = 0;
            float x = 0;
            float y = 0;
    
            float delta = (2*M_PI*freq)/audioInfo->sampleRate; //increase per sample
            float deltaSin = sinf(delta);
            float deltaCos = cosf(delta);
            float currSin = 0;
            float currCos = 1;
            for (size_t j = 0; j < DFT_SET_SIZE; j++)
            {
                x += audioInfo->samples[i*DFT_SET_SIZE + j]*currSin;
                y += audioInfo->samples[i*DFT_SET_SIZE + j]*currCos;
                float sinTemp = currSin;
                currSin = currSin*deltaCos + currCos*deltaSin;
                currCos = currCos*deltaCos - sinTemp*deltaSin;
            }
            amp = sqrtf(x*x + y*y) / freq;
            
        }
    }
}