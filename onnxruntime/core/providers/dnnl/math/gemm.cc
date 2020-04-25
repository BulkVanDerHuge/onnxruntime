// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/shared_library/provider_author.h"
#include "gemm.h"
#include "dnnl.h"
#include "dnnl.hpp"
#include "core/providers/dnnl/dnnl_fwd.h"
#include "gsl/gsl"
#include "Eigen/Core"

namespace onnxruntime {
namespace ort_dnnl {

ONNX_OPERATOR_KERNEL_EX(
    Gemm,
    kOnnxDomain,
    7,
    kDnnlExecutionProvider,
    Prov_KernelDefBuilder::Create()->TypeConstraint("T", DataTypeImpl::GetTensorType<float>()),
    Gemm<float>);

class GemmHelper {
 public:
  GemmHelper(const TensorShape& left, bool trans_left, const TensorShape& right, bool trans_right, const TensorShape& bias) {
    PROVIDER_NOT_IMPLEMENTED
    ORT_UNUSED_PARAMETER(left);
    ORT_UNUSED_PARAMETER(trans_left);
    ORT_UNUSED_PARAMETER(right);
    ORT_UNUSED_PARAMETER(trans_right);
    ORT_UNUSED_PARAMETER(bias);
  }

  int64_t M() const {
    PROVIDER_NOT_IMPLEMENTED
    return 0;
  }
  int64_t N() const {
    PROVIDER_NOT_IMPLEMENTED
    return 0;
  }
  int64_t K() const {
    PROVIDER_NOT_IMPLEMENTED
    return 0;
  }
  Status State() const {
    PROVIDER_NOT_IMPLEMENTED
    return Status::OK();
  }
};

template <typename T>
using EigenMatrixMapRowMajor = Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
template <typename T>
using ConstEigenVectorMap = Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, 1>>;
template <typename T>
using ConstEigenMatrixMapRowMajor = Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;

template <>
Status Gemm<float>::Compute(OpKernelContext* ctx) const {
  const auto X = ctx->Input<Tensor>(0);
  const auto W = ctx->Input<Tensor>(1);
  const auto B = ctx->Input<Tensor>(2);
  GemmHelper helper(X->Shape(), trans_A_, W->Shape(), trans_B_, B->Shape());

  if (!helper.State().IsOK())
    return helper.State();

  dnnl::memory::dim M = gsl::narrow_cast<int>(helper.M());
  dnnl::memory::dim N = gsl::narrow_cast<int>(helper.N());
  dnnl::memory::dim K = gsl::narrow_cast<int>(helper.K());
  auto Y = ctx->Output(0, TensorShape({M, N}));

  if (M <= 0)
    return ORT_MAKE_STATUS(ONNXRUNTIME, NOT_IMPLEMENTED, "Empty Tensor not supported");

  if (beta_ != 0) {
    auto output_mat = EigenMatrixMapRowMajor<float>(
        Y->template MutableData<float>(),
        M,
        N);
    output_mat.setZero();

    auto& b_shape = B->Shape();
    // if B is (), (1,) or (1, 1), add the scalar
    if (b_shape.Size() == 1) {
      output_mat.array() += *(B->template Data<float>());
    }
    // B is (N,)
    else if (b_shape.NumDimensions() == 1) {
      auto bias_vec = ConstEigenVectorMap<float>(
          B->template Data<float>(),
          N);
      output_mat.rowwise() += bias_vec.transpose();
    } else if (b_shape.NumDimensions() == 2) {
      // B is (M, 1)
      if (b_shape[1] == 1) {
        auto bias_vec = ConstEigenVectorMap<float>(
            B->template Data<float>(),
            M);
        output_mat.colwise() += bias_vec;
      }
      // B is (1, N)
      else if (b_shape[0] == 1) {
        auto bias_vec = ConstEigenVectorMap<float>(
            B->template Data<float>(),
            N);
        output_mat.rowwise() += bias_vec.transpose();
      }
      // B is (M, N), no broadcast needed.
      else {
        auto bias_mat = ConstEigenMatrixMapRowMajor<float>(
            B->template Data<float>(),
            M,
            N);
        output_mat += bias_mat;
      }
    }
  }

  // dnnl_sgemm expects row major matrices, so no need to swap the operands A and B
  auto status = dnnl_sgemm(trans_A_ ? 'T' : 'N',
                           trans_B_ ? 'T' : 'N',
                           M, N, K,
                           alpha_, X->template Data<float>(), trans_A_ ? M : K,
                           W->template Data<float>(), trans_B_ ? K : N,
                           beta_, Y->template MutableData<float>(), N);
  if (status == dnnl_success) {
    return Status::OK();
  } else {
    return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "DNNL_sgemm failed with status: ", status);
  }
}

}  // namespace ort_dnnl
}  // namespace onnxruntime
