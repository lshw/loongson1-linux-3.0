#ifndef _LS1X_TIME_H
#define _LS1X_TIME_H

#ifdef CONFIG_LS1X_TIMER
extern void setup_ls1x_timer(void);
extern void disable_ls1x_counter(void);
extern void enable_ls1x_counter(void);
#else
static inline void __maybe_unused setup_ls1x_timer(void)
{
}
static inline void __maybe_unused disable_ls1x_counter(void)
{
}
static inline void __maybe_unused enable_ls1x_counter(void)
{
}
#endif

#endif /*!_LS1X_TIME_H */
