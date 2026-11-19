#!/usr/bin/env bash
set -euo pipefail

# LazyLook Model Setup Helper
# Downloads and verifies local GGUF models for zero-setup AI semantic search.

DEFAULT_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/lazylook/models"
TARGET_DIR="${DEFAULT_DIR}"

TEXT_MODEL="embeddinggemma-2-Q8_0.gguf"
TEXT_URL="https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/resolve/main/embeddinggemma-2-Q8_0.gguf"

VISION_MODEL="mmproj-embeddinggemma-2.gguf"
VISION_URL="https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/resolve/main/mmproj-embeddinggemma-2.gguf"

WHISPER_MODEL="ggml-tiny.bin"
WHISPER_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"

DO_TEXT=1
DO_VISION=0
DO_WHISPER=0
DO_CHECK_ONLY=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Download and manage companion AI neural models for LazyLook.

Options:
  --text         Download primary text embedding model (EmbeddingGemma-2 Q8_0, default)
  --vision       Download companion vision projector for image frame embeddings
  --whisper      Download companion Whisper speech-to-text model (ggml-tiny)
  --all          Download text, vision, and Whisper speech models
  --dir <path>   Custom destination directory (default: ${DEFAULT_DIR})
  --check        Check status and sizes of currently installed models
  --help         Show this help message
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --text)
            DO_TEXT=1
            shift
            ;;
        --vision)
            DO_VISION=1
            shift
            ;;
        --whisper)
            DO_WHISPER=1
            shift
            ;;
        --all)
            DO_TEXT=1
            DO_VISION=1
            DO_WHISPER=1
            shift
            ;;
        --dir)
            if [[ -n "${2:-}" ]]; then
                TARGET_DIR="$2"
                shift 2
            else
                echo "Error: --dir requires a directory path" >&2
                exit 1
            fi
            ;;
        --check)
            DO_CHECK_ONLY=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            ;;
    esac
done

check_status() {
    echo "=========================================="
    echo " LazyLook Models Status"
    echo " Directory: ${TARGET_DIR}"
    echo "=========================================="
    if [[ ! -d "${TARGET_DIR}" ]]; then
        echo "Directory does not exist yet."
        return
    fi

    for m in "${TEXT_MODEL}" "${VISION_MODEL}" "${WHISPER_MODEL}" "embeddinggemma-2-BF16.gguf"; do
        if [[ -f "${TARGET_DIR}/${m}" ]]; then
            local sz
            sz=$(du -h "${TARGET_DIR}/${m}" | cut -f1)
            echo "  [INSTALLED] ${m} (${sz})"
        else
            echo "  [MISSING]   ${m}"
        fi
    done
    echo "=========================================="
}

if [[ ${DO_CHECK_ONLY} -eq 1 ]]; then
    check_status
    exit 0
fi

# Determine download tool
DOWNLOADER=""
if command -v curl >/dev/null 2>&1; then
    DOWNLOADER="curl"
elif command -v wget >/dev/null 2>&1; then
    DOWNLOADER="wget"
else
    echo "Error: Neither 'curl' nor 'wget' is available on this system." >&2
    exit 1
fi

mkdir -p "${TARGET_DIR}"

download_file() {
    local name="$1"
    local url="$2"
    local dest="${TARGET_DIR}/${name}"
    local tmp_dest="${dest}.tmp"

    if [[ -f "${dest}" ]] && [[ -s "${dest}" ]]; then
        local sz
        sz=$(du -h "${dest}" | cut -f1)
        echo "==> ${name} is already downloaded (${sz}). Skipping."
        return
    fi

    echo "==> Downloading ${name}..."
    echo "    URL: ${url}"
    echo "    Destination: ${dest}"

    if [[ "${DOWNLOADER}" == "curl" ]]; then
        curl -L -C - --progress-bar -o "${tmp_dest}" "${url}"
    else
        wget -c --show-progress -O "${tmp_dest}" "${url}"
    fi

    mv "${tmp_dest}" "${dest}"
    echo "==> Successfully installed ${name} to ${dest}."
}

if [[ ${DO_TEXT} -eq 1 ]]; then
    download_file "${TEXT_MODEL}" "${TEXT_URL}"
fi

if [[ ${DO_VISION} -eq 1 ]]; then
    download_file "${VISION_MODEL}" "${VISION_URL}"
fi

if [[ ${DO_WHISPER} -eq 1 ]]; then
    download_file "${WHISPER_MODEL}" "${WHISPER_URL}"
fi

echo ""
check_status
echo "Done. LazyLook is ready for semantic search."
