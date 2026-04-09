# Pro Keys Preview
A VST3 visualiser for Rock Band 3 Pro Keys charts in REAPER.
<img width="743" height="556" alt="image" src="https://github.com/user-attachments/assets/0c72fcd2-d869-4e75-942b-5c05710639a9" />

Windows, Linux and MacOS versions are available. The Linux and MacOS versions haven't been tested, please create an issue if you run into problems.
The plugin was created and tested using REAPER 7. When using older versions, your mileage may vary.

## Installation
- Download and copy the ProKeysPreview.vst3 folder to wherever REAPER looks for your VST3 plugins.
    - On Windows, this is likely  `C:\Program Files\Common Files\VST3`
- Add the plugin to the FX for track you would like to preview.

## Settings
- Difficulty
    - The plugin assumes it has been placed in the FX window of the Expert difficulty.
    - When placed here, you can switch to the other tracks using the Difficulty setting, which automatically adjusts track speeds.
      - This mimics the functionality of RBN Preview.
    - Alternatively, you can place the plugin on all the different tracks and use the `This track` difficulty, which won't adjust the track speed.
- Time Offset (ms)
    - The plugin will have some latency. It's likely to be different from RBN Preview.
    - Adjust until playback looks accurate.
- Refresh Rate (Hz)
    - This is how often the plugin updates. It is recommended to use the refresh rate of your display.
- Speed
    - This is the track speed.
    - It is designed to be equivalent to Rock Band 3 track speeds.

## Build
This project uses Projucer and JUCE 8.0.12.

1. Initialize the JUCE submodule:

```bash
git submodule update --init --recursive
```

2. Open `ProKeysPreview.jucer` in Projucer and save, or run Projucer headless. If you only have JUCE source, build Projucer from `JUCE/extras/Projucer` first.

```bash
# Windows (PowerShell)
JUCE\extras\Projucer\Builds\VisualStudio2022\x64\Release\App\Projucer.exe --resave ProKeysPreview.jucer

# macOS
./JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app/Contents/MacOS/Projucer --resave ProKeysPreview.jucer

# Linux
./JUCE/extras/Projucer/Builds/LinuxMakefile/build/Projucer --resave ProKeysPreview.jucer
```

3. Build the generated projects:

```bash
# Windows
msbuild Builds/VisualStudio2026/ProKeysPreview.sln /p:Configuration=Release /p:Platform=x64

# macOS
xcodebuild -project Builds/MacOSX/ProKeysPreview.xcodeproj -configuration Release -target ProKeysPreview

# Linux
make -C Builds/LinuxMakefile CONFIG=Release
```

REAPER SDK headers are included in `Source/extern` under the original license.

    - Glissandos that have erroenous notes show those notes highlighted in red
- Chord detection
- Sustain logic
- Beat lines
