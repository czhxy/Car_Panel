#include "drv_motor.h"
#include "Mod_Motor.h"

/* ── 左电机静态状态 ── */
static int16_t last_enc_cnt_l;           /* 左编码器上一次计数值 */
static uint8_t  motor_enabled_l;         /* 左电机当前使能状态 */

/* ── 右电机静态状态 ── */
static int16_t last_enc_cnt_r;           /* 右编码器上一次计数值 */
static uint8_t  motor_enabled_r;         /* 右电机当前使能状态 */

/* ───────────────────────────────────────────
 *  drv_motor_init
 *  初始化指定电机的 PWM(20kHz)、DIR、EN、编码器
 * ─────────────────────────────────────────── */
void drv_motor_init(uint8_t motor_id)
{
	GPIO_InitTypeDef        gpio;
	TIM_TimeBaseInitTypeDef tim_base;
	TIM_OCInitTypeDef       tim_oc;

	if (motor_id == MOTOR_ID_LEFT)
	{
		/* ===== 左电机：TIM1_CH1 PWM + TIM2 编码器 ===== */

		/* ── 时钟使能 ── */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
		                       RCC_APB2Periph_TIM1  |
		                       RCC_APB2Periph_AFIO, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

		/* IN1 (PA4) — 推挽输出，默认低 */
		gpio.GPIO_Pin   = MOTOR_L_IN1_PIN;
		gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(MOTOR_L_IN1_PORT, &gpio);
		GPIO_ResetBits(MOTOR_L_IN1_PORT, MOTOR_L_IN1_PIN);

		/* IN2 (PB0) — 推挽输出，默认低 */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		gpio.GPIO_Pin   = MOTOR_L_IN2_PIN;
		gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(MOTOR_L_IN2_PORT, &gpio);
		GPIO_ResetBits(MOTOR_L_IN2_PORT, MOTOR_L_IN2_PIN);

		/* PA8 (TIM1_CH1) — 复用推挽输出 */
		gpio.GPIO_Pin   = MOTOR_L_PWM_PIN;
		gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(MOTOR_L_PWM_PORT, &gpio);

		/* TIM1 PWM 配置 — 20kHz (72M / 4 / 900) */
		TIM_TimeBaseStructInit(&tim_base);
		tim_base.TIM_Prescaler         = 3;
		tim_base.TIM_Period            = PWM_TIM_ARR;
		tim_base.TIM_CounterMode       = TIM_CounterMode_Up;
		tim_base.TIM_ClockDivision     = TIM_CKD_DIV1;
		tim_base.TIM_RepetitionCounter = 0;
		TIM_TimeBaseInit(TIM1, &tim_base);

		TIM_OCStructInit(&tim_oc);
		tim_oc.TIM_OCMode      = TIM_OCMode_PWM1;
		tim_oc.TIM_OutputState = TIM_OutputState_Enable;
		tim_oc.TIM_Pulse       = 0;
		tim_oc.TIM_OCPolarity  = TIM_OCPolarity_High;
		TIM_OC1Init(TIM1, &tim_oc);
		TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

		TIM_CtrlPWMOutputs(TIM1, ENABLE);
		TIM_Cmd(TIM1, ENABLE);

		/* TIM2 编码器模式 — PA0/PA1 上拉输入 */
		gpio.GPIO_Pin  = MOTOR_L_ENC_PIN_A | MOTOR_L_ENC_PIN_B;
		gpio.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_Init(MOTOR_L_ENC_PORT, &gpio);

		TIM_EncoderInterfaceConfig(TIM2,
		                           TIM_EncoderMode_TI12,
		                           TIM_ICPolarity_Rising,
		                           TIM_ICPolarity_Rising);

		/* 数字滤波：IC1F=IC2F=0xF（~3.5μs 毛刺抑制，防止 GPIO 切换干扰编码器） */
		TIM2->CCMR1 |= (uint16_t)(0x0F << 4) | (uint16_t)(0x0F << 12);

		tim_base.TIM_Period    = 0xFFFF;
		tim_base.TIM_Prescaler = 0;
		TIM_TimeBaseInit(TIM2, &tim_base);

		TIM_SetCounter(TIM2, 0);
		TIM_Cmd(TIM2, ENABLE);

		last_enc_cnt_l = 0;
	}
	else if (motor_id == MOTOR_ID_RIGHT)
	{
		/* ===== 右电机：TIM4_CH3 PWM + TIM3 编码器 ===== */

		/* ── 时钟使能 ── */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
		                       RCC_APB2Periph_GPIOB |
		                       RCC_APB2Periph_AFIO, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 |
		                       RCC_APB1Periph_TIM4, ENABLE);

		/* IN1 (PB9) — 推挽输出，默认低 */
		gpio.GPIO_Pin   = MOTOR_R_IN1_PIN;
		gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(MOTOR_R_IN1_PORT, &gpio);
		GPIO_ResetBits(MOTOR_R_IN1_PORT, MOTOR_R_IN1_PIN);

		/* IN2 (PA5) — 推挽输出，默认低 */
		gpio.GPIO_Pin   = MOTOR_R_IN2_PIN;
		gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(MOTOR_R_IN2_PORT, &gpio);
		GPIO_ResetBits(MOTOR_R_IN2_PORT, MOTOR_R_IN2_PIN);

		/* PB8 (TIM4_CH3) — 复用推挽输出 */
		gpio.GPIO_Pin   = MOTOR_R_PWM_PIN;
		gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
		gpio.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(MOTOR_R_PWM_PORT, &gpio);

		/* TIM4 PWM 配置 — 20kHz (72M / 4 / 900) */
		TIM_TimeBaseStructInit(&tim_base);
		tim_base.TIM_Prescaler         = 3;
		tim_base.TIM_Period            = PWM_TIM_ARR;
		tim_base.TIM_CounterMode       = TIM_CounterMode_Up;
		tim_base.TIM_ClockDivision     = TIM_CKD_DIV1;
		tim_base.TIM_RepetitionCounter = 0;
		TIM_TimeBaseInit(TIM4, &tim_base);

		TIM_OCStructInit(&tim_oc);
		tim_oc.TIM_OCMode      = TIM_OCMode_PWM1;
		tim_oc.TIM_OutputState = TIM_OutputState_Enable;
		tim_oc.TIM_Pulse       = 0;
		tim_oc.TIM_OCPolarity  = TIM_OCPolarity_High;
		TIM_OC3Init(TIM4, &tim_oc);
		TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);

		TIM_Cmd(TIM4, ENABLE);

		/* TIM3 编码器模式 — PA6/PA7 上拉输入 */
		gpio.GPIO_Pin  = MOTOR_R_ENC_PIN_A | MOTOR_R_ENC_PIN_B;
		gpio.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_Init(MOTOR_R_ENC_PORT, &gpio);

		TIM_EncoderInterfaceConfig(TIM3,
		                           TIM_EncoderMode_TI12,
		                           TIM_ICPolarity_Rising,
		                           TIM_ICPolarity_Rising);

		/* 数字滤波：IC1F=IC2F=0xF（~3.5μs 毛刺抑制，防止 GPIO 切换干扰编码器） */
		TIM3->CCMR1 |= (uint16_t)(0x0F << 4) | (uint16_t)(0x0F << 12);

		tim_base.TIM_Period    = 0xFFFF;
		tim_base.TIM_Prescaler = 0;
		TIM_TimeBaseInit(TIM3, &tim_base);

		TIM_SetCounter(TIM3, 0);
		TIM_Cmd(TIM3, ENABLE);

		last_enc_cnt_r = 0;
	}
}

/* ───────────────────────────────────────────
 *  drv_motor_set_pwm
 *
 *  TB6612 IN/IN 模式方向控制（IN1/IN2 双 GPIO）：
 *   正转: IN1=H, IN2=L, PWM 占空比
 *   反转: IN1=L, IN2=H, PWM 占空比
 *   刹车: IN1=L, IN2=L, PWM=0
 *
 *  duty: -999 ~ +999，正值正转、负值反转
 * ─────────────────────────────────────────── */
void drv_motor_set_pwm(uint8_t motor_id, int16_t duty)
{
	GPIO_TypeDef *in1_port, *in2_port;
	uint16_t      in1_pin,  in2_pin;
	uint16_t      ccr;

	/* 限幅 */
	if (duty > PWM_MAX)  duty = PWM_MAX;
	if (duty < -PWM_MAX) duty = -PWM_MAX;

	if (motor_id == MOTOR_ID_LEFT)
	{
		in1_port = MOTOR_L_IN1_PORT;
		in1_pin  = MOTOR_L_IN1_PIN;
		in2_port = MOTOR_L_IN2_PORT;
		in2_pin  = MOTOR_L_IN2_PIN;
	}
	else
	{
		in1_port = MOTOR_R_IN1_PORT;
		in1_pin  = MOTOR_R_IN1_PIN;
		in2_port = MOTOR_R_IN2_PORT;
		in2_pin  = MOTOR_R_IN2_PIN;
	}

	if (duty > 0)
	{
		/* 正转：IN1=H, IN2=L */
		GPIO_SetBits(in1_port, in1_pin);
		GPIO_ResetBits(in2_port, in2_pin);
		ccr = (uint16_t)((uint32_t)duty * PWM_TIM_ARR / PWM_MAX);
	}
	else if (duty < 0)
	{
		/* 反转：IN1=L, IN2=H */
		GPIO_ResetBits(in1_port, in1_pin);
		GPIO_SetBits(in2_port, in2_pin);
		ccr = (uint16_t)((uint32_t)(-duty) * PWM_TIM_ARR / PWM_MAX);
	}
	else
	{
		/* 刹车：IN1=L, IN2=L */
		GPIO_ResetBits(in1_port, in1_pin);
		GPIO_ResetBits(in2_port, in2_pin);
		ccr = 0;
	}

	if (motor_id == MOTOR_ID_LEFT)
		TIM_SetCompare1(TIM1, ccr);
	else
		TIM_SetCompare3(TIM4, ccr);
}

/* ───────────────────────────────────────────
 *  drv_motor_set_enable
 *  TB6612 STBY 硬接 VCC，单电机启停靠 PWM=0 刹车：
 *   0=禁止 → PWM清零（H桥下管导通，刹车制动）
 *   1=使能 → 允许 PID 控制
 * ─────────────────────────────────────────── */
void drv_motor_set_enable(uint8_t motor_id, uint8_t en)
{
	uint8_t      *p_enabled;
	Motor_Struct *p_motor;

	if (motor_id == MOTOR_ID_LEFT)
	{
		p_enabled = &motor_enabled_l;
		p_motor   = &motor_left;
	}
	else
	{
		p_enabled = &motor_enabled_r;
		p_motor   = &motor_right;
	}

	if (en)
	{
		*p_enabled = 1;
		p_motor->status |= MOTOR_STATUS_ENABLE;
	}
	else
	{
		/* PWM=0 → TB6612 下管导通刹车 */
		drv_motor_set_pwm(motor_id, 0);
		*p_enabled = 0;
		p_motor->status &= ~(uint8_t)(MOTOR_STATUS_ENABLE | MOTOR_STATUS_RUN);
	}
}

/* ───────────────────────────────────────────
 *  drv_motor_update — 每 5ms 由任务层调用
 *
 *  转速 rpm×10 = delta_count × 1000 / 11
 *  角度 °×10 = pos × 3600 / 1320 (0~3599)
 * ─────────────────────────────────────────── */
void drv_motor_update(uint8_t motor_id)
{
	TIM_TypeDef  *enc_tim;
	int16_t      *p_last_enc;
	uint8_t       enabled;
	Motor_Struct *p_motor;
	int16_t       enc_cnt;
	int16_t       delta;
	int32_t       pos_raw;

	if (motor_id == MOTOR_ID_LEFT)
	{
		enc_tim    = TIM2;
		p_last_enc = &last_enc_cnt_l;
		enabled    = motor_enabled_l;
		p_motor    = &motor_left;
	}
	else
	{
		enc_tim    = TIM3;
		p_last_enc = &last_enc_cnt_r;
		enabled    = motor_enabled_r;
		p_motor    = &motor_right;
	}

	/* 读编码器（TIM 做有符号 16-bit，配合 int16_t 自动处理回绕） */
	enc_cnt = (int16_t)TIM_GetCounter(enc_tim);

	/* ── 转速（rpm×10） ── */
	delta  = enc_cnt - *p_last_enc;
	*p_last_enc = enc_cnt;

	p_motor->cur_speed_enc = (int16_t)(((int32_t)delta * (int32_t)RPM_FACTOR) / (int32_t)ENC_CPR);

	/* ── 角度 °×10（0 ~ 3599） ── */
	pos_raw = (int32_t)enc_cnt % (int32_t)ENC_CPR;
	if (pos_raw < 0)
	{
		pos_raw += (int32_t)ENC_CPR;
	}
	p_motor->cur_angle_enc = (int16_t)((pos_raw * 3600) / (int32_t)ENC_CPR);

	/* ── 电流：无采样电阻，暂填 0 ── */
	p_motor->cur_current = 0;

	/* ── 状态位 ── */
	if (enabled)
	{
		p_motor->status |= MOTOR_STATUS_ENABLE;
		if (p_motor->cur_speed_enc != 0)
		{
			p_motor->status |= MOTOR_STATUS_RUN;
		}
		else
		{
			p_motor->status &= (uint8_t)(~MOTOR_STATUS_RUN);
		}
	}
}

/* ───────────────────────────────────────────
 *  drv_motor_get_raw_enc — 读取原始编码器计数值
 *  返回有符号 16bit，用于调试时观察计数器原始值
 * ─────────────────────────────────────────── */
int16_t drv_motor_get_raw_enc(uint8_t motor_id)
{
	if (motor_id == MOTOR_ID_LEFT)
		return (int16_t)TIM_GetCounter(TIM2);
	else
		return (int16_t)TIM_GetCounter(TIM3);
}

/* ───────────────────────────────────────────
 *  drv_motor_enc_tim_cmd — 启停编码器定时器
 *  用于诊断：禁用单路 TIM 后，若该 TIM 计数仍变 → 硬件串扰
 * ─────────────────────────────────────────── */
void drv_motor_enc_tim_cmd(uint8_t motor_id, FunctionalState NewState)
{
	if (motor_id == MOTOR_ID_LEFT)
		TIM_Cmd(TIM2, NewState);
	else
		TIM_Cmd(TIM3, NewState);
}
