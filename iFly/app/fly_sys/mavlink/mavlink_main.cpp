/**
 * @file mavlink_main.cpp
 * @brief MAVLink 主入口。
 */

#include "mavlink_main.hpp"

#include "cli.hpp"
#include "mavlink_console.hpp"
#include "mavlink_link.hpp"
#include "mavlink_receiver.hpp"
#include "mavlink_stream.hpp"

namespace iFly {

namespace {

CliService mavlink_cli;
MavlinkConsole mavlink_console;
MavlinkReceiver mavlink_receiver;
MavlinkStream mavlink_stream;

} // namespace

bool MavlinkMainInit(MavlinkLink *link)
{
  if (link == nullptr) {
    return false;
  }

  mavlink_cli.Init();
  mavlink_cli.Console().DisableActivationKey();
  mavlink_console.BindLink(link);
  mavlink_console.BindCli(&mavlink_cli);
  mavlink_receiver.BindLink(link);
  mavlink_stream.BindLink(link);
  mavlink_stream.ResetStreams();
  return true;
}

void MavlinkMain(uint32_t now_ms)
{
  (void)mavlink_receiver.Poll();
  mavlink_stream.Update(now_ms);
}

bool MavlinkMainHandleConsole(const mavlink_message_t &msg)
{
  return mavlink_console.ProcessConsoleMessage(msg);
}

} // namespace iFly
