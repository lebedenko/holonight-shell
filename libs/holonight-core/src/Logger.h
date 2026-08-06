#pragma once

#include <QString>

namespace holonight::logger {

// Initialize the logging system. Sets up a custom thread-safe message handler,
// configures log levels for the console and file, routes logs to a file in
// the standard user data directory, and implements automatic log rotation.
void initialize(int argc, char** argv);

// Shuts down the logging system, closes log files, and restores default handlers.
void shutdown();

// Returns the path of the current log file.
QString logFilePath();

}  // namespace holonight::logger
