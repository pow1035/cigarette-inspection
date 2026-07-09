cmake_minimum_required(VERSION 3.18)
project(YOLO26_TensorRT_v2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TensorRT path
set(TENSORRT_ROOT "D:/深度学习软件/TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8/TensorRT-8.6.1.6" CACHE PATH "TensorRT root")

# CUDA path
set(CUDA_TOOLKIT_ROOT_DIR "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8" CACHE PATH "CUDA toolkit root")

# OpenCV from standalone installation
set(OpenCV_DIR "E:/code/opencv/build/x64/vc16/lib" CACHE PATH "OpenCV directory")

find_package(OpenCV REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${TENSORRT_ROOT}/include
    ${CUDA_TOOLKIT_ROOT_DIR}/include
)

# Link directories - Use TensorRT bin since we copied CUDA libs there
link_directories(
    ${TENSORRT_ROOT}/lib
    ${TENSORRT_ROOT}/bin
    ${CUDA_TOOLKIT_ROOT_DIR}/lib/x64
    E:/code/opencv/build/x64/vc16/lib
)

# Library
add_library(yolo_trt_v2 STATIC
    yolo_trt_v2.cpp
    yolo_trt_v2.h
)

target_link_libraries(yolo_trt_v2
    ${OpenCV_LIBS}
    nvinfer
    nvonnxparser
    cudart
)

# Executable
add_executable(test_yolo_trt_v2
    test_yolo_trt_v2.cpp
)

target_link_libraries(test_yolo_trt_v2
    yolo_trt_v2
    ${OpenCV_LIBS}
    nvinfer
    nvonnxparser
    cudart
)

# New executable for yanzhi20260120 model test
add_executable(test_yanzhi20260120
    test_yanzhi20260120.cpp
)

target_link_libraries(test_yanzhi20260120
    yolo_trt_v2
    ${OpenCV_LIBS}
    nvinfer
    nvonnxparser
    cudart
)

# MSVC settings
if(MSVC)
    set_property(TARGET yolo_trt_v2 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set_property(TARGET test_yolo_trt_v2 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set_property(TARGET test_yanzhi20260120 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Copy TensorRT DLLs to output directory
    add_custom_command(TARGET test_yolo_trt_v2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TENSORRT_ROOT}/lib/nvinfer.dll"
        "${TENSORRT_ROOT}/lib/nvinfer_plugin.dll"
        "${TENSORRT_ROOT}/lib/nvonnxparser.dll"
        "${TENSORRT_ROOT}/lib/nvinfer_dispatch.dll"
        "${TENSORRT_ROOT}/lib/nvinfer_lean.dll"
        $<TARGET_FILE_DIR:test_yolo_trt_v2>
    )

    # Copy CUDA DLLs from bin directory
    add_custom_command(TARGET test_yolo_trt_v2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TENSORRT_ROOT}/bin/cudart64_110.dll"
        "${TENSORRT_ROOT}/bin/cublas64_11.dll"
        "${TENSORRT_ROOT}/bin/cublasLt64_11.dll"
        $<TARGET_FILE_DIR:test_yolo_trt_v2>
    )

    # Copy OpenCV DLLs from conda
    file(GLOB OPENCV_DLLS "${OpenCV_DIR}/bin/opencv_world*.dll")
    if(OPENCV_DLLS)
        add_custom_command(TARGET test_yolo_trt_v2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENCV_DLLS}
            $<TARGET_FILE_DIR:test_yolo_trt_v2>
        )
    endif()

    # Copy DLLs for test_yanzhi20260120
    add_custom_command(TARGET test_yanzhi20260120 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TENSORRT_ROOT}/lib/nvinfer.dll"
        "${TENSORRT_ROOT}/lib/nvinfer_plugin.dll"
        "${TENSORRT_ROOT}/lib/nvonnxparser.dll"
        "${TENSORRT_ROOT}/lib/nvinfer_dispatch.dll"
        "${TENSORRT_ROOT}/lib/nvinfer_lean.dll"
        $<TARGET_FILE_DIR:test_yanzhi20260120>
    )

    add_custom_command(TARGET test_yanzhi20260120 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TENSORRT_ROOT}/bin/cudart64_110.dll"
        "${TENSORRT_ROOT}/bin/cublas64_11.dll"
        "${TENSORRT_ROOT}/bin/cublasLt64_11.dll"
        $<TARGET_FILE_DIR:test_yanzhi20260120>
    )

    if(OPENCV_DLLS)
        add_custom_command(TARGET test_yanzhi20260120 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENCV_DLLS}
            $<TARGET_FILE_DIR:test_yanzhi20260120>
        )
    endif()
endif()

message(STATUS "========================================")
message(STATUS "YOLO26 TensorRT v2 Configuration")
message(STATUS "========================================")
message(STATUS "TensorRT:  ${TENSORRT_ROOT}")
message(STATUS "OpenCV:    ${OpenCV_DIR}")
message(STATUS "OpenCV Libs: ${OpenCV_LIBS}")
message(STATUS "========================================")
