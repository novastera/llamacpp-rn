// Stub file to replace SYCL includes
// This is used when SYCL is disabled but the code tries to include it

#pragma once

#define GGML_USE_SYCL 0
#define GGML_USE_VULKAN 0
#define GGML_USE_CUDA 0
#define GGML_USE_HIP 0
#define GGML_USE_OPENGL 0
#define GGML_USE_OPENCL 0
#define GGML_USE_CANN 0

// Create a mock for sycl/sycl.hpp
namespace sycl {
    // Empty namespace to satisfy includes
}

// Create a mock for sycl/half_type.hpp
namespace sycl {
    struct half {
        // Empty struct
    };
}

// This is used to mock the sycl headers that are being included
// but shouldn't be used because SYCL is disabled
#define _SYCL_HPP_
#define _SYCL_HALF_TYPE_HPP_

// Empty namespace for sycl - this prevents compiler errors when the code tries to use sycl::
namespace sycl {
    // Empty namespace
}

// This file is included instead of <sycl/sycl.hpp>
// It serves as a stub to allow compilation without SYCL support