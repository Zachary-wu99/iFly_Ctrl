#include "mavlink_cli.hpp"

#include "main.h"
#include "tick.hpp"

namespace iFly {

void MavlinkCliService::RegisterFunctions()
{
  (void)shell_.RegisterFunction(
      {"status", "print flight-controller status", &MavlinkCliService::StatusFunction,
       this});
  (void)shell_.RegisterFunction(
      {"sys.reboot", "trigger MCU software reset",
       &MavlinkCliService::RebootFunction, this});
}

bool MavlinkCliService::StatusFunction(Shell *shell, void *context, uint8_t argc,
                                       const char *const *argv)
{
  (void)argv;

  MavlinkCliService *cli = reinterpret_cast<MavlinkCliService *>(context);
  if ((shell == nullptr) || (cli == nullptr) || (argc != 0U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call status");
    }
    return false;
  }

  shell->WriteLine("Flight Controller Status");
  shell->Printf("  transport     : %s\r\n",
                (cli->active_transport_name_ != nullptr) ? cli->active_transport_name_
                                                         : "unbound");
  shell->Printf("  shell_link    : %s\r\n",
                cli->shell_.IsConnected() ? "connected" : "disconnected");
  shell->Printf("  uptime_ms     : %lu\r\n",
                static_cast<unsigned long>(tick::NowMs()));
  return true;
}

bool MavlinkCliService::RebootFunction(Shell *shell, void *context, uint8_t argc,
                                       const char *const *argv)
{
  (void)context;
  (void)argv;

  if ((shell == nullptr) || (argc != 0U)) {
    if (shell != nullptr) {
      shell->WriteLine("Usage: call sys.reboot");
    }
    return false;
  }

  shell->WriteLine("System reboot requested.");
  NVIC_SystemReset();
  return true;
}

} // namespace iFly
