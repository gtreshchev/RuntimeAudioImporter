<a href="https://github.com/gtreshchev/RuntimeAudioImporter/blob/main/LICENSE">![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)</a>
<a href="https://georgy.dev/discord">![Discord](https://img.shields.io/discord/1055168498919284786.svg?label=Discord&logo=discord&color=7289DA&labelColor=2C2F33)</a>
<a href="https://www.unrealengine.com/">![Unreal Engine](https://img.shields.io/badge/Unreal-4.24%2B-dea309)</a>

<br/>
<p align="center">
  <a href="https://github.com/gtreshchev/RuntimeAudioImporter">
    <img src="Resources/Icon128.png" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Runtime Audio Importer</h3>

  <p align="center">
    Importing audio of various formats at runtime
    <br/>
    <br/>
    <a href="https://github.com/gtreshchev/RuntimeAudioImporter/wiki"><strong>Explore the docs »</strong></a>
    <br/>
    <br/>
    <a href="https://unrealengine.com/marketplace/product/runtime-audio-importer">Marketplace</a>
    .
    <a href="https://github.com/gtreshchev/RuntimeAudioImporter/releases">Releases</a>
    <br/>
    <a href="https://georgy.dev/discord">Discord support chat</a>
  </p>

## Features

- Fast transcoding speed
- Supported for major audio formats: MP3, WAV, FLAC, OGG Vorbis and BINK
- Supported for RAW formats: int8, uint8, int16, uint16, int32, uint32, float32
- Same behavior as regular sound wave, including SoundCue, MetaSounds (starting from 5.3), etc
- Automatic detection of audio format
- Audio streaming functionality
- Audio capturing from input devices (eg microphone)
- Voice Activity Detection (VAD)
- Exporting a sound wave to a separate file
- Pre-imported sound assets
- No static libraries or external dependencies
- Cross-platform compatibility (Windows, Mac, Linux, Android, iOS, etc)

## Additional information

MP3, WAV, and FLAC audio transcoding operations are powered by [dr_libs](https://github.com/mackron/dr_libs) and [minimp3](https://github.com/lieff/minimp3).

VAD (Voice Activity Detection) is powered by [libfvad](https://github.com/dpirch/libfvad).

## Legal info

Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.

Unreal® Engine, Copyright 1998 – 2024, Epic Games, Inc. All rights reserved.
