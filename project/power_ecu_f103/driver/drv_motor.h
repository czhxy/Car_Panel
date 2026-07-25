#ifndef __DRV_MOTOR_H
#define __DRV_MOTOR_H

#include "stm32f10x.h"

/* ===== 电机型号选择（修改 MOTOR_MODEL 即可切换）=====
 * MG513 (当前使用): 13 极对, 28:1 减速比, 额定 12V
 * MG310 (之前使用): 13 极对, 20:1 减速比, 额定 7.4V */
#define MOTOR_MODEL_MG513  1
#define MOTOR_MODEL_MG310  0

#define MOTOR_MODEL  MOTOR_MODEL_MG513  /* ← 切换电机：改为 MOTOR_MODEL_MG310 */

#if MOTOR_MODEL == MOTOR_MODEL_MG513
  #define ENC_PPR         13u      /* 编码器单相每转脉冲数 */
  #define ENC_GEAR_RATIO  28u      /* 减速比 1:28 */
  #define MOTOR_RATED_V   12.0f    /* 额定电压 (V) */
#else
  #define ENC_PPR         13u      /* 编码器单相每转脉冲数 */
  #define ENC_GEAR_RATIO  20u      /* 减速比 1:20 */
  #define MOTOR_RATED_V   7.4f     /* 额定电压 (V) */
#endif

#define ENC_CPR     (ENC_PPR * 4u * ENC_GEAR_RATIO)  /* 有效计数/转 = 13×4×减速比 */
#define RPM_FACTOR  120000u  /* rpm×10 换算因子: (1000ms/5ms) × 60s × 10 */

/* ===== PWM 参数 ===== */
#define PWM_MAX         999                       /* 占空比有效范围 -999 ~ +999 */
#define PWM_TIM_ARR     899                       /* ARR = 899, PSC = 3, f = 72M/(4*900) = 20kHz */

/* ===== 电机 ID ===== */
#define MOTOR_ID_LEFT   0
#define MOTOR_ID_RIGHT  1

/* ===== 左电机引脚（TIM1_CH1 + TIM2 编码器） ===== */
#define MOTOR_L_PWM_PORT   GPIOA
#define MOTOR_L_PWM_PIN    GPIO_Pin_8    /* PA8 TIM1_CH1 → TB6612 PWMA */
#define MOTOR_L_IN1_PORT   GPIOA
#define MOTOR_L_IN1_PIN    GPIO_Pin_4    /* PA4 → TB6612 AIN1 */
#define MOTOR_L_IN2_PORT   GPIOB
#define MOTOR_L_IN2_PIN    GPIO_Pin_0    /* PB0 → TB6612 AIN2 */
#define MOTOR_L_ENC_PORT   GPIOA
#define MOTOR_L_ENC_PIN_A  GPIO_Pin_0    /* PA0 TIM2_CH1 */
#define MOTOR_L_ENC_PIN_B  GPIO_Pin_1    /* PA1 TIM2_CH2 */

/* ===== 右电机引脚（TIM4_CH3 + TIM3 编码器） ===== */
#define MOTOR_R_PWM_PORT   GPIOB
#define MOTOR_R_PWM_PIN    GPIO_Pin_8    /* PB8 TIM4_CH3 → TB6612 PWMB */
#define MOTOR_R_IN1_PORT   GPIOB
#define MOTOR_R_IN1_PIN    GPIO_Pin_9    /* PB9 → TB6612 BIN1 */
#define MOTOR_R_IN2_PORT   GPIOA
#define MOTOR_R_IN2_PIN    GPIO_Pin_5    /* PA5 → TB6612 BIN2 */
#define MOTOR_R_ENC_PORT   GPIOA
#define MOTOR_R_ENC_PIN_A  GPIO_Pin_6    /* PA6 TIM3_CH1 */
#define MOTOR_R_ENC_PIN_B  GPIO_Pin_7    /* PA7 TIM3_CH2 */

/*
 * TB6612FNG 单芯片引脚:
 *   AIN1 → PA4, AIN2 → PB0 (双 GPIO 控制方向)
 *   BIN1 → PB9, BIN2 → PA5
 *   STBY → VCC (5V), 硬接线始终使能
 *   VM   → 12V (稳压模块 / 3S 电池)
 *   VCC  → 5V  (稳压模块)
 */

/* ===== 初始化：两个电机的 PWM/方向/使能/编码器 ===== */
void drv_motor_init(uint8_t motor_id);

/* 设置 PWM 占空比：-PWM_MAX ~ +PWM_MAX，正转为正方向 */
void drv_motor_set_pwm(uint8_t motor_id, int16_t duty);

/* 使能/禁止电机：0=禁止（PWM清零刹车）、1=使能（恢复PID控制）
 * 注：TB6612 STBY 硬接 VCC，单电机启停靠 PWM=0 刹车实现 */
void drv_motor_set_enable(uint8_t motor_id, uint8_t en);

/* 周期调用（5ms），读取编码器并更新对应电机的实测值 */
void drv_motor_update(uint8_t motor_id);

/* 读取原始编码器计数值（int16_t 有符号，用于调试） */
int16_t drv_motor_get_raw_enc(uint8_t motor_id);

/* 启停编码器定时器（用于诊断：禁用单路 TIM 以排查硬件串扰） */
void drv_motor_enc_tim_cmd(uint8_t motor_id, FunctionalState NewState);

#endif
