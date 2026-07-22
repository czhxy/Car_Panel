#ifndef __DRV_MOTOR_H
#define __DRV_MOTOR_H

#include "stm32f10x.h"

/* ===== 电机参数（临时电机，后期更换） =====
 * 当前电机编码器反转时 TIM3 4×=1040 计数/转正常，
 * 正转时因蜗轮蜗杆自锁特性只计 260。暂按 260 配置。 */
#define ENC_PPR         13u   /* 编码器单相每转脉冲数 */
#define ENC_GEAR_RATIO  20u   /* 减速比 1:20 */
#define ENC_CPR         260u  /* 有效计数/转（临时，换电机后改为 13×20×4=1040） */
#define RPM_FACTOR      120000u  /* rpm×10 换算因子: (1000ms/5ms) × 60s × 10 */

/* ===== PWM 参数 ===== */
#define PWM_MAX         999                       /* 占空比有效范围 -999 ~ +999 */
#define PWM_TIM_ARR     899                       /* ARR = 899, PSC = 3, f = 72M/(4*900) = 20kHz */

/* ===== 电机 ID ===== */
#define MOTOR_ID_LEFT   0
#define MOTOR_ID_RIGHT  1

/* ===== 左电机引脚（TIM1_CH1 + TIM2 编码器） ===== */
#define MOTOR_L_PWM_PORT   GPIOA
#define MOTOR_L_PWM_PIN    GPIO_Pin_8    /* PA8 TIM1_CH1 → TB6612 PWMA */
#define MOTOR_L_DIR_PORT   GPIOA
#define MOTOR_L_DIR_PIN    GPIO_Pin_4    /* PA4 → TB6612 AIN1 */
#define MOTOR_L_ENC_PORT   GPIOA
#define MOTOR_L_ENC_PIN_A  GPIO_Pin_0    /* PA0 TIM2_CH1 */
#define MOTOR_L_ENC_PIN_B  GPIO_Pin_1    /* PA1 TIM2_CH2 */

/* ===== 右电机引脚（TIM4_CH3 + TIM3 编码器） ===== */
#define MOTOR_R_PWM_PORT   GPIOB
#define MOTOR_R_PWM_PIN    GPIO_Pin_8    /* PB8 TIM4_CH3 → TB6612 PWMB */
#define MOTOR_R_DIR_PORT   GPIOB
#define MOTOR_R_DIR_PIN    GPIO_Pin_9    /* PB9 → TB6612 BIN1 */
#define MOTOR_R_ENC_PORT   GPIOA
#define MOTOR_R_ENC_PIN_A  GPIO_Pin_6    /* PA6 TIM3_CH1 */
#define MOTOR_R_ENC_PIN_B  GPIO_Pin_7    /* PA7 TIM3_CH2 */

/*
 * TB6612FNG 单芯片全局引脚 (不归具体电机)：
 *   STBY  → VCC (5V), 硬接线始终使能
 *   AIN2  → GND, 低侧接地
 *   BIN2  → GND, 低侧接地
 *   VM    → 7.4V (稳压模块)
 *   VCC   → 5V (稳压模块)
 *
 * 空闲引脚: PB0 (原左 EN), PA5 (原右 EN)
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

#endif
