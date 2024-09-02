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


nvcc -ptx ./cross_gpgpu/CUDA/kernel/radixALL.cu -o ./cross_gpgpu/CUDA/kernel/radixALL.ptx

# python CL_Embedder.py
printf "#pragma once\nclass okl_embed {\n public:\n const char* ptx_code = \n R\"(" | cat - ./cross_gpgpu/CUDA/kernel/radixALL.ptx > ./cross_gpgpu/CUDA/kernel/temp.txt

{
    cat ./cross_gpgpu/CUDA/kernel/temp.txt - <<EOF
    )";};
EOF
}> ./cross_gpgpu/CUDA/kernel/okl_embed.hpp