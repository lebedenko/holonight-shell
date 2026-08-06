# Phase 30 — PulseAudio Channel-Count Remediation: Design

**Status**: Complete — automated checks and user verification passed.

## Approach

Each existing volume setter requests current PulseAudio metadata for its
target, then its completion callback constructs and submits a `pa_cvolume`
using that metadata's channel count. This avoids an independently synchronized
channel-count cache while keeping the public control API intact.

## Safety

The request context owns only the requested percentage and is released by the
terminal PulseAudio info callback, after any object response has used it.
Missing responses submit no volume update. A zero reported channel count is
normalized to one before calling `pa_cvolume_set`.

## Validation

Focused fake-PulseAudio tests will assert mono, surround, and multichannel
volume vectors, followed by the project test and formatting checks.
