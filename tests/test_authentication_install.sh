#!/usr/bin/env bash
set -euo pipefail

source_dir="${1:?source directory required}"
build_dir="${2:?build directory required}"
stage="$(mktemp -d)"
trap 'rm -rf -- "${stage}"' EXIT

cmake --install "${build_dir}" --prefix "${stage}" --component Unspecified >/dev/null

require_executable() {
  [[ -x "$1" ]] || { printf 'missing installed executable: %s\n' "$1" >&2; exit 1; }
}

require_executable "${stage}/bin/holonight-session"
require_executable "${stage}/bin/holonight-sudo-askpass"
require_executable "${stage}/bin/holonight-ssh-askpass"
require_executable "${stage}/bin/holonight-askpass"
require_executable "${stage}/libexec/holonight-polkit-agent"
require_executable "${stage}/libexec/holonight-polkit-agent-session"
require_executable "${stage}/libexec/holonight-wayland-session-environment"

unit="${stage}/share/systemd/user/holonight-polkit-agent@.service"
[[ -f "${unit}" ]]
grep -Fqx "ExecStart=\"${stage}/libexec/holonight-polkit-agent-session\" %i" "${unit}" || { echo "installed unit points outside its prefix" >&2; exit 1; }
for directive in 'After=graphical-session.target' 'Restart=on-failure' 'RestartSec=2s' \
  'StartLimitIntervalSec=30s' 'StartLimitBurst=3' 'RestartPreventExitStatus=78' 'LimitCORE=0'; do
  grep -Fqx "${directive}" "${unit}" || { printf 'missing unit directive: %s\n' "${directive}" >&2; exit 1; }
done
if grep -Eq '(holonight-shell|^(Requires|PartOf|BindsTo|WantedBy)=)' "${unit}"; then
  printf 'Polkit unit is coupled to the shell or globally enabled\n' >&2
  exit 1
fi

session_launcher="${stage}/bin/holonight-session"
grep -Fqx "installation_datadir=\"${stage}/share\"" "${session_launcher}"
grep -Fq 'askpass_bindir="${HOLONIGHT_ASKPASS_BINDIR:-${script_directory}}"' "${session_launcher}"
for alias in holonight-sudo-askpass holonight-ssh-askpass holonight-askpass; do
  [[ -L "${stage}/bin/${alias}" ]]
  [[ "$(readlink -f "${stage}/bin/${alias}")" == "${stage}/libexec/holonight-askpass" ]]
done
for helper in holonight-session holonight-shell-systemd; do
  bash -n "${stage}/bin/${helper}"
done
bash -n "${stage}/libexec/holonight-polkit-agent-session"
! grep -Fq '@CMAKE_INSTALL_' "${session_launcher}" "${unit}"
grep -Fqx "ExecStart=\"${stage}/bin/holonight-shell-systemd\"" "${stage}/share/systemd/user/holonight-shell.service"
grep -Fq 'SUDO_ASKPASS="${askpass_bindir}/holonight-sudo-askpass"' "${session_launcher}"
grep -Fq 'SSH_ASKPASS="${askpass_bindir}/holonight-ssh-askpass"' "${session_launcher}"
! grep -Fq 'export SSH_ASKPASS_PROMPT' "${session_launcher}"

qml_dir="${stage}/lib/qt6/qml/Holonight/Authentication"
[[ -s "${qml_dir}/qmldir" ]]
qmltypes="$(find "${qml_dir}" -maxdepth 1 -type f -name '*.qmltypes' -print -quit)"
[[ -n "${qmltypes}" && -s "${qmltypes}" ]]
grep -Fq 'AuthenticationPromptModel' "${qmltypes}"

for executable in "${stage}/libexec/holonight-askpass" "${stage}/libexec/holonight-polkit-agent"; do
  if readelf -d "${executable}" | grep -E '(RPATH|RUNPATH).*(/tmp/|/build)' >/dev/null; then
    printf 'installed executable retains a development runtime path: %s\n' "${executable}" >&2
    exit 1
  fi
  if readelf -d "${executable}" | grep -E 'NEEDED.*(holonight-shell|holonight_services|holonight_surfaces|holonight_compositor)' >/dev/null; then
    printf 'authentication executable has a forbidden runtime dependency: %s\n' "${executable}" >&2
    exit 1
  fi
done

if grep -RInE --include='CMakeLists.txt' --include='*.cmake' --include='*.sh' \
  '(/etc/sudo\.conf|systemctl[^#]*(enable|disable|mask|stop).*(polkit|policykit)|pkill[^#]*(polkit|policykit))' \
  "${source_dir}/CMakeLists.txt" "${source_dir}/apps" "${source_dir}/cmake" "${source_dir}/scripts" >/dev/null; then
  printf 'installation code contains prohibited agent or sudo-policy manipulation\n' >&2
  exit 1
fi

printf 'authentication install tests passed\n'
