# Phase 30 — PulseAudio Channel-Count Remediation: Specification

**Status**: Complete — automated checks and user verification passed.

## Scope

Remediate Phase 7 candidate U-04 I-04: volume requests must preserve the
target PulseAudio object's actual channel count instead of assuming stereo.

## Requirements

- REQ-F-01: Sink, source, and playback-stream volume updates obtain the
  current PulseAudio object information before constructing `pa_cvolume`.
- REQ-F-02: The resulting volume vector uses the reported channel count, with
  a safe one-channel minimum for malformed zero-channel input.
- REQ-NF-01: Existing public `PulseAudioBackend` and `AudioService` volume
  APIs remain unchanged.

## Out of scope

- Changing QML audio controls or exposing channel counts in UI models.
- Altering mute, default-device, or stream-routing behavior.
