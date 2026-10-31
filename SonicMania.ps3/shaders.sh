#!/bin/bash
# ==============================================================================
# Script to compile all Cg shaders into .vpo and .fpo files for PS3 RSX/GCM
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHADERS_DIR="${SCRIPT_DIR}/../dependencies/RSDKv5/RSDKv5/shaders"

# Find Cg compiler
CGCOMP_BIN=""
if command -v cgcomp >/dev/null 2>&1; then
    CGCOMP_BIN="cgcomp"
elif command -v ppu-cgcomp >/dev/null 2>&1; then
    CGCOMP_BIN="ppu-cgcomp"
elif [ -n "$CELL_SDK" ] && [ -x "$CELL_SDK/host-linux/bin/cgcomp" ]; then
    CGCOMP_BIN="$CELL_SDK/host-linux/bin/cgcomp"
elif [ -n "$PSL1GHT" ] && [ -x "$PSL1GHT/bin/cgcomp" ]; then
    CGCOMP_BIN="$PSL1GHT/bin/cgcomp"
fi

if [ -z "$CGCOMP_BIN" ]; then
    echo "Error: cgcomp / ppu-cgcomp not found in PATH." >&2
    echo "Please ensure the PS3 toolchain bin directory is added to your PATH." >&2
    exit 1
fi

if [ ! -d "$SHADERS_DIR" ]; then
    echo "Error: Shaders directory not found at: $SHADERS_DIR" >&2
    exit 1
fi

echo "Compiling Cg shaders in: $SHADERS_DIR"
echo "Using compiler: $CGCOMP_BIN"

count=0

for shader in "$SHADERS_DIR"/*.cg; do
    [ -e "$shader" ] || continue
    filename="$(basename "$shader")"

    # Determine whether shader is vertex or fragment
    is_vertex=0
    is_fragment=0

    # 1. Check file naming conventions
    case "$filename" in
        *.v.cg|*_v.cg|*.vertex.cg|*.vp.cg|*_vs.cg|*.vs.cg)
            is_vertex=1
            ;;
        *.f.cg|*_f.cg|*.fragment.cg|*.fp.cg|*_fs.cg|*.fs.cg)
            is_fragment=1
            ;;
    esac

    # 2. If not determined by name, inspect content keywords
    if [ $is_vertex -eq 0 ] && [ $is_fragment -eq 0 ]; then
        if grep -qi "POSITION" "$shader" && ! grep -qi "sampler2D" "$shader"; then
            is_vertex=1
        elif grep -qi "sampler2D\|tex2D" "$shader"; then
            is_fragment=1
        elif grep -qi "VS_OUTPUT\|VS_INPUT" "$shader"; then
            is_vertex=1
        elif grep -qi "FS_INPUT\|COLOR0" "$shader"; then
            is_fragment=1
        fi
    fi

    # Compile based on detected type
    base_name="${filename%.*}"
    base_name="${base_name%.v}"
    base_name="${base_name%.f}"
    base_name="${base_name%_v}"
    base_name="${base_name%_f}"

    if [ $is_vertex -eq 1 ]; then
        out_file="${SHADERS_DIR}/${base_name}.vpo"
        echo "  [VERTEX]   $filename -> $(basename "$out_file")"
        "$CGCOMP_BIN" -v "$shader" "$out_file"
        count=$((count + 1))
    elif [ $is_fragment -eq 1 ]; then
        out_file="${SHADERS_DIR}/${base_name}.fpo"
        echo "  [FRAGMENT] $filename -> $(basename "$out_file")"
        "$CGCOMP_BIN" -f "$shader" "$out_file"
        count=$((count + 1))
    else
        echo "  [WARNING]  Could not determine shader type for: $filename (skipping)" >&2
    fi
done

echo "Successfully processed $count shader(s)."
