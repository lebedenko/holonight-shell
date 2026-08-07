#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${BUILD_DIR:-build}}"
if [[ "${build_dir}" = /* ]]; then
  build_root="${build_dir}"
else
  build_root="${repo_root}/${build_dir}"
fi

shell_module_dir="${build_root}/apps/shell/HolonightShell"
qmltypes_file="${shell_module_dir}/holonight-shell.qmltypes"
qmldir_file="${shell_module_dir}/qmldir"
raw_qml_qrc="${build_root}/apps/shell/.qt/rcc/holonight-shell_raw_qml_0.qrc"
taskfile="${repo_root}/Taskfile.yml"

required_types=(
  "AppearanceService"
  "AudioService"
  "NotificationService"
  "WorkspaceModel"
)

if [[ ! -s "${qmltypes_file}" ]]; then
  echo "Missing or empty qmltypes file: ${qmltypes_file}" >&2
  echo "Build holonight-shell before running this check." >&2
  exit 1
fi

if ! grep -q 'Module {' "${qmltypes_file}"; then
  echo "Malformed qmltypes file: ${qmltypes_file}" >&2
  exit 1
fi

missing_types=()
for type_name in "${required_types[@]}"; do
  if ! grep -q "name: \"${type_name}\"" "${qmltypes_file}"; then
    missing_types+=("${type_name}")
  fi
done

if (( ${#missing_types[@]} > 0 )); then
  {
    echo "Generated qmltypes file is missing required HolonightShell types:"
    printf '  %s\n' "${missing_types[@]}"
    echo
    echo "Checked: ${qmltypes_file}"
  } >&2
  exit 1
fi

echo "QML type metadata check passed."

required_qml_entries=(
  "AppearanceReloadBridge 1.0 Utility/AppearanceReloadBridge.qml"
)

if [[ ! -s "${qmldir_file}" ]]; then
  echo "Missing or empty shell qmldir file: ${qmldir_file}" >&2
  echo "Build holonight-shell before running this check." >&2
  exit 1
fi

missing_qml_entries=()
for entry in "${required_qml_entries[@]}"; do
  if ! grep -Fqx "${entry}" "${qmldir_file}"; then
    missing_qml_entries+=("${entry}")
  fi
done

if (( ${#missing_qml_entries[@]} > 0 )); then
  {
    echo "Generated HolonightShell qmldir is missing required QML entries:"
    printf '  %s\n' "${missing_qml_entries[@]}"
    echo
    echo "Checked: ${qmldir_file}"
  } >&2
  exit 1
fi

if [[ ! -s "${raw_qml_qrc}" ]]; then
  echo "Missing or empty shell QML resource map: ${raw_qml_qrc}" >&2
  echo "Build holonight-shell before running this check." >&2
  exit 1
fi

if ! grep -Fq 'alias="Utility/AppearanceReloadBridge.qml"' "${raw_qml_qrc}"; then
  echo "Shell QML resource map does not bundle Utility/AppearanceReloadBridge.qml: ${raw_qml_qrc}" >&2
  exit 1
fi

if grep -Fq '/src/qml/' "${raw_qml_qrc}"; then
  echo "Shell QML resource map contains legacy src/qml paths: ${raw_qml_qrc}" >&2
  exit 1
fi

if ! grep -Fq 'QML_IMPORT_PATH: "{{.HOLONIGHT_QT_PREFIX}}/lib/qt6/qml"' "${taskfile}"; then
  echo "Development run task does not select the staged Holonight Qt QML module: ${taskfile}" >&2
  exit 1
fi

if grep -Rqs --include='*.qml' 'onFixedFontChanged' "${repo_root}/apps"; then
  echo "Shell QML still contains the removed fixed-font signal handler." >&2
  exit 1
fi

echo "QML module packaging check passed."
