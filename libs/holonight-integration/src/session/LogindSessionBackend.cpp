#include "LogindSessionBackend.h"

LogindSessionBackend::LogindSessionBackend(const ProcessEnvironment* env, CommandRunner* runner)
    : SessionBackend(env, runner) {}
