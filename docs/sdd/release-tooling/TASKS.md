# SDD Tasks — release-tooling

- [x] T-001: Create version.h.in template and CMake configure_file integration
  - REQs: REQ-F-003, REQ-C-001
  - Check: `grep -q 'HOLONIGHT_SHELL_VERSION "@PROJECT_VERSION@"' src/version.h.in && grep -q 'configure_file.*src/version.h.in' CMakeLists.txt && grep -q 'target_include_directories(holonight-shell PRIVATE.*CMAKE_CURRENT_BINARY_DIR' CMakeLists.txt`

- [x] T-002: Implement --version and -V flag in src/main.cpp
  - REQs: REQ-F-001, REQ-F-002
  - Check: `./build/holonight-shell --version 2>&1 | grep -q '^holonight-shell [0-9]\+\.[0-9]\+\.[0-9]\+$' && ./build/holonight-shell -V 2>&1 | grep -q '^holonight-shell [0-9]\+\.[0-9]\+\.[0-9]\+$' && test $? -eq 0`

- [x] T-003: Create CHANGELOG.md with Keep a Changelog format for 0.1.0
  - REQs: REQ-F-015
  - Check: `grep -q '## \[0.1.0\]' CHANGELOG.md && grep -q 'Initial release' CHANGELOG.md && grep -q 'Keep a Changelog' CHANGELOG.md || head -c 100 CHANGELOG.md | grep -q 'Keep'`

- [x] T-004: Write scripts/release.sh with validation, version bump, commit, and tag creation
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-C-002, REQ-NF-002, REQ-NF-004
  - Check: `test -x scripts/release.sh && bash scripts/release.sh 0.1.0 2>&1 | grep -q 'CMakeLists.txt updated' && git tag -l | grep -q '^v0.1.0$' && git reset --hard HEAD~1 && git tag -d v0.1.0`

- [x] T-005: Create .github/workflows/release.yml with build gate, tarball creation, and GitHub Release
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-C-003, REQ-C-004, REQ-C-005, REQ-NF-001
  - Check: `grep -q "tags: \[?'v\*" .github/workflows/release.yml && grep -q 'ctest.*--test-dir' .github/workflows/release.yml && grep -q 'git archive.*tar.gz' .github/workflows/release.yml && grep -q 'gh release create' .github/workflows/release.yml && grep -q 'permissions:' .github/workflows/release.yml`

- [x] T-006: Write RELEASING.md with step-by-step release instructions for maintainers
  - REQs: REQ-F-014, REQ-NF-002, REQ-NF-003
  - Check: `test -f RELEASING.md && wc -l < RELEASING.md | awk '{exit ($1 >= 30 ? 0 : 1)}' && grep -q 'CMakeLists.txt' RELEASING.md && grep -q 'scripts/release.sh' RELEASING.md && grep -q 'gh release' RELEASING.md`

- [x] T-007: Verify version single-source-of-truth and consistency across all artifacts
  - REQs: REQ-F-003, REQ-NF-003
  - Check: `v_cmake=$(grep '^project(holonight-shell VERSION' CMakeLists.txt | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+') && ! grep -r '"0\.1\.0"' src/ CMakeLists.txt 2>/dev/null | grep -v 'version.h.in' && test -n "$v_cmake"`

- [x] T-008: Integration test: dry-run full release workflow locally and verify git state
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-C-002, REQ-NF-004
  - Check: `cd /tmp && git clone /home/andrii/projects/holonight-shell test-release && cd test-release && bash scripts/release.sh 0.1.0 2>&1 | tee /tmp/release.log && git tag -l | grep -q '^v0.1.0$' && git show v0.1.0 | grep -q 'Release v0.1.0'`

- [x] T-009: Verify CI release workflow can extract version and create tarball in dry-run context
  - REQs: REQ-F-010, REQ-F-012, REQ-C-004, REQ-C-005
  - Check: `cd build && VERSION=$(echo 'v0.1.0' | sed 's/^v//') && git archive --format=tar.gz --prefix="holonight-shell-${VERSION}/" -o "test-holonight-shell-${VERSION}.tar.gz" HEAD && test -f "test-holonight-shell-${VERSION}.tar.gz" && tar -tzf "test-holonight-shell-${VERSION}.tar.gz" | head -1 | grep -q "holonight-shell-${VERSION}/"`
