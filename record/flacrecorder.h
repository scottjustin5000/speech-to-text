#ifndef FLAC_RECORDER_H_ 
#define FLAC_RECORDER_H_

#include <stdint.h>
#include <stdlib.h>
#include <portaudio.h>

typedef struct{
  uint16_t formatType;
  uint8_t numberOfChannels;
  uint32_t sampleRate;
  size_t size;
  float *recordedSamples;
} AudioData;

typedef struct{
  float *snippet;
  size_t size;
} AudioSnippet;

int record(const char *fileName);
#endif