//
// Created by rain on 13.6.15.
//

#ifndef DTW_RECORD_H
#define DTW_RECORD_H


/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>


class Record{
private:
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned val;
    short int *record;

public:
    Record();
    int initRecord();
    int recordSound();

};

#endif //DTW_RECORD_H

