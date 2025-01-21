#include "FLinuxPortAudioCaptureFactory.h"
#include "FLinuxPortAudioCaptureStream.h"
#include "AudioCaptureCoreLog.h"

namespace Audio
{
    TUniquePtr<IAudioCaptureStream> FLinuxPortAudioCaptureFactory::CreateNewAudioCaptureStream()
    {
	if (GEngine && GEngine->UseSound())
	{
		UE_LOG(LogAudioCaptureCore, Log, TEXT("FLinuxPortAudioCaptureFactory::CreateNewAudioCaptureStream called"));
		TUniquePtr<IAudioCaptureStream> NewStream = MakeUnique<FLinuxPortAudioCaptureStream>();
		
		TArray<FCaptureDeviceInfo> AvailableDevices;
		if (NewStream->GetInputDevicesAvailable(AvailableDevices))
		{
			UE_LOG(LogAudioCaptureCore, Log, TEXT("Found %d capture devices:"), AvailableDevices.Num());
			for (const auto& Device : AvailableDevices)
			{
				UE_LOG(LogAudioCaptureCore, Log, TEXT("- Device: %s (ID: %s, Channels: %d, Rate: %d Hz)"),
					*Device.DeviceName, *Device.DeviceId, Device.InputChannels, Device.PreferredSampleRate);
			}
		}
		else
		{
			UE_LOG(LogAudioCaptureCore, Error, TEXT("No audio capture devices found."));
		}

		if (NewStream.IsValid())
			{
				UE_LOG(LogAudioCaptureCore, Log, TEXT("FLinuxPortAudioCaptureFactory::CreateNewAudioCaptureStream created a valid stream"));
			}
			else
			{
				UE_LOG(LogAudioCaptureCore, Error, TEXT("FLinuxPortAudioCaptureFactory::CreateNewAudioCaptureStream failed to create a valid stream"));
			}
		    return MoveTemp(NewStream);
		}
		else
		{
			UE_LOG(LogAudioCaptureCore, Error, TEXT("FLinuxPortAudioCaptureFactory::CreateNewAudioCaptureStream failed to create a valid stream, GEngine is missing."));
		}

		// Always return something, even in the error case
		return nullptr;
    }
}
