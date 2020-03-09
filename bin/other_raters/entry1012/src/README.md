# VoIP contest submission
Daniil Gentili's submission to the VoIP contest: <daniil@daniil.it>, @danogentili.
Licensed under GPLv3, will be published at https://github.com/danog/contest-voip.

The rating software is based on voice recognition with custom audio models and vocabulary, generated using built-in scripts (along with secondary rating modules based on silence and length recognition).
The call software is a very simple OOP wrapper around libtgvoip, making use of libopusenc and libopusfile to encode and decode OGG OPUS files.

## Build instructions

The call executable requires the (Debian `sid`) `libopusenc` library to generate ogg files, as well as a slightly tweaked libtgvoip with disabled console logging.
To build and install both libraries, simply run `make`:

```
cd src
make
```

This will automatically build and install all required libraries, build an audio recognition model and generate the required `tgvoip` executables.


## Rationale

### tgvoipcall

The call software is a simple OOP wrapper around libtgvoip, making use of the cross-platform libtgvoip Semaphore wrapper to wait for call termination.
OPUS encoding and decoding is handled by the vanilla `libopus` library (no need to use the whole lavf library): since the original OPUS samples were actually framed inside of OGG containers, I included the `libopusfile` and `libopusenc` libraries to very easily read and generate OPUS frames from/to OGG files.

The latter is a rather recent development of the xiph devteam (the ones who created the OPUS codec and `libopus`, `libopusfile` libraries), which is only available in debian sid (`unstable`).
To make up for this, I included some directives in the `Makefile` to automatically build and install the `libopusenc` library from source.

The `tgvoipcall` library accepts one more **optional** parameter with a path to a logfile for libtgvoip.

### tgvoiprate

When I first approached the task of creating the rating software, I thought of how a human user like me detects when a Telegram call starts glitching due to network changes or other causes: typically, it starts with some audio interruptions and noise, typically followed by a drastical change in quality (due to libtgvoip automatically adjusting the bitrate of the OPUS encoder).
Because of this, I initially wrote the code for a noise detection rating system based on the webrtc NoiseEstimator, along with another module based on fftw3 to detect changes in the cutoff frequency using FFT.
However, the NoiseEstimator webrtc module did not give good results, and since most of the original files already presented cutoff at 20khz, I later scrapped both modules (they can be seen in the history of the git repo).

Then I realized that the main problem for __voice calls__ is caused by degradation of speech: which is why I have developed a rating module mainly based on voice recognition.

The **voice recognition** module is based on pocketsphinx: I also developed a fully-working testing platform and acoustic model generator, which takes the original audio files + manual transcriptions to generate improved pocketsphinx acoustic models and a new vocabulary, which greatly improves the efficiency and accuracy of voice recognition (near 100% for harvard sentence set H11, which is the sentence set used by most of the provided samples).

The model generation scripts (automatically called by `make`) look for new samples in the `assets/samples` directory and ask the user to listen and transcribe the contents of new samples, writing transcripted text to `assets/transcription`.
Then, sphinxtrain utilities and a custom transcription formatter written in C++ (`fixTranscripts.cpp`) are used to generate an acoustic model and vocabulary for pocketsphinx.
Then, the new model is used to generate automatic transcriptions for the samples, which are then compared with the manual transcriptions using `diff --color`.

The transcription formatter, originally written in PHP, was rewritten in C++17 for greater speed (and to remove a dependency :).
The build dependencies for the project consist only in the `sphinx` and `opus` libraries (the sphinx training library wasn't in the buster repos, so the `Makefile` downloads and builds that, too).

Two other rating modules based on length and silence detection are also used in a weighed approach to generate the final rating.

The `tgvoiprate` library accepts one more **optional** parameter with a path to a logfile for pocketsphinx and `tgvoiprate` itself.
