#!/usr/bin/env bash
# Fetch GEMV inputs from two repositories and assemble data/gemv/.
set -euo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DEST="$PROJECT_DIR/data/gemv"
REPO1="${GEMV_REPO1:-https://github.com/ayushkumar7380/data1_cs_683project.git}"
REPO2="${GEMV_REPO2:-https://github.com/ayushkumar7380/data2_cs683_project.git}"

command -v git >/dev/null || { echo 'ERROR: Git is required. Install it with: sudo apt install git' >&2; exit 1; }
TMP="$(mktemp -d)"
trap 'rm -rf -- "$TMP"' EXIT
mkdir -p "$TMP/merged"

choose_source() {
  local repo="$1"
  if [[ -d "$repo/data/gemv" ]]; then printf '%s\n' "$repo/data/gemv"
  elif [[ -d "$repo/gemv" ]]; then printf '%s\n' "$repo/gemv"
  elif [[ -d "$repo/data" ]]; then printf '%s\n' "$repo/data"
  else printf '%s\n' "$repo"
  fi
}

files=0
for i in 1 2; do
  if [[ "$i" == 1 ]]; then url="$REPO1"; else url="$REPO2"; fi
  echo "Downloading dataset part $i from $url"
  # For public repositories, this does not require a GitHub account.
  git clone --quiet --depth 1 "$url" "$TMP/repo$i" || {
    echo "ERROR: Cannot clone dataset repository $i: $url" >&2
    echo 'Confirm the URL, repository visibility, and GitHub access.' >&2
    exit 1
  }
  src="$(choose_source "$TMP/repo$i")"
  echo "Found dataset directory: ${src#"$TMP/"}"
  while IFS= read -r -d '' file; do
    rel="${file#"$src/"}"
    # Ignore repo-level metadata and non-dataset documentation.
    case "${rel##*/}" in README|README.md|LICENSE|LICENSE.md|.gitignore|.gitattributes) continue;; esac
    case "$rel" in .git/*|*/.git/*) continue;; esac
    # Include NumPy inputs and corresponding generator/helper scripts.
    case "$rel" in *.npy|*.npz|*.py|*.csv|*.txt|*.bin) ;; *) continue;; esac
    target="$TMP/merged/$rel"
    mkdir -p "$(dirname "$target")"
    if [[ -e "$target" ]]; then
      if ! cmp -s -- "$file" "$target"; then
        echo "ERROR: Both repositories contain different data for '$rel'." >&2
        echo 'Resolve the duplicate filename before using this setup script.' >&2
        exit 1
      fi
    else
      cp -- "$file" "$target"
      ((files+=1))
    fi
  done < <(find "$src" -path '*/.git' -prune -o -type f -print0)
done

if (( files == 0 )) || ! find "$TMP/merged" -type f \( -name '*.npy' -o -name '*.npz' \) | grep -q .; then
  echo 'ERROR: No .npy/.npz inputs found. Check the dataset repository layout.' >&2
  exit 1
fi

# Check conflicts with any pre-existing local files before copying anything.
while IFS= read -r -d '' file; do
  rel="${file#"$TMP/merged/"}"
  if [[ -e "$DEST/$rel" ]] && ! cmp -s -- "$file" "$DEST/$rel"; then
    echo "ERROR: Local file conflicts with downloaded dataset: data/gemv/$rel" >&2
    echo 'Existing files were left untouched. Move the conflicting file and rerun.' >&2
    exit 1
  fi
done < <(find "$TMP/merged" -type f -print0)

mkdir -p "$DEST"
while IFS= read -r -d '' file; do
  rel="${file#"$TMP/merged/"}"
  mkdir -p "$(dirname "$DEST/$rel")"
  if [[ ! -e "$DEST/$rel" ]]; then cp -- "$file" "$DEST/$rel"; fi
done < <(find "$TMP/merged" -type f -print0)

echo "Dataset ready at: $DEST"
echo "Dataset files: $(find "$DEST" -type f | wc -l)"
echo "NumPy arrays: $(find "$DEST" -type f \( -name '*.npy' -o -name '*.npz' \) | wc -l)"
echo 'Next: scons && bash run_project.sh --full'
