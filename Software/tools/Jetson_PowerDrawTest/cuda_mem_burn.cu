#include <cuda_runtime.h>
#include <stdio.h>

__global__ void mem_burn(float *a, float *b, float *c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    for (int j = i; j < n; j += stride) {
        float x = a[j];
        float y = b[j];
        for (int k = 0; k < 32; k++) {
            x = x * 1.000001f + y;
            y = y * 0.999999f + x;
        }
        c[j] = x + y;
    }
}

int main() {
    const int N = 1 << 27; // ~512 MB per array
    float *a, *b, *c;
    cudaMalloc(&a, N * sizeof(float));
    cudaMalloc(&b, N * sizeof(float));
    cudaMalloc(&c, N * sizeof(float));

    cudaMemset(a, 1, N * sizeof(float));
    cudaMemset(b, 2, N * sizeof(float));

    while (true) {
        mem_burn<<<4096, 256>>>(a, b, c, N);
        cudaDeviceSynchronize();
    }
}
