#include <gflags/gflags.h>
#include <string>

DEFINE_int32(port, 8000, "TCP Port of this server");

std::string GetUserRouter(std::string username) {
  return "127.0.0.1:" + std::to_string(FLAGS_port);
}
