# Test commands run with /home hidden. Do not reuse FindPython3's build-tool
# cache or search the user's PATH, virtualenv, or package-manager prefixes.
unset(HOLONIGHT_TEST_PYTHON)
unset(HOLONIGHT_TEST_PYTHON CACHE)
find_program(HOLONIGHT_TEST_PYTHON NAMES python3
  PATHS /usr/bin /bin NO_DEFAULT_PATH NO_CACHE REQUIRED)
