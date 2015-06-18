//
// Created by rain on 13.6.15.
//

#include <iostream>
#include "record.h"
#include "check.h"

using namespace std;

Record::Record(){

}

int Record::initRecord(){
    int rc;
    int dir;
    snd_pcm_uframes_t frames;

    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&handle, "default",
                      SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        cerr << "unable to open pcm device: " << snd_strerror(rc) << endl;
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params,
                                 SND_PCM_FORMAT_S16_LE);

    /* Two channels (stereo) */
    //snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params_set_channels(handle, params, 2);

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params,
                                    &val, &dir);

    /* Set period size to 32 frames. */
    frames = 32;
    snd_pcm_hw_params_set_period_size_near(handle,
                                           params, &frames, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        cerr << "unable to set hw parameters: " << snd_strerror(rc) << endl;
        return -1;
    }
    return 0;
}

int Record::recordSound(){
    long loops;
    int rc;
    int size;
    int dir;
    snd_pcm_uframes_t frames;
    char *buffer;

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params,
                                      &frames, &dir);
    size = frames * 4; /* 2 bytes/sample, 2 channels */
    buffer = (char *) malloc(size);

    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params,
                                      &val, &dir);
    loops = 5000000 / val;

    record = (short int *) malloc(size*loops);

    while (loops > 0) {
        loops--;
        rc = snd_pcm_readi(handle, buffer, frames);
        if (rc == -EPIPE) {
            /* EPIPE means overrun */
            cerr << "overrun occurred" << endl;
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            cerr << "error from read: " << snd_strerror(rc) << endl;
        } else if (rc != (int) frames) {
            cerr << "short read, read " << rc << " frames" << endl;
        }
        //rc = write(1, buffer, size);
        memcpy(record + loops * size, buffer, size);

        if (rc != size)
            cerr << "short write: wrote " << rc << " bytes" << endl;
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    free(buffer);
    free(record);
    return 0;
}



