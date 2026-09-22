#include <dynamips/dynamips.h>

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <unistd.h>

import dynamips;

int main() {
  dyn_vm_status status = DYN_VM_RUNNING;
  if (dyn_vm_get_status(nullptr, &status) != DYN_ERR_INVALID_ARGUMENT ||
      status != DYN_VM_HALTED)
    return 1;
  dyn_nio *nio = reinterpret_cast<dyn_nio *>(1);
  if (dyn_nio_create_udp_auto("test", "127.0.0.1", 0, 0, &nio, nullptr) !=
          DYN_ERR_INVALID_ARGUMENT ||
      nio != nullptr)
    return 1;
  if (dynamips::message(dynamips::error::internal).empty())
    return 1;

  char directory[] = "/tmp/dynamips-module-XXXXXX";
  if (mkdtemp(directory) == nullptr)
    return 1;
  const auto original_directory = std::filesystem::current_path();
  if (chdir(directory) != 0)
    return 1;

  const auto result = [] {
    auto runtime = dynamips::runtime::init_embedded();
    if (!runtime)
      return 1;
    auto router = dynamips::vm::create("module-router", 42, "c7200");
    auto endpoint = dynamips::nio::create_udp("module-nio", 0, "127.0.0.1", 9);
    auto network = dynamips::ethernet_switch::create("module-switch");
    if (!router || !endpoint || !network)
      return 1;

    runtime = std::unexpected(dynamips::error::internal);
    if (!router->status() || !endpoint->stats() || !network->add(*endpoint))
      return 1;

    router = std::unexpected(dynamips::error::internal);
    network = std::unexpected(dynamips::error::internal);
    endpoint = std::unexpected(dynamips::error::internal);
    dyn_nio *late = nullptr;
    return dyn_nio_create_udp("late", 0, "127.0.0.1", 9, &late) ==
                       DYN_ERR_NOT_INITIALIZED &&
                   late == nullptr
               ? 0
               : 1;
  }();

  std::filesystem::current_path(original_directory);
  std::filesystem::remove_all(directory);
  return result;
}
