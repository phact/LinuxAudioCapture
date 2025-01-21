using UnrealBuildTool;
using System.IO;

public class LinuxPortAudioCapture : ModuleRules
{
    public LinuxPortAudioCapture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AudioCaptureCore" });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            // Link PortAudio
            PublicIncludePaths.Add("/usr/include/");
            PublicSystemLibraries.Add("portaudio");
			PublicAdditionalLibraries.Add(@"/usr/lib/x86_64-linux-gnu/libportaudio.so");
        }

    }
}
