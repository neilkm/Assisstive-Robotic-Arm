#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_NAME="$(basename "$SCRIPT_DIR")"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/builds/$TOOL_NAME"

CUDA_BURN_SRC="$SCRIPT_DIR/cuda_burn.cu"
CUDA_MEM_BURN_SRC="$SCRIPT_DIR/cuda_mem_burn.cu"
CUDA_BURN_BIN="$BUILD_DIR/cuda_burn"
CUDA_MEM_BURN_BIN="$BUILD_DIR/cuda_mem_burn"

NVCC_BIN="${NVCC:-}"
if [[ -z "$NVCC_BIN" ]]; then
  if command -v nvcc >/dev/null 2>&1; then
    NVCC_BIN="$(command -v nvcc)"
  elif [[ -x /usr/local/cuda/bin/nvcc ]]; then
    NVCC_BIN="/usr/local/cuda/bin/nvcc"
  fi
fi

#Test Duration
# default 30s
DURATION="${1:-30}" 

#Logfile location
LOGFILE="$BUILD_DIR/tegrastats_burn.log"

mkdir -p "$BUILD_DIR"

build_cuda_helper() {
  local source_file="$1"
  local output_file="$2"

  if [[ ! -x "$output_file" || "$source_file" -nt "$output_file" ]]; then
    echo "[jetson-burn] building $(basename "$output_file")..."
    if [[ -z "$NVCC_BIN" ]]; then
      echo "[jetson-burn] error: nvcc not found. Install CUDA or set NVCC=/path/to/nvcc." >&2
      exit 1
    fi
    "$NVCC_BIN" "$source_file" -o "$output_file" || exit 1
  fi
}

build_cuda_helper "$CUDA_BURN_SRC" "$CUDA_BURN_BIN"
build_cuda_helper "$CUDA_MEM_BURN_SRC" "$CUDA_MEM_BURN_BIN"

echo "Jetson Nano Power Draw Test"
echo "maxing cpu, gpu, memory, swap usage for ${DURATION}s"
echo "ignore errors and wait till Power summary stats at end of test"
echo "logs stored in: ${LOGFILE}"
echo "====starting test===="

pkill -f cuda_burn 2>/dev/null || true
pkill -f cuda_mem_burn 2>/dev/null || true
pkill stress-ng 2>/dev/null || true
pkill tegrastats 2>/dev/null || true

#Linux swap aggression
# 0 avoid swap, 60 default, 100 max pressure
sudo sysctl vm.swappiness=100 

#Jetson power mode
# 0 MAXN, 1 reduced power, 2 MAXN_SUPER
sudo nvpmodel -m 2 
sudo jetson_clocks

echo "[jetson-burn] starting tegrastats..."

#Stats polling interval
# check power draw every 100ms
tegrastats --interval 100 > "$LOGFILE" & 
TEGRAPID=$!

echo "[jetson-burn] starting workloads..."

# GPU compute burn
timeout "${DURATION}s" "$CUDA_BURN_BIN" &
timeout "${DURATION}s" "$CUDA_BURN_BIN" &
timeout "${DURATION}s" "$CUDA_BURN_BIN" &

# GPU memory burn
timeout "${DURATION}s" "$CUDA_MEM_BURN_BIN" &
timeout "${DURATION}s" "$CUDA_MEM_BURN_BIN" &
timeout "${DURATION}s" "$CUDA_MEM_BURN_BIN" &

# CPU + RAM + swap pressure
stress-ng \
  --cpu 0 \
  --cpu-method fft \
  --vm 3 \
  --vm-bytes 40% \
  --vm-keep \
  --timeout "${DURATION}s"

echo "[jetson-burn] cleaning up..."

pkill -f cuda_burn 2>/dev/null || true
pkill -f cuda_mem_burn 2>/dev/null || true
pkill stress-ng 2>/dev/null || true

kill "$TEGRAPID" 2>/dev/null || true
sleep 2

echo
echo "========== POWER SUMMARY =========="

awk '
{
    for(i=1;i<=NF;i++) {
        if($i=="VDD_IN") {
            split($(i+1),a,"mW/")
            vin=a[1]
            vin_sum+=vin
            if(vin>vin_max) vin_max=vin
            vin_count++
        }

        if($i=="VDD_CPU_GPU_CV") {
            split($(i+1),a,"mW/")
            vcpu=a[1]
            vcpu_sum+=vcpu
            if(vcpu>vcpu_max) vcpu_max=vcpu
            vcpu_count++
        }

        if($i=="VDD_SOC") {
            split($(i+1),a,"mW/")
            vsoc=a[1]
            vsoc_sum+=vsoc
            if(vsoc>vsoc_max) vsoc_max=vsoc
            vsoc_count++
        }
    }
}
END {
    printf("VDD_IN:\n")
    printf("  Max: %.2f W\n", vin_max/1000)
    printf("  Avg: %.2f W\n\n", (vin_sum/vin_count)/1000)

    printf("VDD_CPU_GPU_CV:\n")
    printf("  Max: %.2f W\n", vcpu_max/1000)
    printf("  Avg: %.2f W\n\n", (vcpu_sum/vcpu_count)/1000)

    printf("VDD_SOC:\n")
    printf("  Max: %.2f W\n", vsoc_max/1000)
    printf("  Avg: %.2f W\n", (vsoc_sum/vsoc_count)/1000)
}
' "$LOGFILE"

echo "==================================="
