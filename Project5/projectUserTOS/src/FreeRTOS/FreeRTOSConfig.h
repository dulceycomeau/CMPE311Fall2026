/*
 * FreeRTOSConfig.h : FreeRTOS-Kernel V11.3.1 (FreeRTOS 202604.01 LTS)
 * configuration for PROJECT-USERTOS on the ELEGOO Uno R3 (ATmega328P, 16 MHz).
 * CMPE 311 - UMBC - Dulcey Comeau
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Scheduler ------------------------------------------------------------ */
#define configUSE_PREEMPTION                     1   /* preemptive, priority based */
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       16000000UL
#define configTICK_RATE_HZ                       1000  /* 1 tick = 1.000 ms (Timer1) */
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS /* wraps like millis() */
#define configMAX_PRIORITIES                     4     /* 0 idle, 1 serial, 2 fan, 3 LEDs */
#define configMINIMAL_STACK_SIZE                 85    /* bytes; used by the idle task */
#define configMAX_TASK_NAME_LEN                  5
#define configIDLE_SHOULD_YIELD                  1
#define configNUMBER_OF_CORES                    1
#define configSTACK_DEPTH_TYPE                   uint16_t

/* ---- Memory: static allocation only (no heap) ----------------------------- */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      1     /* kernel supplies idle task memory */

/* ---- Features used -------------------------------------------------------- */
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1

/* ---- Features not used (smaller code and RAM) ----------------------------- */
#define configUSE_MUTEXES                        0
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_QUEUE_SETS                     0
#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_TIMERS                         0
#define configUSE_CO_ROUTINES                    0
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0
#define configUSE_TRACE_FACILITY                 0
#define configUSE_STATS_FORMATTING_FUNCTIONS     0
#define configGENERATE_RUN_TIME_STATS            0

/* ---- Hooks and checking --------------------------------------------------- */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0
#define configCHECK_FOR_STACK_OVERFLOW           2     /* calls vApplicationStackOverflowHook() */
#define configASSERT( x )    if( ( x ) == 0 ) { portDISABLE_INTERRUPTS(); for( ; ; ) { } }

/* ---- Optional API functions ----------------------------------------------- */
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_vTaskPrioritySet                 0
#define INCLUDE_uxTaskPriorityGet                0
#define INCLUDE_vTaskDelete                      0
#define INCLUDE_vTaskSuspend                     0
#define INCLUDE_xTaskGetSchedulerState           0
#define INCLUDE_xTaskGetCurrentTaskHandle        0
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_eTaskGetState                    0
#define INCLUDE_xTaskAbortDelay                  0
#define INCLUDE_xTaskGetHandle                   0
#define INCLUDE_xTaskResumeFromISR               0

#endif /* FREERTOS_CONFIG_H */
