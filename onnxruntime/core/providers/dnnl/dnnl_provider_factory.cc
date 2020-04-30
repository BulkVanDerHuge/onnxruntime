// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/shared_library/provider_author.h"
#include "core/providers/dnnl/dnnl_provider_factory.h"
#include <atomic>
#include "dnnl_execution_provider.h"

using namespace onnxruntime;

extern onnxruntime::ProviderHost* g_host;

namespace onnxruntime {

void SetProviderHost(ProviderHost& host);

struct DnnlProviderFactory : Prov_IExecutionProviderFactory {
  DnnlProviderFactory(bool create_arena) : create_arena_(create_arena) {}
  ~DnnlProviderFactory() override {}

  std::unique_ptr<Prov_IExecutionProvider> CreateProvider() override;

 private:
  bool create_arena_;
};

std::unique_ptr<Prov_IExecutionProvider> DnnlProviderFactory::CreateProvider() {
  DNNLExecutionProviderInfo info;
  info.create_arena = create_arena_;
  return onnxruntime::make_unique<DNNLExecutionProvider>(info);
}

std::shared_ptr<Prov_IExecutionProviderFactory> CreateExecutionProviderFactory_Dnnl(int device_id) {
  return std::make_shared<onnxruntime::DnnlProviderFactory>(device_id);
  //TODO: This is apparently a bug. The consructor parameter is create-arena-flag, not the device-id
}

struct Dnnl_Provider : Provider {
  std::shared_ptr<Prov_IExecutionProviderFactory> CreateExecutionProviderFactory(int device_id) override {
    return std::make_shared<DnnlProviderFactory>(device_id);
  }

  void SetProviderHost(ProviderHost& host) {
    onnxruntime::SetProviderHost(host);
  }
} g_provider;

}  // namespace onnxruntime

extern "C" {

ORT_API(onnxruntime::Provider*, GetProvider) {
  return &onnxruntime::g_provider;
}
}
