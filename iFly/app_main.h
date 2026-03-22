#ifndef IFLY_APP_MAIN_H
#define IFLY_APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 应用层主任务入口。
 *
 * @details
 * - 该函数由 `main.c` 在主循环中周期调用。
 * - 函数内部自行完成一次性初始化和后续循环处理。
 * - 这样可以把上层应用逻辑放在 C++ 文件中实现，同时保留 CubeMX 生成的 `main.c`
 *   作为整个工程的唯一启动入口。
 */
void app_main(void);

#ifdef __cplusplus
}
#endif

#endif /* IFLY_APP_MAIN_H */
