# Shared System Services Adoption

Shell adopts `HoloNightSystem::Audio` from baseline `2e63d8edfbe1e332266997b41ff80765588d01eb`.
The local `AudioService` is only a QML registration wrapper around `HoloNight::System::AudioController`; backend,
controller, model, value-type, reconnect, and conversion behavior belongs to the provider. Existing QML names,
properties, invokables, signals, and model roles remain unchanged.
