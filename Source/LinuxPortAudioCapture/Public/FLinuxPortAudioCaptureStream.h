#pragma once

#include "AudioCaptureDeviceInterface.h"
#include <atomic>
#include <thread>
THIRD_PARTY_INCLUDES_START
#include <portaudio.h>
THIRD_PARTY_INCLUDES_END

// Forward declaration of PortAudio types
//typedef struct PaStream PaStream;

namespace Audio
{
	/**
	 * Minimal PortAudio-based Audio Capture Stream for Unreal
	 */
	class FLinuxPortAudioCaptureStream : public IAudioCaptureStream
	{
	public:
		FLinuxPortAudioCaptureStream();
		virtual ~FLinuxPortAudioCaptureStream();

		// ~Begin IAudioCaptureStream overrides
		virtual bool RegisterUser(const TCHAR* UserId) override;
		virtual bool UnregisterUser(const TCHAR* UserId) override;
		virtual bool GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex) override;
		virtual bool OpenAudioCaptureStream(const FAudioCaptureDeviceParams& InParams, FOnAudioCaptureFunction InOnCapture, uint32 NumFramesDesired) override;
		virtual bool CloseStream() override;
		virtual bool StartStream() override;
		virtual bool StopStream() override;
		virtual bool AbortStream() override;
		virtual bool GetStreamTime(double& OutStreamTime) override;
		virtual int32 GetSampleRate() const override;
		virtual bool IsStreamOpen() const override;
		virtual bool IsCapturing() const override;
		virtual void OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow) override;
		virtual bool GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices) override;
		// ~End IAudioCaptureStream overrides

	private:
		bool InitializePortAudio(const FAudioCaptureDeviceParams& InParams, uint32 NumFramesDesired);
		void CaptureThreadLoop();

	private:
		// PortAudio stream object
		PaStream* Stream;

		// Whether the stream is currently open
		bool bStreamOpen;

		// Whether we are actively capturing
		std::atomic<bool> bShouldCapture;

		// UE callback to return audio data
		FOnAudioCaptureFunction OnCaptureCallback;

		// Actual sample rate (PortAudio may differ from requested if device doesn't support it)
		int32 ActualSampleRate;

		// Number of channels
		int32 NumChannels;

		// Worker thread for pulling audio from PortAudio
		std::thread CaptureThread;

		// Used to track how long the stream has been capturing
		double StartTimeSec;
	};
} // namespace Audio
