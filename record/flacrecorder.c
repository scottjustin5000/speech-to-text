#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <portaudio.h>
#include <sndfile.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include "flacrecorder.h"

const static int FRAMES_PER_BUFFER = 1024;
const static float MIN_TALKING_BUFFERS = 8;
const static float TALKING_THRESHOLD_WEIGHT = 0.99;
const static float TALKING_TRIGGER_RATIO = 4.0;

AudioData* allocAudioData() {
  AudioData *data = malloc(sizeof(AudioData));
  data->formatType = paFloat32;
  data->numberOfChannels = 1; 
  data->sampleRate = 16000;
  data->size = 0;
  data->recordedSamples = NULL;
  return data;
}


#define debug_print(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

float rms(float *data, size_t len){
    double sum = 0.0;
    for(size_t i = 0; i < len; ++i)
    {
        sum += pow(data[i], 2);
    }
    return sqrt(sum / len);
}

long storeFlac(AudioData *data, const char *fileName) {
    uint8_t err = SF_ERR_NO_ERROR;
    SF_INFO sfinfo ={
        .channels = data->numberOfChannels,
        .samplerate = data->sampleRate,
        .format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16
    };

    SNDFILE *outfile = sf_open(fileName, SFM_WRITE, &sfinfo);
    if (!outfile) return -1;

    long wr = sf_writef_float(outfile, data->recordedSamples, data->size / sizeof(float));
    err = data->size - wr;

    sf_write_sync(outfile);
    sf_close(outfile);
    return err;
}

float adjustThreshold(float talkingThreshold,float talkingIntensity) {
  return TALKING_THRESHOLD_WEIGHT * talkingThreshold + (1 - TALKING_THRESHOLD_WEIGHT) * talkingIntensity / TALKING_TRIGGER_RATIO;
}

int processStream(PaStream *stream, AudioData *data, AudioSnippet *sampleBlock, const char *fileName, bool *sampleComplete) {
    if (!stream || !data || !sampleBlock || !sampleComplete) return -1;
    static int i = 0;
    static time_t talking = 0;
    static time_t silence = 0;
    static float talkingThreshold = 0.000750;
    static bool wasTalking = false;

    PaError err = 0;
    Pa_ReadStream(stream, sampleBlock->snippet, FRAMES_PER_BUFFER);
    
    float talkingIntensity = rms(sampleBlock->snippet, FRAMES_PER_BUFFER);
    
    if (err) return err;
    
    if(talkingIntensity > talkingThreshold) {
    	if (!wasTalking){
        wasTalking = true;
        talkingThreshold /= TALKING_TRIGGER_RATIO; 
      }

      talkingThreshold = adjustThreshold(talkingThreshold,talkingIntensity);
      printf("Listening: %d\n", i);
      i++;
      time(&talking);
      data->recordedSamples = realloc(data->recordedSamples, sampleBlock->size * i);
      data->size = sampleBlock->size * i;
        if (data->recordedSamples){
          size_t skipIndex = (i - 1) * sampleBlock->size;
          char *destination = (char*)data->recordedSamples + skipIndex;
          memcpy(destination, sampleBlock->snippet, sampleBlock->size);
        }
        else{
          free(data->recordedSamples);
          data->recordedSamples = NULL;
          data->size = 0;
        }
    } else { 
      if (wasTalking) {
        wasTalking = false;
        talkingThreshold *= TALKING_TRIGGER_RATIO; 
      }
      talkingThreshold = adjustThreshold(talkingThreshold,talkingIntensity);
      double test = difftime(time(&silence), talking);
      if (test >= 1.5 && test <= 10 && data->recordedSamples && i >= MIN_TALKING_BUFFERS){
          if (sampleComplete) *sampleComplete = true;
           
         // char buffer[100];
          //snprintf(buffer, 100, "file:%d.flac", i);
          storeFlac(data, fileName);
          talking = 0;
          free(data->recordedSamples);
          data->recordedSamples = NULL;
          data->size = 0;
          i = 0;
      }
    }
    return err;
}

int initializeRecording(PaStream **stream, AudioData *data, AudioSnippet *sampleBlock) {
    if (!stream || !data || !sampleBlock) return -1;

    PaError err = Pa_Initialize();
    if (err) return err;

    err = Pa_GetDeviceCount();
    debug_print("audio devices: %d\n", err);
    if (err < 0) { // PaErrorCode thrown
      fprintf(stdout, "%s\n", "Error retrieving device count");
      return err;
    }
    else if (err == 0){
      fprintf(stdout, "%s\n", "No audio devices available");
      return -1;
    }

    const PaDeviceInfo *info = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice());
    if (!info) {
      fprintf(stdout, "%s\n", "Unable to find info on default input device.");
      return -1;
    }

    PaStreamParameters inputParameters = {
      .device = Pa_GetDefaultInputDevice(),
      .channelCount = data->numberOfChannels,
      .sampleFormat = data->formatType,
      .suggestedLatency = info->defaultHighInputLatency,
      .hostApiSpecificStreamInfo = NULL
    };
    err = Pa_OpenStream(stream, &inputParameters, NULL, data->sampleRate, FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
    if (err) return err;

    sampleBlock->size = FRAMES_PER_BUFFER * sizeof(float) * data->numberOfChannels;
    sampleBlock->snippet = malloc(sampleBlock->size);

    return Pa_StartStream(*stream);
}

int recording(PaStream *stream, AudioData *data, AudioSnippet *sampleBlock, const char *fileName, bool *sampleComplete) {
  int err = 0;
  size_t len;
  while(err == 0 && *sampleComplete == false) {
    err = processStream(stream, data, sampleBlock, fileName, sampleComplete);
  }
  return err;
}

int run(const char *fileName) {
  AudioData *data = allocAudioData();
  AudioSnippet *sampleBlock = &((AudioSnippet){
                                  .size = 0,
                                  .snippet = NULL
                                });
  PaStream *stream = NULL;
  time_t talking = 0;
  time_t silence = 0;
   
  PaError err = paNoError;
  err = initializeRecording(&stream, data, sampleBlock);
  if (err){
    fprintf(stderr, "%s\n", "error");
    return err;
  }
  bool sampleComplete = false;
  while (!err && !sampleComplete) {
    err = recording(stream, data, sampleBlock, fileName, &sampleComplete);
  }
  return err;
}

int record(const char *fileName) {
  run(fileName); 
  return 1;  
}