#pragma once

#include "AudioCaptureDeviceInterface.h"
#include "FLinuxPortAudioCaptureStream.h"

namespace Audio
{
	class FLinuxPortAudioCaptureFactory : public IAudioCaptureFactory
	{
	public:
		virtual ~FLinuxPortAudioCaptureFactory() {}

		virtual TUniquePtr<IAudioCaptureStream> CreateNewAudioCaptureStream() override;
	};
}
