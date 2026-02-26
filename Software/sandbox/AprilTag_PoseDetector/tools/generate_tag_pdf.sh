#!/usr/bin/env bash
set -euo pipefail

# Repo root is parent of tools/
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${TOOLS_DIR}/.." && pwd)"

APRILTAG_IMGS_DIR="${TOOLS_DIR}/_apriltag-imgs"
GENERATED_DIR="${REPO_ROOT}/generated_pdfs"
PY_SCRIPT="${TOOLS_DIR}/make_tag_pdf.py"

APRILTAG_IMGS_REPO="https://github.com/AprilRobotics/apriltag-imgs.git"
DEFAULT_FAMILY="tag36h11"
TAG_SIZE_MM="100.0"   # exactly 10cm

# ---- Activate repo virtualenv automatically (Option 1 fix) ----
VENV_DIR="${REPO_ROOT}/.venv"
if [[ -f "${VENV_DIR}/bin/activate" ]]; then
  echo "== Activating virtual environment: ${VENV_DIR}"
  # shellcheck disable=SC1090
  source "${VENV_DIR}/bin/activate"
else
  echo "WARNING: ${VENV_DIR} not found. Using system python."
  echo "Tip: run ./setup.sh once to create the venv and install deps."
fi
# --------------------------------------------------------------

mkdir -p "${GENERATED_DIR}"

# Clone or update apriltag-imgs
if [[ ! -d "${APRILTAG_IMGS_DIR}/.git" ]]; then
  echo "== Cloning apriltag-imgs into ${APRILTAG_IMGS_DIR}"
  git clone "${APRILTAG_IMGS_REPO}" "${APRILTAG_IMGS_DIR}"
else
  echo "== Updating apriltag-imgs in ${APRILTAG_IMGS_DIR}"
  git -C "${APRILTAG_IMGS_DIR}" pull --ff-only
fi

echo ""
echo "Available families in apriltag-imgs:"
ls -1 "${APRILTAG_IMGS_DIR}" | grep -E '^tag' || true
echo ""

read -r -p "Tag family [${DEFAULT_FAMILY}]: " FAMILY
FAMILY="${FAMILY:-$DEFAULT_FAMILY}"

FAMILY_DIR="${APRILTAG_IMGS_DIR}/${FAMILY}"
if [[ ! -d "${FAMILY_DIR}" ]]; then
  echo "ERROR: family directory not found: ${FAMILY_DIR}"
  exit 1
fi

read -r -p "Tag ID (integer, e.g. 0, 1, 42): " TAG_ID
if [[ -z "${TAG_ID}" || ! "${TAG_ID}" =~ ^[0-9]+$ ]]; then
  echo "ERROR: Tag ID must be a non-negative integer."
  exit 1
fi

PAD5="$(printf "%05d" "${TAG_ID}")"
PNG_CANDIDATE=""

if [[ "${FAMILY}" == "tag36h11" ]]; then
  C="${FAMILY_DIR}/tag36_11_${PAD5}.png"
  if [[ -f "${C}" ]]; then
    PNG_CANDIDATE="${C}"
  fi
fi

if [[ -z "${PNG_CANDIDATE}" ]]; then
  PNG_CANDIDATE="$(find "${FAMILY_DIR}" -maxdepth 1 -type f -name "*.png" \
    \( -name "*_${PAD5}.png" -o -name "*${PAD5}.png" \) | head -n 1 || true)"
fi

if [[ -z "${PNG_CANDIDATE}" || ! -f "${PNG_CANDIDATE}" ]]; then
  echo "ERROR: Could not locate PNG for family=${FAMILY}, id=${TAG_ID}"
  echo "Searched in: ${FAMILY_DIR}"
  echo "Tip: list files with: ls -1 ${FAMILY_DIR} | head"
  exit 1
fi

OUT_PDF="${GENERATED_DIR}/${FAMILY}_id${PAD5}_10cm.pdf"

echo ""
echo "== Converting to PDF"
echo "Input PNG : ${PNG_CANDIDATE}"
echo "Output PDF: ${OUT_PDF}"
echo "Size      : ${TAG_SIZE_MM} mm (10 cm)"
echo ""

# IMPORTANT: use `python` (venv-aware) not `python3`
python "${PY_SCRIPT}" --png "${PNG_CANDIDATE}" --out "${OUT_PDF}" --size-mm "${TAG_SIZE_MM}" --dpi 600

echo ""
echo "DONE."
echo "Open the PDF in Preview and print with:"
echo "  - Scale: 100% (Actual Size)"
echo "  - Disable: 'Scale to Fit'"
echo ""
echo "Generated: ${OUT_PDF}"

