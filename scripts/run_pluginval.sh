#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
PLUGIN="${1:-${PROJECT_DIR}/Build-Git2/EonMiniEEF_artefacts/Release/VST3/EEF-JP8000.vst3}"
PLUGINVAL="${PLUGINVAL_BIN:-pluginval}"
STRICTNESS="${PLUGINVAL_STRICTNESS:-5}"

if [[ ! -d "${PLUGIN}" ]]; then
  echo "plugin bundle not found: ${PLUGIN}" >&2
  echo "Build the Release VST3 first or pass its path as the first argument." >&2
  exit 2
fi

if [[ "${PLUGINVAL}" == */* ]]; then
  if [[ ! -x "${PLUGINVAL}" ]]; then
    echo "pluginval is not executable: ${PLUGINVAL}" >&2
    exit 2
  fi
else
  if ! command -v "${PLUGINVAL}" >/dev/null 2>&1; then
    echo "pluginval not found; set PLUGINVAL_BIN or install pluginval" >&2
    exit 2
  fi
fi

if ! [[ "${STRICTNESS}" =~ ^[0-5]$ ]]; then
  echo "PLUGINVAL_STRICTNESS must be an integer from 0 to 5 (got ${STRICTNESS})" >&2
  exit 2
fi

exec "${PLUGINVAL}" --strictness-level "${STRICTNESS}" --validate "${PLUGIN}"
