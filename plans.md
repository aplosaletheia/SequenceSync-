Instead of taking a wav input for r (recommendation) mode, the clip will be taken from audio paying by the audio card using miniaudio library.

  there will be a start recording and then an end recording option through the terminal *maybe a GUI in the future*

Add to database will still be via wav files or maybe .mp3.


DFT will be replaced by a hand written FFT algorithm.


Instead of the current change determining system (always calculates the minimum change) the new system will check for variations in frequencies with similar timbers (same source of note).


Filter 2 will make sure that the clubbedAmpBands have common parts (with some allowed mismatches) between the cli pand the songs in the filteredSongs through filter1.

  will use the hash tabe properly for this.
