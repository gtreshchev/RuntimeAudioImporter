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
    <a href="https://docs.georgy.dev/runtime-audio-importer/overview"><strong>Explore the docs »</strong></a>
    <br/>
    <a href="https://www.fab.com/listings/66e0d72e-982f-4d9e-aaaf-13a1d22efad1">Fab</a>
    .
    <a href="https://georgy.dev/discord">Discord support chat</a>
  </p>

## Features

- Fast transcoding speed
- Supported for major audio formats: MP3, WAV, FLAC, OGG Vorbis, OGG Opus and BINK
- Supported for RAW formats: int8, uint8, int16, uint16, int32, uint32, float32
- Same behavior as regular sound wave, including SoundCue, MetaSounds (starting from 5.3), etc
- Automatic detection of audio format
- Audio streaming functionality
- Audio capturing from input devices (eg microphone), including within Pixel Streaming context
- Voice Activity Detection (VAD)
- Exporting a sound wave to a separate file
- Pre-imported sound assets
- No static libraries or external dependencies
- Cross-platform compatibility (Windows, Mac, Linux, Android, iOS, etc)

## Additional information

MP3, WAV, and FLAC audio transcoding operations are powered by [dr_libs](https://github.com/mackron/dr_libs) and [minimp3](https://github.com/lieff/minimp3).

OGG Opus audio decoding is powered by [opusfile](https://github.com/xiph/opusfile).

VAD (Voice Activity Detection) is powered by [libfvad](https://github.com/dpirch/libfvad).

## Like my work?

Consider [supporting me](https://ko-fi.com/georgydev). Hire me at [gtreshchev@gmail.com](mailto:gtreshchev@gmail.com).