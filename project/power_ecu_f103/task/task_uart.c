#include "task_uart.h"
#include "Mod_Motor.h"
#include <string.h>
#include <stdio.h>

/* PID 调试打印开关：pid l/r on/off 命令控制 */
uint8_t pid_debug_enabled = 0;
uint8_t pid_debug_side    = 0;    /* 0=左电机, 1=右电机 */

/* VOFA+ FireWater 波形输出开关：vofa l/r on/off 命令控制 */
uint8_t vofa_enabled = 0;
uint8_t vofa_side    = 0;         /* 0=左电机, 1=右电机 */

/* ===== 辅助函数 ===== */

/** 根据侧(l/r)返回电机指针 */
static Motor_Struct* get_motor(char side)
{
	return (side == 'l' || side == 'L') ? &motor_left : &motor_right;
}

/** 根据侧(l/r)返回 PID 指针 */
static PidController* get_pid(char side)
{
	return (side == 'l' || side == 'L') ? &pid_left : &pid_right;
}

/** 侧(l/r) → 字符 'L'/'R' */
static char side_char(char side)
{
	return (side == 'l' || side == 'L') ? 'L' : 'R';
}

/* ===== 打印函数 ===== */

/** 打印有符号 16 位整数 */
static void uart_put_i16(int16_t val)
{
	char buf[7];
	uint8_t i = 0;
	uint8_t neg = 0;

	if (val < 0) { neg = 1; val = (int16_t)(-val); }

	do {
		buf[i++] = (char)('0' + (uint8_t)(val % 10));
		val /= 10;
	} while (val > 0);

	if (neg) buf[i++] = '-';

	while (i > 0) Mod_Usart_SendByte((uint8_t)buf[--i]);
}

/** 打印 8 位十六进制 */
static void uart_put_hex8(uint8_t val)
{
	static const char hex[] = "0123456789ABCDEF";
	Mod_Usart_SendByte((uint8_t)hex[val >> 4]);
	Mod_Usart_SendByte((uint8_t)hex[val & 0x0F]);
}

/** 字符串 → int16_t（支持负号） */
static int16_t parse_i16(const char *s)
{
	int16_t val = 0;
	uint8_t neg = 0;

	while (*s == ' ') s++;
	if (*s == '-') { neg = 1; s++; }
	else if (*s == '+') { s++; }

	while (*s >= '0' && *s <= '9')
	{
		val = (int16_t)(val * 10 + (*s - '0'));
		s++;
	}

	return neg ? (int16_t)(-val) : val;
}

/** 字符串 → float（用于 PID 参数解析） */
static float parse_f32(const char *s)
{
	float val = 0.0f, frac = 0.0f, divisor = 1.0f;
	uint8_t neg = 0, decimal = 0;

	while (*s == ' ') s++;
	if (*s == '-') { neg = 1; s++; }
	else if (*s == '+') { s++; }

	while ((*s >= '0' && *s <= '9') || *s == '.')
	{
		if (*s == '.') { decimal = 1; s++; continue; }
		if (decimal) { divisor *= 10.0f; frac = frac * 10.0f + (float)(*s - '0'); }
		else          { val = val * 10.0f + (float)(*s - '0'); }
		s++;
	}

	return neg ? -(val + frac / divisor) : (val + frac / divisor);
}

/** 打印 float（3 位小数），用于 PID 增益显示 */
static void uart_put_f32(float f)
{
	int32_t v;
	uint16_t frac;
	uint8_t ndig = 0;
	uint8_t neg = 0;
	char dbuf[10];

	if (f < 0.0f) { neg = 1; f = -f; }

	v = (int32_t)(f * 1000.0f + 0.5f);
	frac = (uint16_t)(v % 1000);
	v /= 1000;

	if (neg) Mod_Usart_SendByte('-');

	/* 整数部分：支持多位数 */
	if (v == 0) { Mod_Usart_SendByte('0'); }
	else
	{
		while (v > 0 && ndig < (uint8_t)sizeof(dbuf))
		{
			dbuf[ndig++] = (char)('0' + (v % 10));
			v /= 10;
		}
		while (ndig > 0) Mod_Usart_SendByte((uint8_t)dbuf[--ndig]);
	}

	Mod_Usart_SendByte('.');
	Mod_Usart_SendByte((uint8_t)('0' + ((frac / 100) % 10)));
	Mod_Usart_SendByte((uint8_t)('0' + ((frac / 10)  % 10)));
	Mod_Usart_SendByte((uint8_t)('0' + ( frac        % 10)));
}

/** 打印单电机状态行 */
static void print_motor_line(char side, Motor_Struct *m)
{
	Mod_Usart_SendByte(side_char(side));
	Mod_Usart_SendString(" spd:");
	uart_put_i16(m->cur_speed_enc);
	Mod_Usart_SendString("/");
	uart_put_i16(m->target_speed_enc);
	Mod_Usart_SendString(" ang:");
	uart_put_i16(m->cur_angle_enc);
	Mod_Usart_SendString(" st:");
	uart_put_hex8(m->status);
	Mod_Usart_SendString("\r\n");
}

/** PID 调试打印 — 显示目标/实测/误差/输出 */
static void print_pid_line(char side)
{
	Motor_Struct  *m = get_motor(side);
	PidController *p = get_pid(side);
	int16_t err = m->target_speed_enc - m->cur_speed_enc;

	Mod_Usart_SendString("[PID");
	Mod_Usart_SendByte(side_char(side));
	Mod_Usart_SendString("] tgt:");
	uart_put_i16(m->target_speed_enc);
	Mod_Usart_SendString(" cur:");
	uart_put_i16(m->cur_speed_enc);
	Mod_Usart_SendString(" err:");
	uart_put_i16(err);
	Mod_Usart_SendString(" out:");
	uart_put_i16(p->output);
	Mod_Usart_SendString("\r\n");
}

/** 打印 PID 参数 */
static void print_pid_params(PidController *pid)
{
	Mod_Usart_SendString("Kp=");  uart_put_f32(pid->kp);
	Mod_Usart_SendString(" Ki=");  uart_put_f32(pid->ki);
	Mod_Usart_SendString(" Kd=");  uart_put_f32(pid->kd);
	Mod_Usart_SendString(" integ="); uart_put_f32(pid->integral);
	Mod_Usart_SendString(" out="); uart_put_i16(pid->output);
	Mod_Usart_SendString("\r\n");
}

/* ===== VOFA+ FireWater 波形输出 ===== */

/**
 * Vofa_SendFrame — 由 5ms 周期 Task_Motor_Ctl 调用
 * FireWater 格式：逗号分隔 float，换行结尾
 * VOFA+ 解析器自动跳过非数字行（命令回显不干扰波形）
 */
void Vofa_SendFrame(void)
{
	Motor_Struct  *m = vofa_side ? &motor_right : &motor_left;
	PidController *p = vofa_side ? &pid_right : &pid_left;
	char buf[48];
	int len;

	/* FireWater: 一行 CSV，4 通道 */
	len = snprintf(buf, sizeof(buf),
		"%.1f,%.1f,%.1f,%.1f\n",
		(float)m->target_speed_enc / 10.0f,          /* CH1: 目标转速 rpm */
		(float)m->cur_speed_enc / 10.0f,              /* CH2: 实测转速 rpm */
		(float)p->output,                              /* CH3: PID 输出 */
		(float)(m->target_speed_enc - m->cur_speed_enc) / 10.0f  /* CH4: 误差 rpm */
	);

	if (len > 0 && len < (int)sizeof(buf))
		Mod_Usart_SendData((uint8_t *)buf, (uint16_t)len);
}

/* ===== 命令解析（强定义，覆盖 Mod_Usart.c 的弱符号）===== */

void Usart_ParseCommand(const char *cmd)
{
	if (cmd == NULL) return;

	/* ── motor l/r <spd> ── */
	if (strncmp(cmd, "motor ", 6) == 0)
	{
		char side = cmd[6];

		if ((side == 'l' || side == 'L' || side == 'r' || side == 'R')
		    && cmd[7] == ' ' && cmd[8] != '\0')
		{
			int16_t spd = parse_i16(cmd + 8);
			get_motor(side)->target_speed_enc = spd;
			Mod_Usart_SendByte(side_char(side));
			Mod_Usart_SendByte(':');
			uart_put_i16(spd);
			Mod_Usart_SendString("\r\n");
			return;
		}
	}

	/* ── motor l/r stop / motor all stop ── */
	if (strncmp(cmd, "motor ", 6) == 0)
	{
		char side = cmd[6];
		if ((side == 'l' || side == 'L' || side == 'r' || side == 'R')
		    && strcmp(cmd + 7, " stop") == 0)
		{
			get_motor(side)->target_speed_enc = 0;
			Mod_Usart_SendByte(side_char(side));
			Mod_Usart_SendString(" STOP\r\n");
			return;
		}

		if (strcmp(cmd + 6, "all stop") == 0
		    || strcmp(cmd + 6, "stop") == 0)
		{
			motor_left.target_speed_enc  = 0;
			motor_right.target_speed_enc = 0;
			Mod_Usart_SendString("ALL STOP\r\n");
			return;
		}
	}

	/* ── status [l/r] ── */
	if (strncmp(cmd, "status ", 7) == 0)
	{
		char side = cmd[7];
		if (side == 'l' || side == 'L' || side == 'r' || side == 'R')
		{
			print_motor_line(side, get_motor(side));
			return;
		}
	}
	if (strcmp(cmd, "status") == 0)
	{
		print_motor_line('l', &motor_left);
		print_motor_line('r', &motor_right);
		return;
	}

	/* ── pid l/r show — 显示 PID 参数 ── */
	if (strncmp(cmd, "pid ", 4) == 0)
	{
		char side = cmd[4];
		if ((side == 'l' || side == 'L' || side == 'r' || side == 'R')
		    && strcmp(cmd + 5, " show") == 0)
		{
			Mod_Usart_SendByte(side_char(side));
			Mod_Usart_SendString(" PID: ");
			print_pid_params(get_pid(side));
			return;
		}
	}

	/* ── pid l/r on — PID 调试打印 ── */
	if (strncmp(cmd, "pid ", 4) == 0)
	{
		char side = cmd[4];
		if ((side == 'l' || side == 'L' || side == 'r' || side == 'R')
		    && strcmp(cmd + 5, " on") == 0)
		{
			pid_debug_enabled = 1;
			pid_debug_side    = (side == 'l' || side == 'L') ? 0 : 1;
			Mod_Usart_SendByte(side_char(side));
			Mod_Usart_SendString(" PID ON\r\n");
			return;
		}
	}
	if (strcmp(cmd, "pid off") == 0)
	{
		pid_debug_enabled = 0;
		Mod_Usart_SendString("PID OFF\r\n");
		return;
	}

	/* ── pid l/r kp/ki/kd <value> ── */
	if (strncmp(cmd, "pid ", 4) == 0)
	{
		char side = cmd[4];
		if (side == 'l' || side == 'L' || side == 'r' || side == 'R')
		{
			PidController *pid = get_pid(side);
			char sc = side_char(side);

			if (strncmp(cmd + 5, " kp ", 4) == 0)
			{
				pid->kp = parse_f32(cmd + 9);
				Pid_Reset(pid);
				Mod_Usart_SendByte(sc);
				Mod_Usart_SendString(" Kp="); uart_put_f32(pid->kp);
				Mod_Usart_SendString("\r\n");
				return;
			}
			if (strncmp(cmd + 5, " ki ", 4) == 0)
			{
				pid->ki = parse_f32(cmd + 9);
				if (pid->ki > 0.001f)
					pid->integral_limit = (float)PWM_MAX / pid->ki;
				Pid_Reset(pid);
				Mod_Usart_SendByte(sc);
				Mod_Usart_SendString(" Ki="); uart_put_f32(pid->ki);
				Mod_Usart_SendString("\r\n");
				return;
			}
			if (strncmp(cmd + 5, " kd ", 4) == 0)
			{
				pid->kd = parse_f32(cmd + 9);
				Pid_Reset(pid);
				Mod_Usart_SendByte(sc);
				Mod_Usart_SendString(" Kd="); uart_put_f32(pid->kd);
				Mod_Usart_SendString("\r\n");
				return;
			}
		}
	}

	/* ── vofa l/r on — 开启 VOFA+ FireWater 波形 ── */
	if (strncmp(cmd, "vofa ", 5) == 0)
	{
		char side = cmd[5];
		if ((side == 'l' || side == 'L' || side == 'r' || side == 'R')
		    && strcmp(cmd + 6, " on") == 0)
		{
			vofa_enabled    = 1;
			vofa_side       = (side == 'l' || side == 'L') ? 0 : 1;
			pid_debug_enabled = 0;  /* 互斥：关文本 PID 打印 */
			Mod_Usart_SendByte(side_char(side));
			Mod_Usart_SendString(" VOFA ON\r\n");
			return;
		}
	}
	if (strcmp(cmd, "vofa off") == 0)
	{
		vofa_enabled = 0;
		Mod_Usart_SendString("VOFA OFF\r\n");
		return;
	}

	/* ── enc — 打印原始编码器计数值（调试用）── */
	if (strcmp(cmd, "enc") == 0)
	{
		Mod_Usart_SendString("Enc L:");
		uart_put_i16(drv_motor_get_raw_enc(0));
		Mod_Usart_SendString(" R:");
		uart_put_i16(drv_motor_get_raw_enc(1));
		Mod_Usart_SendString("\r\n");
		return;
	}

	/* ── echo ── */
	if (strncmp(cmd, "echo ", 5) == 0)
	{
		Mod_Usart_SendString(cmd + 5);
		return;
	}

	/* ── help ── */
	if (strcmp(cmd, "help") == 0)
	{
		Mod_Usart_SendString("=== Commands ===\r\n");
		Mod_Usart_SendString("motor l/r <spd>|stop  motor all stop\r\n");
		Mod_Usart_SendString("status [l/r]          pid l/r show|on|kp/ki/kd\r\n");
		Mod_Usart_SendString("pid off               vofa l/r on|off\r\n");
		Mod_Usart_SendString("echo <msg>            help\r\n");
		return;
	}

	/* ── 未知命令 ── */
	Mod_Usart_SendString("? ");
	Mod_Usart_SendString(cmd);
	Mod_Usart_SendString("\r\n");
}

/* ===== 任务函数 ===== */

void Task_Uart_Init(void)
{
	Mod_Usart_Init();
}

void Task_Uart_Rx(void)
{
	Usart_Rx_Process();

	/* PID 调试打印：每 100ms（5 × 20ms） */
	if (pid_debug_enabled)
	{
		static uint8_t _pid_tick;
		if (++_pid_tick >= 5)
		{
			_pid_tick = 0;
			print_pid_line(pid_debug_side ? 'r' : 'l');
		}
	}
}

void Task_Uart_Tx(void)
{
	Usart_Tx_Process();
}
