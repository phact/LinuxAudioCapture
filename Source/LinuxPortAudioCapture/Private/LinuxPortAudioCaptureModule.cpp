#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "FLinuxPortAudioCaptureFactory.h"
THIRD_PARTY_INCLUDES_START
#include <portaudio.h> // include PortAudio here
THIRD_PARTY_INCLUDES_END

class FLinuxPortAudioCaptureModule : public IModuleInterface
{
private:
    TSharedPtr<Audio::FLinuxPortAudioCaptureFactory> CaptureFactory;

public:
    virtual void StartupModule() override
    {
        // (1) Initialize PortAudio globally one time
        PaError Err = Pa_Initialize();
        if (Err != paNoError)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Pa_Initialize error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
        }
        
        // (2) Create and register the capture factory
        CaptureFactory = MakeShared<Audio::FLinuxPortAudioCaptureFactory>();
        IModularFeatures::Get().RegisterModularFeature(
            Audio::IAudioCaptureFactory::GetModularFeatureName(),
            CaptureFactory.Get()
        );

        UE_LOG(LogTemp, Log, TEXT("FLinuxPortAudioCaptureModule: Factory Registered"));
    }

    virtual void ShutdownModule() override
    {
        // (1) Unregister our capture factory
        if (CaptureFactory.IsValid())
        {
            IModularFeatures::Get().UnregisterModularFeature(
                Audio::IAudioCaptureFactory::GetModularFeatureName(),
                CaptureFactory.Get()
            );

            CaptureFactory.Reset();
            UE_LOG(LogTemp, Log, TEXT("FLinuxPortAudioCaptureModule: Factory Unregistered"));
        }

        // (2) Terminate PortAudio once when shutting down
        PaError Err = Pa_Terminate();
        if (Err != paNoError)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Pa_Terminate error: %s"), ANSI_TO_TCHAR(Pa_GetErrorText(Err)));
        }
    }
};

// Macro to implement the module
IMPLEMENT_MODULE(FLinuxPortAudioCaptureModule, LinuxPortAudioCapture)