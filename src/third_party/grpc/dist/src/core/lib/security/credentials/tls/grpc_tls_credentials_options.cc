//
//
// Copyright 2018 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//

#include <grpc/support/port_platform.h>

#include "src/core/lib/security/credentials/tls/grpc_tls_credentials_options.h"

#include <memory>

#include <grpc/grpc_crl_provider.h>
#include <grpc/support/log.h>

#include "src/core/lib/debug/trace.h"
#include "src/core/lib/gprpp/debug_location.h"
#include "src/core/lib/iomgr/exec_ctx.h"
#include "src/core/lib/surface/api_trace.h"
#include "src/core/tsi/ssl_transport_security.h"

/// -- Wrapper APIs declared in grpc_security.h -- *

grpc_tls_credentials_options* grpc_tls_credentials_options_create() {
  grpc_core::ExecCtx exec_ctx;
  return new grpc_tls_credentials_options();
}

void grpc_tls_credentials_options_set_cert_request_type(
    grpc_tls_credentials_options* options,
    grpc_ssl_client_certificate_request_type type) {
  GPR_ASSERT(options != nullptr);
  options->set_cert_request_type(type);
}

void grpc_tls_credentials_options_set_verify_server_cert(
    grpc_tls_credentials_options* options, int verify_server_cert) {
  GPR_ASSERT(options != nullptr);
  options->set_verify_server_cert(verify_server_cert);
}

void grpc_tls_credentials_options_set_certificate_provider(
    grpc_tls_credentials_options* options,
    grpc_tls_certificate_provider* provider) {
  GPR_ASSERT(options != nullptr);
  GPR_ASSERT(provider != nullptr);
  grpc_core::ExecCtx exec_ctx;
  options->set_certificate_provider(
      provider->Ref(DEBUG_LOCATION, "set_certificate_provider"));
}

void grpc_tls_credentials_options_watch_root_certs(
    grpc_tls_credentials_options* options) {
  GPR_ASSERT(options != nullptr);
  options->set_watch_root_cert(true);
}

void grpc_tls_credentials_options_set_root_cert_name(
    grpc_tls_credentials_options* options, const char* root_cert_name) {
  GPR_ASSERT(options != nullptr);
  options->set_root_cert_name(root_cert_name);
}

void grpc_tls_credentials_options_watch_identity_key_cert_pairs(
    grpc_tls_credentials_options* options) {
  GPR_ASSERT(options != nullptr);
  options->set_watch_identity_pair(true);
}

void grpc_tls_credentials_options_set_identity_cert_name(
    grpc_tls_credentials_options* options, const char* identity_cert_name) {
  GPR_ASSERT(options != nullptr);
  options->set_identity_cert_name(identity_cert_name);
}

void grpc_tls_credentials_options_set_certificate_verifier(
    grpc_tls_credentials_options* options,
    grpc_tls_certificate_verifier* verifier) {
  GPR_ASSERT(options != nullptr);
  GPR_ASSERT(verifier != nullptr);
  options->set_certificate_verifier(verifier->Ref());
}

void grpc_tls_credentials_options_set_crl_directory(
    grpc_tls_credentials_options* options, const char* crl_directory) {
  GPR_ASSERT(options != nullptr);
  options->set_crl_directory(crl_directory);
}

void grpc_tls_credentials_options_set_check_call_host(
    grpc_tls_credentials_options* options, int check_call_host) {
  GPR_ASSERT(options != nullptr);
  options->set_check_call_host(check_call_host);
}

void grpc_tls_credentials_options_set_tls_session_key_log_file_path(
    grpc_tls_credentials_options* options, const char* path) {
  if (!tsi_tls_session_key_logging_supported() || options == nullptr) {
    return;
  }
  GRPC_API_TRACE(
      "grpc_tls_credentials_options_set_tls_session_key_log_config(options=%p)",
      1, (options));
  // Tls session key logging is assumed to be enabled if the specified log
  // file is non-empty.
  if (path != nullptr) {
    gpr_log(GPR_INFO,
            "Enabling TLS session key logging with keys stored at: %s", path);
  } else {
    gpr_log(GPR_INFO, "Disabling TLS session key logging");
  }
  options->set_tls_session_key_log_file_path(path != nullptr ? path : "");
}

void grpc_tls_credentials_options_set_send_client_ca_list(
    grpc_tls_credentials_options* options, bool send_client_ca_list) {
  if (options == nullptr) {
    return;
  }
  options->set_send_client_ca_list(send_client_ca_list);
}

void grpc_tls_credentials_options_set_crl_provider(
    grpc_tls_credentials_options* options,
    std::shared_ptr<grpc_core::experimental::CrlProvider> provider) {
  GPR_ASSERT(options != nullptr);
  options->set_crl_provider(std::move(provider));
}

void grpc_tls_credentials_options_set_min_tls_version(
    grpc_tls_credentials_options* options, grpc_tls_version min_tls_version) {
  GPR_ASSERT(options != nullptr);
  options->set_min_tls_version(min_tls_version);
}

void grpc_tls_credentials_options_set_max_tls_version(
    grpc_tls_credentials_options* options, grpc_tls_version max_tls_version) {
  GPR_ASSERT(options != nullptr);
  options->set_max_tls_version(max_tls_version);
}
