/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once
#include "paddle/framework/ddim.h"
#include "paddle/memory/memcpy.h"
#include "paddle/platform/device_context.h"

namespace paddle {
namespace operators {
namespace detail {

template <typename T, int Rank>
struct TensorCopyFunctor;

template <typename T>
struct TensorCopyFunctor<T, 1> {
  void operator()(const platform::DeviceContext& dev_ctx, const T* src,
                  framework::Dim<1> src_stride, framework::Dim<1> dst_dim,
                  framework::Dim<1> dst_stride, T* dst) const {
    auto place = dev_ctx.GetPlace();
    if (platform::is_cpu_place(place)) {
      auto& cpu_place = boost::get<platform::CPUPlace>(place);
      memory::Copy(cpu_place, dst, cpu_place, src, sizeof(T) * dst_dim.head);
    } else {
#ifndef PADDLE_ONLY_CPU
      auto& gpu_place = boost::get<platform::GPUPlace>(place);
      auto& cuda_ctx =
          reinterpret_cast<const platform::CUDADeviceContext&>(dev_ctx);
      memory::Copy(gpu_place, dst, gpu_place, src, sizeof(T) * dst_dim.head,
                   cuda_ctx.stream());
#else
      PADDLE_THROW("Paddle is not compiled with GPU");
#endif
    }
  }
};

template <typename T, int Rank>
struct TensorCopyFunctor {
  void operator()(const platform::DeviceContext& dev_ctx, const T* src,
                  framework::Dim<Rank> src_stride, framework::Dim<Rank> dst_dim,
                  framework::Dim<Rank> dst_stride, T* dst) const {
    for (int64_t i = 0; i < dst_dim.head; ++i) {
      TensorCopyFunctor<T, Rank - 1> func;
      func(dev_ctx, src, src_stride.tail, dst_dim.tail, dst_stride.tail, dst);
      src += src_stride.head;
      dst += dst_stride.head;
    }
  }
};

template <typename T>
struct TensorCopyDimVisitor : public boost::static_visitor<void> {
  TensorCopyDimVisitor(const platform::DeviceContext& dev_ctx, const T* src,
                       const framework::DDim& src_stride,
                       const framework::DDim& dst_stride, T* dst)
      : dev_ctx_(dev_ctx),
        src_(src),
        src_stride_(src_stride),
        dst_stride_(dst_stride),
        dst_(dst) {}

  template <typename Dim>
  void operator()(Dim dst_dim) const {
    Dim src_stride = boost::get<Dim>(src_stride_);
    Dim dst_stride = boost::get<Dim>(dst_stride_);
    constexpr int dim = Dim::dimensions;
    TensorCopyFunctor<T, dim> functor;
    functor(dev_ctx_, src_, src_stride, dst_dim, dst_stride, dst_);
  }

  const platform::DeviceContext& dev_ctx_;
  const T* src_;
  const framework::DDim& src_stride_;
  const framework::DDim& dst_stride_;
  T* dst_;
};

}  // namespace detail
}  // namespace operators
}  // namespace paddle
