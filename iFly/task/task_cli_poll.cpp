/**
 * @file task_cli_poll.cpp
 * @brief CLI调度任务。
 */

#include "task.hpp"
#include "flight_ctrl_cli.hpp"

namespace {

struct CliPollContext final {
  iFly::FlightCtrlCli *cli = nullptr;
};

CliPollContext cli_poll_context {};
iFly::TaskHandle cli_poll_handle = iFly::kInvalidTaskHandle;

void CliPollTask(void *context)
{
  CliPollContext *ctx = static_cast<CliPollContext *>(context);
  if ((ctx == nullptr) || (ctx->cli == nullptr)) {
    return;
  }

  ctx->cli->Poll();
}

} // namespace

bool InitCliPollTask(iFly::FlightCtrlCli *cli)
{
  if (cli == nullptr) {
    return false;
  }

  cli_poll_context.cli = cli;

  cli_poll_handle = iFly::TaskCreatePeriodic(&CliPollTask,
                                               &cli_poll_context,
                                               200U,
                                               iFly::SoftTimerService::kLowestPriority,
                                               0U,
                                               "cli_poll");

  return cli_poll_handle != iFly::kInvalidTaskHandle;
}


