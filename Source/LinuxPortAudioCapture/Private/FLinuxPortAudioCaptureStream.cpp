#include "FLinuxPortAudioCaptureStream.h"
THIRD_PARTY_INCLUDES_START
#include <portaudio.h>
#include <chrono>
THIRD_PARTY_INCLUDES_END

#include "AudioCaptureCoreLog.h"

static double GetTimeSec()
{
    using namespace std::chrono;
    static auto Start = high_resolution_clock::now();
    auto Now = high_resolution_clock::now();
    return duration<double>(Now - Start).count();
}

namespace Audio
{
    FLinuxPortAudioCaptureStream::FLinuxPortAudioCaptureStream()
        : Stream(nullptr)
        , bStreamOpen(false)
        , bShouldCapture(false)
        , ActualSampleRate(0)
        , NumChannels(0)
        , StartTimeSec(0.0)
    {
    }

    FLinuxPortAudioCaptureStream::~FLinuxPortAudioCaptureStream()
    {
        AbortStream();
        CloseStream();
    }

    bool FLinuxPortAudioCaptureStream::RegisterUser(const TCHAR* UserId)
    {
        // Optionally track user
        return true;
    }

    bool FLinuxPortAudioCaptureStream::UnregisterUser(const TCHAR* UserId)
    {
        // Optionally un-track user
        return true;
    }

    bool FLinuxPortAudioCaptureStream::GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex)
    {
        OutInfo.DeviceName          = TEXT("PortAudio Default Input");
        OutInfo.DeviceId            = TEXT("0");
        OutInfo.InputChannels       = 1;
        OutInfo.PreferredSampleRate = 16000;
        OutInfo.bSupportsHardwareAEC = false;
        return true;
    }

    bool FLinuxPortAudioCaptureStream::OpenAudioCaptureStream(
        const FAudioCaptureDeviceParams& InParams,
        FOnAudioCaptureFunction InOnCapture,
        uint32 NumFramesDesired)
    {
        if (bStreamOpen)
        {
            UE_LOG(LogAudioCaptureCore, Warning, TEXT("PortAudio stream already open."));
            return false;
        }

        OnCaptureCallback = InOnCapture;

        // We only open the stream here, not re-initialize the entire PortAudio library
        if (!InitializePortAudio(InParams, NumFramesDesired))
        {
            UE_LOG(LogAudioCaptureCore, Error,
                   TEXT("Failed to open PortAudio capture stream"));
            return false;
        }

        bStreamOpen = true;
        return true;
    }

    bool FLinuxPortAudioCaptureStream::CloseStream()
    {
        if (!bStreamOpen)
        {
            return false;
        }

        StopStream(); // Ensure capturing is stopped first

        if (Stream)
        {
            PaError Err = Pa_CloseStream(Stream);
            if (Err != paNoError)
            {
                UE_LOG(LogAudioCaptureCore, Error,
                    TEXT("Pa_CloseStream error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
            }
            Stream = nullptr;
        }

        // Removed Pa_Terminate() call — that now happens in ShutdownModule()

        bStreamOpen = false;
        return true;
    }

    bool FLinuxPortAudioCaptureStream::StartStream()
    {
        if (!bStreamOpen || !Stream)
        {
            UE_LOG(LogAudioCaptureCore, Warning,
                   TEXT("PortAudio stream not open, cannot start."));
            return false;
        }

        PaError Err = Pa_StartStream(Stream);
        if (Err != paNoError)
        {
            UE_LOG(LogAudioCaptureCore, Error,
                TEXT("Pa_StartStream error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
            return false;
        }

        bShouldCapture = true;
        StartTimeSec   = GetTimeSec();
        CaptureThread  = std::thread(&FLinuxPortAudioCaptureStream::CaptureThreadLoop, this);

        return true;
    }

    bool FLinuxPortAudioCaptureStream::StopStream()
    {
        if (!bShouldCapture)
        {
            return false; // already stopped
        }

        bShouldCapture = false;
        if (CaptureThread.joinable())
        {
            CaptureThread.join();
        }

        if (Stream)
        {
            PaError Err = Pa_StopStream(Stream);
            if (Err != paNoError)
            {
                UE_LOG(LogAudioCaptureCore, Error,
                    TEXT("Pa_StopStream error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
            }
        }

        return true;
    }

    bool FLinuxPortAudioCaptureStream::AbortStream()
    {
        // In this minimal example, same as StopStream
        return StopStream();
    }

    bool FLinuxPortAudioCaptureStream::GetStreamTime(double& OutStreamTime)
    {
        if (!bShouldCapture)
        {
            OutStreamTime = 0.0;
            return false;
        }

        OutStreamTime = GetTimeSec() - StartTimeSec;
        return true;
    }

    int32 FLinuxPortAudioCaptureStream::GetSampleRate() const
    {
        return ActualSampleRate;
    }

    bool FLinuxPortAudioCaptureStream::IsStreamOpen() const
    {
        return bStreamOpen;
    }

    bool FLinuxPortAudioCaptureStream::IsCapturing() const
    {
        return bShouldCapture;
    }

    void FLinuxPortAudioCaptureStream::OnAudioCapture(
        void* InBuffer,
        uint32 InBufferFrames,
        double StreamTime,
        bool bOverflow)
    {
        // We won't call this ourselves in blocking mode. We directly call OnCaptureCallback in CaptureThreadLoop().
    }

    bool FLinuxPortAudioCaptureStream::GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices)
    {
        int NumDevices = Pa_GetDeviceCount();
        if (NumDevices < 0)
        {
            UE_LOG(LogAudioCaptureCore, Error, TEXT("Pa_GetDeviceCount error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(NumDevices)));
            return false;
        }

        for (int i = 0; i < NumDevices; ++i)
        {
            const PaDeviceInfo* DeviceInfo = Pa_GetDeviceInfo(i);
            if (DeviceInfo && DeviceInfo->maxInputChannels > 0)
            {
                FCaptureDeviceInfo CaptureDevice;
                CaptureDevice.DeviceName = ANSI_TO_TCHAR(DeviceInfo->name);
                CaptureDevice.DeviceId = FString::FromInt(i);
                CaptureDevice.InputChannels = DeviceInfo->maxInputChannels;
                CaptureDevice.PreferredSampleRate = DeviceInfo->defaultSampleRate;
                CaptureDevice.bSupportsHardwareAEC = false;

                OutDevices.Add(CaptureDevice);
            }
        }

        return OutDevices.Num() > 0;
    }

    bool FLinuxPortAudioCaptureStream::InitializePortAudio(
        const FAudioCaptureDeviceParams& InParams,
        uint32 NumFramesDesired)
    {
        PaDeviceIndex DeviceIndex = (InParams.DeviceIndex != INDEX_NONE) ? InParams.DeviceIndex : Pa_GetDefaultInputDevice();

        if (DeviceIndex == paNoDevice)
        {
            UE_LOG(LogAudioCaptureCore, Error, TEXT("No input device found."));
            return false;
        }

        const PaDeviceInfo* DeviceInfo = Pa_GetDeviceInfo(DeviceIndex);
        if (!DeviceInfo)
        {
            UE_LOG(LogAudioCaptureCore, Error, TEXT("Invalid device index: %d"), DeviceIndex);
            return false;
        }

        NumChannels = (InParams.NumInputChannels > 0) ? InParams.NumInputChannels : DeviceInfo->maxInputChannels;
        int32 RequestedSampleRate = (InParams.SampleRate > 0) ? InParams.SampleRate : DeviceInfo->defaultSampleRate;

        PaStreamParameters InputParams;
        FMemory::Memzero(&InputParams, sizeof(InputParams));
        InputParams.device = DeviceIndex;
        InputParams.channelCount = NumChannels;
        InputParams.sampleFormat = paFloat32; // Matches UE's float format
        InputParams.suggestedLatency = DeviceInfo->defaultLowInputLatency;
        // needed?
        InputParams.hostApiSpecificStreamInfo = nullptr;

        // Open PortAudio stream for blocking I/O
        PaError Err = Pa_OpenStream(
            &Stream,
            &InputParams,
            nullptr,               // no output
            (double)RequestedSampleRate,
            NumFramesDesired,
            paNoFlag,
            nullptr,               // no callback
            nullptr
        );

        if (Err != paNoError)
        {
            UE_LOG(LogAudioCaptureCore, Error,
                TEXT("Pa_OpenStream error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
            Stream = nullptr;
            return false;
        }

        // Update the actual sample rate
        ActualSampleRate = RequestedSampleRate;

        UE_LOG(LogAudioCaptureCore, Log,
               TEXT("PortAudio opened with %d channels at %d Hz"),
               NumChannels, ActualSampleRate);

        return true;
    }

    void FLinuxPortAudioCaptureStream::CaptureThreadLoop()
    {
        // Example buffer size
        static const int kFramesPerRead = 256;

        TArray<float> CaptureBuffer;
        CaptureBuffer.SetNumUninitialized(kFramesPerRead * NumChannels);

        while (bShouldCapture)
        {
            if (!Stream)
            {
                break;
            }

            PaError Err = Pa_ReadStream(Stream, CaptureBuffer.GetData(), kFramesPerRead);
            if (Err == paInputOverflowed)
            {
                UE_LOG(LogAudioCaptureCore, Warning,
                    TEXT("PortAudio input overflow"));
                continue;
            }
            else if (Err != paNoError)
            {
                UE_LOG(LogAudioCaptureCore, Warning,
                    TEXT("Pa_ReadStream error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
                continue;
            }

            double StreamTime = GetTimeSec() - StartTimeSec;
            bool bOverflow    = (Err == paInputOverflowed);

            // Deliver the audio frames to UE
            OnCaptureCallback(
                CaptureBuffer.GetData(),
                kFramesPerRead,
                NumChannels,
                ActualSampleRate,
                StreamTime,
                bOverflow
            );
        }
    }
}
