#!/bin/bash


occa translate -D ROOTISBASH -m opencl ./include/RadixALL.okl \
> ./cross_gpgpu/OpenCL/kernel/radixALL.cl

{
    echo "#define _USE_MATH_DEFINES";
    occa translate -D __NEED_PI -D ROOTISBASH -m cuda ./include/RadixALL.okl
}> ./cross_gpgpu/CUDA/kernel/radixALL.cu

{
    echo "#define _USE_MATH_DEFINES";
    occa translate -D __NEED_PI -D ROOTISBASH -m openmp ./include/RadixALL.okl 
}> ./cross_gpgpu/OpenMP/kernel/compiled.hpp

{
    echo "#define _USE_MATH_DEFINES";
    occa translate -D __NEED_PI -D ROOTISBASH -m serial ./include/RadixALL.okl 
}> ./cross_gpgpu/Serial/kernel/compiled.hpp

{
    echo "#define _USE_MATH_DEFINES";
    occa translate -D __NEED_PI -D ROOTISBASH -m hip ./include/RadixALL.okl 
}> ./cross_gpgpu/HIP/kernel/compiled.hpp

{
    echo "#define _USE_MATH_DEFINES";
    occa translate -D __NEED_PI -D ROOTISBASH -m metal ./include/RadixALL.okl 
}> ./cross_gpgpu/METAL/kernel/compiled.hpp

python CL_Embedder.py
