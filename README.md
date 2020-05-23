### Idempotence-based GPU Preemptive Scheduling

This preemptive GPU scheduler for ARM Mali-T628 GPU is an OS-level solution to the priority inversion problem. Exploiting the shared physical memory between the CPU and GPU in heterogeneous system architecture (HSA), we propose two approaches for GPU preemptive scheduling.

First, if GPU kernel is idempotent, preemptive scheduling is available without additional process in schduling because the consistency of initial input data is guaranteed while executing kernel function. We provide a static analysis tool for recognizing the clobber anti-dependency pattern which break idempotent property of kernel function. We also provide a set of functions that tells the driver whether the kernel is idempotent or not. 

Second, if GPu kernel is not idempotent, scheduler needs to a way of ensuring the consistency of initial input data. Therefore we provide the feature to transactionize the GPU kernels.By snapshotting the kernel GPU memory in advance, A transactionized GPU kernel can be aborted at any point during its execution and rolled back to its initial state for re-execution. Based on this transactionizing GPU kernels, it is possible to evict low-priority kernels and immediately schedule high-priority kernels forcibly. The preempted low-priority kernel instances can be re-executed after a GPU becomes available.

Our preemptive GPU scheduling concept is implemented in the device driver of a Mali T-628 MP6 GPU based on a Samsung Exynos 5422 system on chip (SoC). To apply our implementation to the Exynos 5422 system, you must install a specific kernel version 3.10.72 because our code is currently supporting that specific version. You can download the kernel 3.10.72 for Exynos from Hardkernel Github repository (https://github.com/hardkernel/linux). In order to apply our schemes to the Linux kernel, patch the downloaded original Mali driver with our Mali GPU device driver patch file.

##
