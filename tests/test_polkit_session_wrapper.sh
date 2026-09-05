#!/usr/bin/env bash
set -euo pipefail

source_dir="${1:?source directory required}"
test_root="$(mktemp -d)"
trap 'rm -rf -- "${test_root}"' EXIT
fake_bin="${test_root}/bin"
runtime="${test_root}/run/user/1000"
proc_root="${test_root}/proc"
cgroup_root="${test_root}/cgroup"
metadata="${test_root}/metadata"
log_file="${test_root}/calls"
mkdir -p "${fake_bin}" "${runtime}" "${proc_root}/101" "${proc_root}/202" \
  "${cgroup_root}/user.slice/session-a.scope" "${cgroup_root}/user.slice/session-b.scope" "${metadata}"

make_fake() {
  local name="$1" body="$2"
  printf '#!/usr/bin/env bash\nset -euo pipefail\n%s\n' "${body}" >"${fake_bin}/${name}"
  chmod +x "${fake_bin}/${name}"
}

make_fake loginctl '
operation="$1"
property=""
while (( $# > 0 )); do
  [[ "$1" == -p ]] && { property="$2"; break; }
  shift
done
if [[ "${property}" == Scope && "${FAKE_DELAY_SCOPE:-0}" -gt 0 ]]; then
  count="$(cat "${FAKE_SCOPE_COUNT}" 2>/dev/null || printf 0)"
  count=$((count + 1))
  printf "%s\n" "${count}" >"${FAKE_SCOPE_COUNT}"
  (( count <= FAKE_DELAY_SCOPE )) && exit 0
fi
# RuntimePath belongs to the logind User, never to Session.
if [[ "${property}" == RuntimePath && "${operation}" != show-user ]]; then
  exit 0
fi
cat "${FAKE_META_DIR}/${property}" 2>/dev/null || true'
make_fake systemctl '
if [[ "$*" == "show session-a.scope -p ControlGroup --value" ]]; then
  printf "/user.slice/session-a.scope\n"
elif [[ "$*" == "show session-b.scope -p ControlGroup --value" ]]; then
  printf "/user.slice/session-b.scope\n"
elif [[ "$*" == "--user show-environment" ]]; then
  printf "%s" "${FAKE_SYSTEMD_ENV-}"
fi'
make_fake stat '
if [[ "$*" == *"${HOLONIGHT_POLKIT_RUNTIME_DIR}" && -n "${FAKE_RUNTIME_OWNER:-}" ]]; then
  printf "%s\n" "${FAKE_RUNTIME_OWNER}"
else
  printf "%s\n" "${HOLONIGHT_POLKIT_CURRENT_UID}"
fi'
make_fake socket-check '[[ -e "$1.usable" ]]'
make_fake sleep 'printf "sleep\n" >>"${TEST_LOG}"'
make_fake peer-environment 'cat "${FAKE_PEER_ENVIRONMENT:-/dev/null}"; exit "${FAKE_PEER_STATUS:-0}"'
make_fake holonight-polkit-agent 'printf "agent session=%s runtime=%s wayland=%s display=%s qt=%s\n" "${XDG_SESSION_ID-}" "${XDG_RUNTIME_DIR-}" "${WAYLAND_DISPLAY-}" "${DISPLAY-}" "${QT_QPA_PLATFORM-}" >>"${TEST_LOG}"'

set_metadata() {
  printf '%s\n' "${1:-1000}" >"${metadata}/User"
  printf '%s\n' "${2:-wayland}" >"${metadata}/Type"
  printf '%s\n' "${3:-yes}" >"${metadata}/Active"
  printf '%s\n' "${4:-active}" >"${metadata}/State"
  printf '%s\n' "${runtime}" >"${metadata}/RuntimePath"
  printf '%s\n' "${5:-session-a.scope}" >"${metadata}/Scope"
}

write_environment() {
  local pid="$1" session="$2" wayland="$3"
  printf 'XDG_SESSION_ID=%s\0XDG_RUNTIME_DIR=%s\0WAYLAND_DISPLAY=%s\0DISPLAY=:7\0QT_QPA_PLATFORM=wayland\0IGNORED_SECRET=never-export\0' \
    "${session}" "${runtime}" "${wayland}" >"${proc_root}/${pid}/environ"
}

run_wrapper() {
  env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" FAKE_META_DIR="${metadata}" \
    FAKE_SCOPE_COUNT="${test_root}/scope-count" HOLONIGHT_POLKIT_LOGINCTL="${fake_bin}/loginctl" \
    HOLONIGHT_POLKIT_SYSTEMCTL="${fake_bin}/systemctl" HOLONIGHT_POLKIT_SLEEP="${fake_bin}/sleep" \
    HOLONIGHT_POLKIT_STAT="${fake_bin}/stat" HOLONIGHT_POLKIT_SOCKET_CHECK="${fake_bin}/socket-check" \
    HOLONIGHT_POLKIT_PROC_ROOT="${proc_root}" HOLONIGHT_POLKIT_CGROUP_ROOT="${cgroup_root}" \
    HOLONIGHT_POLKIT_RUNTIME_DIR="${runtime}" HOLONIGHT_POLKIT_CURRENT_UID=1000 \
    HOLONIGHT_POLKIT_WAYLAND_ENVIRONMENT="${fake_bin}/peer-environment" "$@" \
    bash "${fake_bin}/holonight-polkit-agent-session" session-a
}

cp "${source_dir}/scripts/holonight-polkit-agent-session" "${fake_bin}/holonight-polkit-agent-session"

set_metadata
write_environment 101 session-a wayland-a
write_environment 202 session-b wayland-b
printf '101\n' >"${cgroup_root}/user.slice/session-a.scope/cgroup.procs"
printf '202\n' >"${cgroup_root}/user.slice/session-b.scope/cgroup.procs"
: >"${runtime}/wayland-a.usable"
: >"${runtime}/wayland-b.usable"

: >"${log_file}"
run_wrapper HOLONIGHT_POLKIT_SESSION_TIMEOUT=0
grep -Fqx "agent session=session-a runtime=${runtime} wayland=wayland-a display=:7 qt=wayland" "${log_file}"

: >"${log_file}"
rm -f "${test_root}/scope-count"
run_wrapper HOLONIGHT_POLKIT_SESSION_TIMEOUT=1 FAKE_DELAY_SCOPE=1
grep -Fq sleep "${log_file}"
grep -Fq 'agent session=session-a' "${log_file}"

assert_rejected() {
  local expected="$1"
  shift
  if run_wrapper HOLONIGHT_POLKIT_SESSION_TIMEOUT=0 "$@" >"${test_root}/stdout" 2>"${test_root}/stderr"; then
    printf 'wrapper unexpectedly accepted invalid routing metadata\n' >&2
    exit 1
  fi
  grep -Fq "classification=${expected}" "${test_root}/stderr"
}

for field_value in 'User 1001' 'Type x11' 'Active no' 'State closing'; do
  read -r field value <<<"${field_value}"
  set_metadata
  printf '%s\n' "${value}" >"${metadata}/${field}"
  assert_rejected session-not-ready
done

set_metadata
assert_rejected session-not-ready FAKE_RUNTIME_OWNER=1001
rm -f "${runtime}/wayland-a.usable"
assert_rejected session-not-ready
: >"${runtime}/wayland-a.usable"
assert_rejected conflicting-global-environment $'FAKE_SYSTEMD_ENV=XDG_SESSION_ID=session-b\nWAYLAND_DISPLAY=wayland-b\n'

set_metadata 1000 wayland yes active session-b.scope
assert_rejected session-not-ready

# UWSM's compositor lives outside the logind session scope and publishes its
# Wayland display only after startup. The verified peer helper supplies it.
set_metadata
: >"${cgroup_root}/user.slice/session-a.scope/cgroup.procs"
peer_environment="${test_root}/peer-environment"
printf 'XDG_SESSION_ID=session-a\0XDG_RUNTIME_DIR=%s\0WAYLAND_DISPLAY=wayland-a\0QT_QPA_PLATFORM=wayland\0' \
  "${runtime}" >"${peer_environment}"
: >"${log_file}"
run_wrapper HOLONIGHT_POLKIT_SESSION_TIMEOUT=0 FAKE_PEER_ENVIRONMENT="${peer_environment}" \
  DISPLAY=:99 $'FAKE_SYSTEMD_ENV=WAYLAND_DISPLAY=wayland-a\nDISPLAY=:99\n'
grep -Fqx "agent session=session-a runtime=${runtime} wayland=wayland-a display= qt=wayland" "${log_file}"
assert_rejected conflicting-global-environment FAKE_PEER_ENVIRONMENT="${peer_environment}" \
  $'FAKE_SYSTEMD_ENV=XDG_SESSION_ID=session-b\nWAYLAND_DISPLAY=wayland-b\n'

assert_rejected session-not-ready FAKE_PEER_ENVIRONMENT="${peer_environment}" FAKE_PEER_STATUS=1

# Refuse global display disagreement even when the session ID is correct.
assert_rejected conflicting-global-environment FAKE_PEER_ENVIRONMENT="${peer_environment}" \
  $'FAKE_SYSTEMD_ENV=XDG_SESSION_ID=session-a\nWAYLAND_DISPLAY=wayland-b\n'

printf 'polkit session wrapper tests passed\n'
