/**
 * @file rc_main.hpp
 * @brief RC 主入口接口。
 */
#ifndef IFLY_APP_FLY_SYS_RC_RC_MAIN_HPP
#define IFLY_APP_FLY_SYS_RC_RC_MAIN_HPP

#include <stdint.h>

#include "rc.hpp"

namespace iFly {

/**
 * @brief 初始化 RC 主逻辑。
 *
 * @param receiver RC 接收服务对象。
 * @return 初始化成功返回 `true`。
 */
bool RcMainInit(Rc *receiver);

/**
 * @brief 执行一次 RC 通道处理。
 *
 * @param now_ms 当前时间戳，单位为 ms。
 */
void RcMain(uint32_t now_ms);

/**
 * @brief 获取最近一次处理后的 RC 输入。
 *
 * @return RC 输入只读引用。
 */
const RcInput &RcMainInput();

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_RC_RC_MAIN_HPP */
