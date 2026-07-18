#ifndef __DRV_MOTOR_H
#define __DRV_MOTOR_H

#include "stm32f10x.h"

/* ===== 电机参数（MG310 直流减速电机） ===== */
#define ENC_PPR         11u   /* 编码器单相每转脉冲数 */
#define ENC_GEAR_RATIO  30u   /* 减速比 1:30 */
#define ENC_CPR         (ENC_PPR * ENC_GEAR_RATIO * 4u)  /* 1320 计数/转（TIM 4× 边沿） */

/* ===== PWM 参数 ===== */
#define PWM_MAX         999                       /* 占空比有效范围 -999 ~ +999 */
#define PWM_TIM_ARR     899                       /* ARR = 899, PSC = 3, f = 72M/(4*900) = 20kHz */

/* ===== 电机 ID ===== */
#define MOTOR_ID_LEFT   0
#define MOTOR_ID_RIGHT  1

/* ===== 左电机引脚（TIM1_CH1 + TIM2 编码器） ===== */
#define MOTOR_L_PWM_PORT   GPIOA
#define MOTOR_L_PWM_PIN    GPIO_Pin_8    /* PA8 TIM1_CH1 → DRV8833 AIN1 */
#define MOTOR_L_DIR_PORT   GPIOA
#define MOTOR_L_DIR_PIN    GPIO_Pin_4    /* PA4 → DRV8833 AIN2 */
#define MOTOR_L_EN_PORT    GPIOB
#define MOTOR_L_EN_PIN     GPIO_Pin_0    /* PB0 → DRV8833 nSLEEP */
#define MOTOR_L_ENC_PORT   GPIOA
#define MOTOR_L_ENC_PIN_A  GPIO_Pin_0    /* PA0 TIM2_CH1 */
#define MOTOR_L_ENC_PIN_B  GPIO_Pin_1    /* PA1 TIM2_CH2 */

/* ===== 右电机引脚（TIM4_CH3 + TIM3 编码器） ===== */
#define MOTOR_R_PWM_PORT   GPIOB
#define MOTOR_R_PWM_PIN    GPIO_Pin_8    /* PB8 TIM4_CH3 → DRV8833 BIN1 */
#define MOTOR_R_DIR_PORT   GPIOB
#define MOTOR_R_DIR_PIN    GPIO_Pin_9    /* PB9 → DRV8833 BIN2 */
#define MOTOR_R_EN_PORT    GPIOA
#define MOTOR_R_EN_PIN     GPIO_Pin_5    /* PA5 → DRV8833 nSLEEP */
#define MOTOR_R_ENC_PORT   GPIOA
#define MOTOR_R_ENC_PIN_A  GPIO_Pin_6    /* PA6 TIM3_CH1 */
#define MOTOR_R_ENC_PIN_B  GPIO_Pin_7    /* PA7 TIM3_CH2 */

/* ===== 初始化：两个电机的 PWM/方向/使能/编码器 ===== */
void drv_motor_init(uint8_t motor_id);

/* 设置 PWM 占空比：-PWM_MAX ~ +PWM_MAX，正转为正方向 */
void drv_motor_set_pwm(uint8_t motor_id, int16_t duty);

/* 使能/禁止电机：0=禁止（DRV8833 休眠）、1=使能 */
void drv_motor_set_enable(uint8_t motor_id, uint8_t en);

/* 周期调用（5ms），读取编码器并更新对应电机的实测值 */
void drv_motor_update(uint8_t motor_id);

#endif
