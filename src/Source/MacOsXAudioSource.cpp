/*
 * MacOsXAudioSource.cpp
 *
 * Created on: Jul 30, 2015
 *     Author: dpayne
 */

#include "Source/MacOsXAudioSource.h"
#include <iostream>

static std::mutex g_pages_mutex;
static std::vector<float *> g_buffers;

vis::MacOsXAudioSource::MacOsXAudioSource(const vis::Settings *const settings)
#ifdef _OS_OSX
    : m_device_id{0u}, m_proc_id{nullptr}, m_settings
{
    settings
}
#endif
{
#ifdef _OS_OSX
    start_core_audio();
#endif
    m_settings->get_mpd_fifo_path();
}

// TODO:
//   1. change cerr to VIS_LOG, remove iostream
//   2. Add buffer, make lock less

vis::MacOsXAudioSource::~MacOsXAudioSource()
{
#ifdef _OS_OSX

    // TODO: reap thread
    CFRunLoopStop(CFRunLoopGetCurrent());

    m_run_loop_thread.join();

    AudioDeviceDestroyIOProcID(m_device_id, m_proc_id);
    AudioDeviceStop(m_device_id, m_proc_id);
#endif
}

#ifdef _OS_OSX

bool vis::MacOsXAudioSource::start_core_audio()
{
    // get default input device
    // setup handle
    // start CFRun thread

    m_device_id = get_default_input_device();
    print_device_info(m_device_id);

    VisCoreAudioData core_audio_data;
    core_audio_data.sample = 0;

    auto result = AudioDeviceCreateIOProcID(
        m_device_id, &vis::MacOsXAudioSource::core_audio_handle,
        &core_audio_data, &m_proc_id);

    if (result != noErr)
    {
        std::cerr << "AudioDeviceAddIOProc: " << result << std::endl;
        return 1;
    }

    result = AudioDeviceStart(m_device_id, m_proc_id);

    if (result != noErr)
    {
        std::cerr << "Error starting process: " << result << std::endl;
        return 1;
    }

    m_run_loop_thread = std::thread(start_run_loop);

    return true;
}

void vis::MacOsXAudioSource::start_run_loop()
{
    CFRunLoopRun();
}

AudioDeviceID vis::MacOsXAudioSource::get_default_input_device()
{
    AudioDeviceID device_id;
    UInt32 property_size = sizeof(device_id);

    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

    // get the ID of the default input device
    auto result = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &property_address, CORE_AUDIO_OSX_CHANNEL,
        nullptr, &property_size, &device_id);
    if (result != noErr)
    {
        std::cerr << "Error getting default device" << result << std::endl;
        return 0;
    }

    return device_id;
}

OSStatus vis::MacOsXAudioSource::core_audio_handle(
    AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *inInputData,
    const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, void *)
{
    const AudioBuffer *buf = &inInputData->mBuffers[0];

    uint32_t units = buf->mDataByteSize / sizeof(float);
    float *data = static_cast<float *>(buf->mData);

    float *new_buffer = static_cast<float *>(calloc(units, sizeof(float)));
    memcpy(new_buffer, data, units * sizeof(float));

    g_pages_mutex.lock();
    if (g_buffers.size() > 100 )
    {
        for ( auto i = 0u; i < g_buffers.size(); ++i)
        {
            free(g_buffers[i]);
        }
        g_buffers.clear();
    }

    g_buffers.push_back(new_buffer);
    g_pages_mutex.unlock();

    if (units > 0)
    {
        for (auto i = 0u; i < units; ++i)
        {
            std::cerr << new_buffer[i] << ' ';
        }
        std::cerr << std::endl;
    }

    return 0;
}

void vis::MacOsXAudioSource::print_stream_info(AudioDeviceID device_id)
{
    AudioObjectPropertyAddress device_address;

    device_address.mSelector = kAudioStreamPropertyPhysicalFormat;
    device_address.mScope = kAudioDevicePropertyScopeInput;
    device_address.mElement = kAudioObjectPropertyElementMaster;

    AudioStreamBasicDescription description;
    UInt32 property_size = sizeof(AudioStreamBasicDescription);

    auto result = AudioObjectGetPropertyData(device_id, &device_address,
                                             CORE_AUDIO_OSX_CHANNEL, nullptr,
                                             &property_size, &description);

    if (result != noErr)
    {
        std::cerr << "Failed to get info on stream: " << device_id << std::endl;
    }
    else
    {
        std::cerr << "Stream for device: " << device_id << std::endl;
        std::cerr << "\tSample rate: " << description.mSampleRate << std::endl;
        std::cerr << "\tmFormatID: " << description.mFormatID << std::endl;
    }
}

void vis::MacOsXAudioSource::print_device_info(const AudioDeviceID id)
{
    char deviceName[64];
    char manufacturerName[64];

    UInt32 propertySize = sizeof(deviceName);
    AudioObjectPropertyAddress deviceAddress;

    deviceAddress.mSelector = kAudioDevicePropertyDeviceName;
    deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
    deviceAddress.mElement = kAudioObjectPropertyElementMaster;

    if (AudioObjectGetPropertyData(id, &deviceAddress, CORE_AUDIO_OSX_CHANNEL,
                                   nullptr, &propertySize, deviceName) == noErr)
    {

        propertySize = sizeof(manufacturerName);
        deviceAddress.mSelector = kAudioDevicePropertyDeviceManufacturer;
        deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
        deviceAddress.mElement = kAudioObjectPropertyElementMaster;

        if (AudioObjectGetPropertyData(
                id, &deviceAddress, CORE_AUDIO_OSX_CHANNEL, nullptr,
                &propertySize, manufacturerName) == noErr)
        {
            CFStringRef uidString;

            propertySize = sizeof(uidString);
            deviceAddress.mSelector = kAudioDevicePropertyDeviceUID;
            deviceAddress.mScope = kAudioObjectPropertyScopeGlobal;
            deviceAddress.mElement = kAudioObjectPropertyElementMaster;

            if (AudioObjectGetPropertyData(id, &deviceAddress,
                                           CORE_AUDIO_OSX_CHANNEL, nullptr,
                                           &propertySize, &uidString) == noErr)
            {
                std::cerr << "device " << deviceName << "\t\t"
                          << manufacturerName << "\t\t" << uidString
                          << std::endl;

                CFRelease(uidString);
            }
        }
    }
    else
    {
        std::cerr << "Could not get device name" << std::endl;
    }
}
#endif

bool vis::MacOsXAudioSource::read(pcm_stereo_sample *buffer,
                                  const uint32_t buffer_size)
{
#ifdef _OS_OSX
    // Read from buffer if available, otherwise return false
    auto buffers_to_read = buffer_size / 512u;

    for (auto j = 0u; j < buffers_to_read; ++j)
    {
        float *core_audio_buffer{nullptr};
        g_pages_mutex.lock();
        if (!g_buffers.empty())
        {
            core_audio_buffer = g_buffers.front();
            g_buffers.erase(g_buffers.begin());
        }
        g_pages_mutex.unlock();

        if (core_audio_buffer == nullptr)
        {
            break;
        }

        if (512 > buffer_size)
        {
            return false;
        }

        for (auto i = 0u; i < 512u; ++i)
        {
            //convert from linear PCM float to 16-bit PCM
            buffer[i + (j * 512u)].l =
                static_cast<int16_t>(32768.0*core_audio_buffer[i]);
        }

        free(core_audio_buffer);
    }

    return true;
#else
    if (buffer_size > 0 || buffer != nullptr)
    {
        return false;
    }

    return false;
#endif
}
