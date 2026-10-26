# Grouped playback volume

AudioService owns playbackApplications for its entire lifetime. The raw playbackStreams
and all provider controls remain available. Recording is unchanged.

AudioApplicationModel groups trimmed identifiers by application ID, then process binary,
then application name. Key prefixes distinguish identity types. Streams without any
identity get individual stream keys. Groups retain first-appearance order until their
final member disappears. Rows are inserted, removed and updated without model resets,
preserving delegates and slider focus during membership changes.

Display the first nonempty application name and icon. Name fallback is process binary,
application ID, then “Unknown application.” Multiple members show “N streams”; a single
member shows its media title unless identical to its application name. Volume is the
highest reported member volume; muted means all members are muted.

Grouped commands clamp volume to 0–100 and fan out through provider per-stream controls.
Explicit volume and mute choices are remembered independently while the group exists.
Each new member inherits those choices once; repeated notifications and external changes
only update aggregates. Commands do not optimistically change display values. Vanished
groups ignore commands. Final removal or backend disconnection forgets choices; closing
the popup does not.

The popup uses grouped rows and commands with the existing slider, keyboard handling,
drag protection and empty state. The four-row cap and Show All count applications.
There is no per-stream expansion, browser-tab control or long-term persistence.

Acceptance: C++ identity, aggregate, fan-out, inheritance and lifecycle tests; QML Teams
four-stream plus mpv regression, repeated upserts, drag/delegate stability, subtitles,
keyboard use and expansion beyond four applications. Run QML lint and metadata checks.
Live acceptance requires mpv during a Teams call and browser playback: both application
rows remain visible and each grouped slider affects only its own application.
