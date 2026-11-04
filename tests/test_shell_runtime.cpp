#include "ShellRuntime.h"

#include <gtest/gtest.h>
TEST(ShellRuntime, CapturesExactStartupValue) {
  const auto saved = qgetenv("HOLONIGHT_TROUBLESHOOT");
  const bool was_set = qEnvironmentVariableIsSet("HOLONIGHT_TROUBLESHOOT");
  for (const auto& value : {QByteArray(""), QByteArray("false"), QByteArray("1"), QByteArray("TRUE"),
                            QByteArray("True"), QByteArray(" true"), QByteArray("true "), QByteArray("true")}) {
    qputenv("HOLONIGHT_TROUBLESHOOT", value);
    ShellRuntime runtime;
    EXPECT_EQ(runtime.troubleshootEnabled(), value == "true");
    qunsetenv("HOLONIGHT_TROUBLESHOOT");
    EXPECT_EQ(runtime.troubleshootEnabled(), value == "true");
  }
  ShellRuntime unset;
  EXPECT_FALSE(unset.troubleshootEnabled());
  if (was_set)
    qputenv("HOLONIGHT_TROUBLESHOOT", saved);
  else
    qunsetenv("HOLONIGHT_TROUBLESHOOT");
}
