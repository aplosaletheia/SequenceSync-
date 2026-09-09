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
 */