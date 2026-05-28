#include <cuda_runtime.h>
#include <stdio.h>

__global__ void burn(float *out) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    float x = i * 0.000001f;

    for (int k = 0; k < 100000; k++) {
        x = sinf(x) * cosf(x) + sqrtf(fabsf(x) + 1.0f);
    }

    out[i] = x;
}

int main() {
    const int N = 1 << 24;
    float *d_out;
    cudaMalloc(&d_out, N * sizeof(float));

    while (true) {
        burn<<<N / 256, 256>>>(d_out);
        cudaDeviceSynchronize();
    }

    cudaFree(d_out);
    return 0;
}
