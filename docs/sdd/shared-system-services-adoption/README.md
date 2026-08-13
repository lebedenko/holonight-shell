# Shared System Services Adoption

Shell adopts `HoloNightSystem::Audio` from baseline `c328df03a308f14b6815e64ea160b3f2c749d9e5`.
The local `AudioService` is only a QML registration wrapper around `HoloNight::System::AudioController`; backend,
controller, model, value-type, reconnect, and conversion behavior belongs to the provider. Existing QML names,
properties, invokables, signals, and model roles remain unchanged.
