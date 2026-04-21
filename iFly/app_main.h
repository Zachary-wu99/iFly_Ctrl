/**
 * @file app_main.h
 * @brief 应用层主入口声明。
 */
#ifndef IFLY_APP_MAIN_H
#define IFLY_APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 进入 C++ 应用层主流程。
 *
 * @details
 * 该接口由 `main.c` 调用，用于把 CubeMX 生成的 C 启动框架与
 * `iFly` 目录下的 C++ 应用逻辑连接起来。
 */
void app_main(void);

#ifdef __cplusplus
}
#endif

#endif /* IFLY_APP_MAIN_H */
