/**
 * @file mavlink_console.hpp
 * @brief MAVLink 控制台接口。
 */
#ifndef IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_CONSOLE_HPP
#define IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_CONSOLE_HPP

#include "cli.hpp"
#include "mavlink_link.hpp"

namespace iFly {

/**
 * @brief MAVLink 控制台服务。
 */
class MavlinkConsole final {
public:
  /**
   * @brief 构造 MAVLink 控制台服务对象。
   *
   * @param link MAVLink 字节流链路。
   * @param cli 飞控 CLI 对象。
   */
  explicit MavlinkConsole(MavlinkLink *link = nullptr,
                          CliService *cli = nullptr)
      : link_(link)
  {
    BindCli(cli);
  }

  /**
   * @brief 绑定 MAVLink 字节流链路。
   *
   * @param link MAVLink 字节流链路。
   */
  void BindLink(MavlinkLink *link)
  {
    link_ = link;
  }

  /**
   * @brief 绑定飞控 CLI 对象。
   *
   * @param cli 飞控 CLI 对象。
   */
  void BindCli(CliService *cli)
  {
    cli_ = cli;
    if (cli_ != nullptr) {
      cli_->SetOutput(&MavlinkConsole::ConsoleOutput, this);
    }
  }

  /**
   * @brief 处理 MAVLink 控制台消息。
   *
   * @param msg MAVLink 消息。
   * @return 控制台消息处理成功返回 `true`。
   */
  bool ProcessConsoleMessage(const mavlink_message_t &msg)
  {
    if ((link_ == nullptr) || (cli_ == nullptr)) {
      return false;
    }

    mavlink_serial_control_t control {};
    if (!link_->DecodeConsoleMessage(msg, &control)) {
      return false;
    }

    cli_->SetConnected(true);
    if (control.count > 0U) {
      cli_->ProcessInput(control.data, control.count);
    }

    return true;
  }

  /**
   * @brief 发送 MAVLink 控制台输出。
   *
   * @param data 控制台输出数据。
   * @param length 控制台输出字节数。
   * @return 实际发送字节数。
   */
  uint32_t SendConsoleOutput(const uint8_t *data, uint32_t length)
  {
    if (link_ == nullptr) {
      return 0U;
    }

    return link_->SendConsoleOutput(data, length);
  }

  /**
   * @brief 发送一帧 MAVLink 控制台回包。
   *
   * @param data 控制台输出数据。
   * @param len 控制台输出字节数。
   * @param flags SERIAL_CONTROL 标志位。
   */
  void SendConsoleReply(const uint8_t *data, uint8_t len, uint8_t flags)
  {
    if (link_ == nullptr) {
      return;
    }

    link_->SendConsoleReply(data, len, flags);
  }

private:
  /**
   * @brief MAVLink 控制台输出回调。
   *
   * @param context 回调上下文。
   * @param data 输出数据。
   * @param length 输出字节数。
   * @return 实际发送字节数。
   */
  static uint32_t ConsoleOutput(void *context,
                                const uint8_t *data,
                                uint32_t length)
  {
    MavlinkConsole *console = reinterpret_cast<MavlinkConsole *>(context);
    if (console == nullptr) {
      return 0U;
    }

    return console->SendConsoleOutput(data, length);
  }

  MavlinkLink *link_ = nullptr; /**< MAVLink 字节流链路。 */
  CliService *cli_ = nullptr; /**< 飞控 CLI 对象指针。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_GROUND_STATION_MAVLINK_CONSOLE_HPP */

