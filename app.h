#ifndef __app_H__
#define __app_H__

/*
 * register 0: low or high: 0 := 18, 1 := 27
 * register 1: variable value:
 */
#define APP_BASE_ADDR 0    /* holding register */
#  define APP_BASE_LOW  18
#  define APP_BASE_HIGH 27

#define APP_VAL_ADDR  0    /* input register */

#endif

