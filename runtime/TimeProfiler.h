#ifndef TIME_PROFILER_H
#define TIME_PROFILER_H

#ifdef __cplusplus
extern "C" {
#endif

    // MT-3000设备端函数声明
    unsigned int get_thread_id(void);
    unsigned long get_clk(void);
    int hthread_printf(const char *format, ...);

    // 时间单位换算宏（时钟频率为4150MHz）
#define CLK_FREQ 4150000000UL
#define CYCLES_TO_NS(cycles) ((cycles) * 1000000000UL / CLK_FREQ)
#define CYCLES_TO_US(cycles) ((cycles) * 1000000UL / CLK_FREQ)
#define CYCLES_TO_MS(cycles) ((cycles) * 1000UL / CLK_FREQ)

#ifdef __cplusplus
}
#endif

#endif // TIME_PROFILER_H