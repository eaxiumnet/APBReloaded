# APB SDK - Complete Development Environment (2025 Update Required)

## Overview
This SDK provides a complete development environment for APB Reloaded with multiple tools and pre-generated class definitions. The SDK includes:
- Runtime pattern scanner with discovered offsets (CURRENT)
- Pre-generated class definitions from 2020 (OBSOLETE)
- Multiple SDK generator tools (UP-TO-DATE)
- Core SDK header with helper functions

## CRITICAL: SDK Update Required

⚠️ **IMPORTANT**: The pre-generated SDK in the `SDK/` folder is from 2020 and is NOT compatible with the current 2025 version of APB. You must regenerate the SDK to use it with the current game version.

## Current Configuration (Runtime Scanner - Current)
- **GObjects Offset**: 0x3CFFC70 (Found at: 0x143CFFC70) - *Current*
- **GNames Offset**: 0x3C7FAD0 (Found at: 0x143C7FAD0) - *Current*
- **Process**: APB.exe (PID: 16984)
- **Image Base**: 0x140000000
- **Pre-generated SDK**: Available in `SDK/` folder but OUTDATED (from 2020)

## Complete Toolset Available

### 1. Pre-Generated SDK (OBSOLETE - FROM 2020)
The `SDK/` folder contains class definitions from 2020:
- `APBGame.h` / `APBGame.cpp` - Core APB game classes (2020 version)
- `Engine.h` / `Engine.cpp` - UE3 engine classes (2020 version)
- `Core.h` / `Core.cpp` - Core UE3 classes (2020 version)
- Other modules: `GFxUI`, `IpDrv`, `AKAudio`, etc. (2020 versions)
- **⚠️ DO NOT USE - THESE ARE OUTDATED AND WILL NOT WORK WITH CURRENT APB**

### 2. CodeRed Generator (UP-TO-DATE)
- Modern C++20 UE3 SDK generator
- Located in `CodeRed-Generator-main/`
- Advanced features for SDK generation
- **RECOMMENDED FOR GENERATING CURRENT SDK**

### 3. UnrealEngineSDKGenerator (UP-TO-DATE)
- Multi-engine SDK generator (UE1-UE4)
- Located in `UnrealEngineSDKGenerator-master/`
- APB-specific configuration in `Target/AllPointsBulletin/`
- **RECOMMENDED FOR GENERATING CURRENT SDK**

### 4. UE3 SDK Generator (UP-TO-DATE)
- Modern UE3 SDK generator
- Located in `ue3-sdkgen-master/`
- CMake-based build system

## Setup Instructions

### 1. Driver Setup
1. Run Command Prompt as Administrator
2. Navigate to the SDK folder: `cd C:\Users\Support\Desktop\apbsdk`
3. Load the driver: `LoadDriver.bat`
4. Keep the driver loaded while using the SDK

### 2. Game Setup
1. Launch APB Reloaded
2. Log into a district/character
3. Keep the game running while using the SDK

## Using the Current SDK (Runtime Scanner - Recommended)

Since the pre-generated SDK is outdated, use the runtime scanner based SDK:
1. Include `APB_SDK.hpp` in your C++ projects
2. Use the helper functions provided
3. This uses the current runtime-discovered offsets

## Regenerating Current SDK (Required)

### Using CodeRed Generator (Recommended):
1. Open `CodeRed-Generator-main\CodeRedGenerator.sln` in Visual Studio
2. Build the project in Release mode
3. Run the generator while APB is running
4. Generated SDK will replace outdated files in output folder

### Using UnrealEngineSDKGenerator:
1. Open `UnrealEngineSDKGenerator-master\UnrealTournament4.vcxproj` in Visual Studio
2. Build the project
3. Run the generator while APB is running
4. Generated SDK will be in the output folder

## Testing the Current SDK

1. Compile the test program:
   ```
   g++ -o TestSDK.exe TestSDK.cpp -static
   ```

   Or run: `CompileTest.bat`

2. The test will show:
   - Global object count and first few objects (using current offsets)
   - Name system functionality
   - SDK initialization status

## Key Features

### Core Structures
- `UObject`: Base class for all game objects
- `FName`: Name system with string lookup
- `UClass`: Class definitions
- `UFunction`: Function definitions
- `TArray<T>`: Generic array template

### Helper Functions
- `UObject::GetGlobalObjects()`: Access all game objects
- `UObject::FindObject(name)`: Find object by name
- `UObject::GetObjectCasted(index)`: Get object by index
- `obj->GetFullName()`: Get full name of object
- `obj->IsA(class)`: Check object type

## Example Usage (Current SDK)

```cpp
#include "APB_SDK.hpp"  // Use the runtime scanner SDK (current)

int main() {
    // Access all objects in the game using current offsets
    auto& objects = UObject::GetGlobalObjects();
    std::cout << "Total objects: " << objects.Num() << std::endl;

    // Find a specific object
    auto playerController = UObject::FindObject<UObject>("APBPlayerController");

    // Iterate through objects
    for (int i = 0; i < objects.Num() && i < 100; ++i) {
        auto obj = objects[i];
        if (obj) {
            std::cout << obj->GetFullName() << std::endl;
        }
    }

    return 0;
}
```

## Tools Included

### ReClass.NET Integration
- Use `ReClass.NET_Launcher.exe` for memory analysis
- Load the generated SDK structures into ReClass
- Visualize memory layout of game objects

### Pattern Scanner
- `Runtime_Pattern_Scanner.cpp` - Finds GObjects/GNames at runtime
- No hardcoded offsets required
- Works across game updates
- **Current and functional**

## Important Notes

- The SDK requires the KsDumper driver to be loaded
- Always run as Administrator when loading drivers
- Disable antivirus temporarily during driver loading
- Secure Boot must be disabled in BIOS
- **⚠️ The pre-generated SDK in `SDK/` folder is from 2020 and MUST BE REGENERATED**
- Use the runtime scanner SDK or regenerate the pre-generated SDK for current APB version
- This is for educational/research purposes only

## Troubleshooting

- If the SDK stops working after a game update, run the pattern scanner again
- Ensure the driver is properly loaded (use LoadDriver.bat)
- Make sure APB is running before accessing memory
- Check that the game process is accessible
- **⚠️ For complete class definitions, REGENERATE the SDK using one of the generator tools**
- The 2020 SDK files in the `SDK/` folder will not work with current APB