# Asset audio

Sound effects, music and PCM streams through one hosted-capable call.
Read this when an application plays audio.

`kryon/Media` retains `MediaOpen`, pause/stop/state/message and text fetching.
It also exports `MediaAudio(session, command: MediaAudioCommand) ->
MediaAudioResult`. A hosted bundle uses `KRYON_HOSTED_UI` and requests only
`media_audio.MediaAudioHost` for this function. The same types/function are
available directly from `kryon/media_audio`.

| `MediaAudioCommand` field | Meaning |
| --- | --- |
| `operation: MediaAudioOperation` | `Capabilities`, `Load`, `Play`, `Gain`, `Stop`, `Unload`, `Status`, `CreateStream`, `SubmitStream`, `Pause` or `Resume` |
| `voice: s32` | Caller-selected `1..16`, local to one actual app instance. Each loaded voice has an independent decoder and gain, so several effects and music voices play together |
| `asset: string`, `music: bool`, `gain: float32` | `Load` uses these; unload an occupied ID before reloading it |
| `looping: bool` | `Play` starts from the beginning; `looping` is accepted only on a voice loaded with `music = true` |
| `gain: float32` | `Gain` changes only that voice. Gain must be finite and in `0..1`; out-of-range, NaN and infinite values are rejected, without clamping or changing playback. Zero-initialized gain means silence |
| PCM fields | `format`, `sample_rate`, `channels`, `frames_per_block` and `pcm` for streams (below) |

`Stop` retains a decoded voice for replay; `Unload` stops and frees it.
`Status` updates the stream and reports actual completion. `Capabilities`
does not need a voice ID. There is no queued playback or automatic transition.

`MediaAudioResult` owns `message: string` across the hosted boundary, with
`code: MediaAudioCode`, `state: MediaAudioState`, `capabilities: u32`, and
`writable: bool` for PCM streams, `has_position: bool`, and `position_seconds: float64`.

| `MediaAudioCode` | Meaning |
| --- | --- |
| `Ok` | Success |
| `Unsupported` | No provider |
| `Unavailable` | No device/reader or an operation already in flight |
| `Suspended` | Browser context cannot play, including before an autoplay-unlocking gesture |
| `Invalid`, `Denied`, `Missing`, `Limit` | Bad request, refused, unknown voice/resource, or a limit reached |
| `Failed` | Provider/decode failure |
| `Backpressure` | A full PCM stream |

States are `Unloaded`, `Ready`, `Playing`, `Stopped`, `Finished`, and `Paused`.
A failed command does not imply playing; inspect `code` first.

| Capability bit | Meaning |
| --- | --- |
| 1, 2, 4 | Asset effects, asset music and gain; providers keep these bits when the device is unavailable or suspended |
| 8 | Bounded PCM streams |
| 16, 32, 64 | Asset pause/resume, asset-music position, PCM pause/resume |

A partial provider need not implement these features; check its bits and
result code. Operation and state wire values stay stable; `Pause`, `Resume`
and `Paused` extend the lists.

`Pause` changes Playing to Paused, preserving the individual voice's offset,
loop, gain and queued PCM copies. Repeating Pause on Paused succeeds. `Resume`
changes Paused to Playing; repeating it on Playing succeeds only when the
device/context is available. Both operations reject Ready, Stopped and Finished
with Invalid, and unloaded IDs with Missing. Resume never starts a new voice or
replays a stopped one. Play remains an explicit restart from the beginning.
Neither operation suspends the shared device/context or another owner.

Asset-music Status sets `has_position` only when `position_seconds` contains a
finite, nonnegative provider playhead in seconds, within the current loop
iteration (it wraps at the loop boundary). Paused time freezes. Browser time
also freezes during external context suspension, using AudioContext.currentTime;
Status then reports Suspended with its available frozen position. Resume while
suspended returns Suspended and queues nothing; retry explicitly after unlocking.
PCM and effects have `has_position = false`; zero seconds alone never indicates
support. PCM exposes neither a drained counter nor an estimated elapsed duration.
Paused PCM is not writable and rejects submission with Invalid, preserving the
same bounded queued copies until Resume; Stop and Unload clear those copies.

```zi
using Media :: #import "kryon/Media";
// Once, after verifying the capability/result:
loaded := MediaAudio(session, MediaAudioCommand.{.operation = .Load,
    .voice = 1, .asset = "assets/my-game/chime.wav", .gain = .6})
if loaded.code == .Ok {
    played := MediaAudio(session, MediaAudioCommand.{.operation = .Play, .voice = 1})
    // Consume played.code in the application's audio policy.
}
```

`CreateStream` occupies one of the same 16 voices. Set `format: MediaAudioFormat`
to `S16LE` (signed little-endian 16-bit interleaved samples), `sample_rate` to
8000–48000 Hz, `channels` to 1 or 2, `frames_per_block` to 2048–8192, and
`gain` to a finite value in `0..1`. These are the provider's supported bounds;
no other PCM formats are advertised. Creation accepts no samples. `Play`
starts an empty stream; repeating it clears pending samples. `SubmitStream`
uses `pcm: string` as binary bytes, including NULs, and requires exactly
`frames_per_block * channels * 2` bytes (at most 32768). Samples are copied
before return; the guest retains no host pointer. Format/rate/channels are
fixed until `Unload`; submission uses the voice's creation settings.

`Status` returns `writable: bool`, true only for a running stream with room
for a complete block. `SubmitStream` returns `Backpressure` without accepting
any bytes when full; retry the same block later. `Ok` means the entire block
was copied. The provider holds at most two blocks in its own stream
buffers; no application command queue is added. `Stop` clears queued samples,
`Gain` changes only that voice, and `Unload` frees the stream. Streams remain
`Playing` across frames and underruns, emitting silence while empty: they are
open-ended and do not claim `Finished` or a precise drained count. Asset voices
keep their completion semantics. Suspended/unavailable providers
reject creation, play and submission without queuing them for later autoplay;
after a trusted browser gesture the app must retry explicitly. Owner reset,
revocation, close and successful restart dispose streams with asset voices.
Wurm's synthesis, phrase scheduling, crossfades and game policy stay in Wurm;
its mono 22050 Hz / 2048-frame transport fits these bounds.

Only packed paths under `assets/` pass: absolute paths, empty path components,
traversal, backslashes and schemes are refused before the host's
allowed-asset callback and packed reader. An asset is at most 8 MiB; there
are at most 16 loaded voices per owner and no command queue. Resource bytes
are copied/decoded into provider-owned storage; no native pointer or decoder
handle crosses the VM. The host reply codec uses named fields, and the VM
copies its strings before the host reuses the reply buffers.

`HostedApp.audio` stores the owner for the actual VM instance. `AppContextCall` dispatches audio only in an identified app's active widget
frame, validates its session and the widget asset policy, and reads
only that runtime's packed resources. `AppContextRun` / `AppContextRunFrame`
service streams; audio commands disable quiet-frame replay so effects cannot
repeat as cached UI. `HostedAppClose` and successful `HostedAppRestart`
release all voices. Failed restart preserves the previous instance's audio.
Before changing a context's account/resource ownership, the host
must call `AppContextAudioReset(app)`; it revokes in-flight decoding too.
Never use the outer Session alone as the resource owner.

A custom hosted widget host can supply its own instance's `MediaAudioOwner`
and packed reader as the final optional arguments to
`WidgetHostBegin(session, allowed, resolve, audio, audio_read, audio_context)`.
The original arguments remain sufficient. Without an owner, audio returns
`Unsupported`. `WidgetHostEnd` clears dispatch only; music survives frames.
The custom host must call `MediaAudioOwnerClose(owner)` on actual disposal,
account/resource context changes and instance close. Keep the owner storage
alive until an in-flight synchronous host call returns; a browser decode may
yield. A revoked decode discards its temporary resource before returning.

Native app hosts can use `MediaAudioBegin(owner, session, allowed, read,
context)` / `MediaAudioEnd()` around each callback, and
`MediaAudioOwnerTick(owner)` once per host frame while voices continue.
Binding a changed session or resource context closes previous voices;
close explicitly before replacing a reader or changing its policy in place.
closing an invalid session is also detected by the next tick. Owner close
is explicit and idempotent. Closing one owner never closes the shared device
or another owner's voices. Hosts can register a different actual provider
with `MediaAudioUseProvider`; current owners keep their provider until
closed. The caller retains cue selection, score/synthesis, volume UI, mute,
phrase scheduling and any single-in-flight music transition policy.

`kryon/media_audio_desktop`'s `MediaAudioUseDesktop()` installs the UI
provider over `std/audio` and `std/audio_sink`. The same provider works on
Android: the selected sink uses AAudio from API 26 and OpenSL ES for the
minSdk-21 scaffold. Device opening is asynchronous; voices and PCM blocks can
be queued during opening. A confirmed device failure returns `Unavailable`.
Kryon owns the UI commands and resource lifetime; audio works without a UI:

- `std/audio` mixes voices into 48 kHz float32 stereo. It provides decoded
  clips, streaming music, signed PCM16 blocks, gain, pitch, looping,
  pause/resume, position, length, seeking and a mixed-output tap.
- `std/audio_sink` selects SDL, Android or WebAudio. `AudioSinkOpen()` starts
  opening; `AudioSinkPending()`, `AudioSinkReady()` and
  `AudioSinkUnavailable()` report progress. `AudioSinkClose()` explicitly
  joins device teardown. SDL's disk driver supports deterministic tests.
- Decoders register by import: `std/audio_decode_wav` (PCM16, float32 and
  IMA/MS ADPCM), `std/audio_decode_mp3` and `std/audio_decode_vorbis`.
  MP3 uses the core `ziran_mp3` archive; Vorbis links the core recipe's
  `vorbisfile`, `vorbis` and `ogg` archives. `audio/decode_opus` optionally
  registers Ogg Opus decoding and links libopus. A custom decoder registers
  `AudioUseDecoder(probe, open)` with an `AudioSource`.

`make desktop-audio-test` exercises the mixer and UI provider through SDL's
disk driver. `make android-audio-test ANDROID_SERIAL=ZE2223BQZT` exercises
the same UI PCM provider, source and saved IR, on the USB Moto.
