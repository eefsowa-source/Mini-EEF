#!/usr/bin/env bash
set -euo pipefail

# Run the checks that do not need a DAW.  This script is safe to invoke from
# any working directory and is suitable for local development and CI.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_DIR}/Build-Git2}"

python3 "${PROJECT_DIR}/scripts/run_dsp_sanity.py"

if [[ -f "${BUILD_DIR}/CTestTestfile.cmake" ]]; then
  ctest --test-dir "${BUILD_DIR}" -C Release --output-on-failure
else
  echo "quality: CTest skipped (configure first: cmake -B \"${BUILD_DIR}\")" >&2
fi

if [[ "${RUN_PLUGINVAL:-0}" == "1" ]]; then
  "${PROJECT_DIR}/scripts/run_pluginval.sh" "${2:-${BUILD_DIR}/EonMiniEEF_artefacts/Release/VST3/EEF-JP8000.vst3}"
else
  echo "quality: pluginval skipped (set RUN_PLUGINVAL=1 to enable)"
fi

echo "quality: PASS"
