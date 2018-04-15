#pragma once

#include "PIDX.h"

#define PIDX_CHECK(F) \
  { \
    PIDX_return_code rc = F; \
    if (rc != PIDX_success) { \
      const std::string er = "PIDX Error at " #F ": " + pidx_error_to_string(rc); \
      std::cerr << er << std::endl; \
      throw std::runtime_error(er); \
    } \
  }

inline std::string pidx_error_to_string(const PIDX_return_code rc) {
  if (rc == PIDX_success) return "PIDX_success";
  else if (rc == PIDX_err_id) return "PIDX_err_id";
  else if (rc == PIDX_err_unsupported_flags) return "PIDX_err_unsupported_flags";
  else if (rc == PIDX_err_file_exists) return "PIDX_err_file_exists";
  else if (rc == PIDX_err_name) return "PIDX_err_name";
  else if (rc == PIDX_err_box) return "PIDX_err_box";
  else if (rc == PIDX_err_file) return "PIDX_err_file";
  else if (rc == PIDX_err_time) return "PIDX_err_time";
  else if (rc == PIDX_err_block) return "PIDX_err_block";
  else if (rc == PIDX_err_comm) return "PIDX_err_comm";
  else if (rc == PIDX_err_count) return "PIDX_err_count";
  else if (rc == PIDX_err_size) return "PIDX_err_size";
  else if (rc == PIDX_err_offset) return "PIDX_err_offset";
  else if (rc == PIDX_err_type) return "PIDX_err_type";
  else if (rc == PIDX_err_variable) return "PIDX_err_variable";
  else if (rc == PIDX_err_not_implemented) return "PIDX_err_not_implemented";
  else if (rc == PIDX_err_point) return "PIDX_err_point";
  else if (rc == PIDX_err_access) return "PIDX_err_access";
  else if (rc == PIDX_err_mpi) return "PIDX_err_mpi";
  else if (rc == PIDX_err_rst) return "PIDX_err_rst";
  else if (rc == PIDX_err_chunk) return "PIDX_err_chunk";
  else if (rc == PIDX_err_compress) return "PIDX_err_compress";
  else if (rc == PIDX_err_hz) return "PIDX_err_hz";
  else if (rc == PIDX_err_agg) return "PIDX_err_agg";
  else if (rc == PIDX_err_io) return "PIDX_err_io";
  else if (rc == PIDX_err_unsupported_compression_type) return "PIDX_err_unsupported_compression_type";
  else if (rc == PIDX_err_close) return "PIDX_err_close";
  else if (rc == PIDX_err_flush) return "PIDX_err_flush";
  else if (rc == PIDX_err_header) return "PIDX_err_header";
  else if (rc == PIDX_err_wavelet) return "PIDX_err_wavelet";
  else return "Unknown PIDX Error";
}
