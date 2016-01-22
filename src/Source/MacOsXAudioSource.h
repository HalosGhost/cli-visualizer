/*
 * MacOsXAudioSource.h
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 */

#ifndef _VIS_MAC_OS_X_AUDIO_SOURCE_H
#define _VIS_MAC_OS_X_AUDIO_SOURCE_H

#include <cstdint>
#include "Domain/VisTypes.h"
#include "Source/AudioSource.h"
#include "Domain/Settings.h"

#ifdef _OS_OSX
#include <thread>
#include <CoreAudio/CoreAudio.h>
#endif

namespace vis
{

class MacOsXAudioSource : public vis::AudioSource
{
  public:
    explicit MacOsXAudioSource(const vis::Settings *const settings);

    ~MacOsXAudioSource() override;

    /**
     * Reads "buffer_size" frames of the audio stream into "buffer"
     */
    bool read(pcm_stereo_sample *buffer, uint32_t buffer_size) override;

#ifdef _OS_OSX
    AudioDeviceID get_default_input_device();

    void print_device_info(const AudioDeviceID id);

    void print_stream_info(AudioDeviceID device_id);
#endif

  private:
#ifdef _OS_OSX

#define CORE_AUDIO_OSX_CHANNEL 0

    struct VisCoreAudioData
    {
        int sample;
        int freq;
        AudioStreamBasicDescription description;
    };

    AudioDeviceID m_device_id;

    AudioDeviceIOProcID m_proc_id;

    std::thread m_run_loop_thread;

    std::vector<float *> m_buffers;

    const vis::Settings * const m_settings;

    bool start_core_audio();

    static OSStatus core_audio_handle(AudioDeviceID inDevice,
                               const AudioTimeStamp *inNow,
                               const AudioBufferList *inInputData,
                               const AudioTimeStamp *inInputTime,
                               AudioBufferList *outOutputData,
                               const AudioTimeStamp *inOutputTime,
                               void *inClientData);

    static void start_run_loop();
#endif
};
}

#endif
