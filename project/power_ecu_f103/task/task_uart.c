#include "task_uart.h"
#include "Mod_Motor.h"
#include <string.h>

/* 编码器调试打印开关：enc on/off 命令控制 */
uint8_t enc_debug_enabled = 0;

/* ===== 辅助打印函数 ===== */

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

	while (i > 0) Usart_SendByte((uint8_t)buf[--i]);
}

/** 打印 8 位十六进制 */
static void uart_put_hex8(uint8_t val)
{
	static const char hex[] = "0123456789ABCDEF";
	Usart_SendByte((uint8_t)hex[val >> 4]);
	Usart_SendByte((uint8_t)hex[val & 0x0F]);
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

/** 打印单电机状态行 */
static void print_motor_line(const char *name, Motor_Struct *m)
{
	Usart_SendString(name);
	Usart_SendString(" spd:");
	uart_put_i16(m->cur_speed_enc);
	Usart_SendString("/");
	uart_put_i16(m->target_speed_enc);
	Usart_SendString(" ang:");
	uart_put_i16(m->cur_angle_enc);
	Usart_SendString(" st:");
	uart_put_hex8(m->status);
	Usart_SendString("\r\n");
}

/* ===== 命令解析（强定义，覆盖 Mod_Usart.c 的弱符号）===== */

void Usart_ParseCommand(const char *cmd)
{
	if (cmd == NULL) return;

	/* ── motor left <speed> ── */
	if (strncmp(cmd, "motor left ", 11) == 0)
	{
		int16_t spd = parse_i16(cmd + 11);
		motor_left.target_speed_enc = spd;
		Usart_SendString("Left target: ");
		uart_put_i16(spd);
		Usart_SendString("\r\n");
	}
	/* ── motor right <speed> ── */
	else if (strncmp(cmd, "motor right ", 12) == 0)
	{
		int16_t spd = parse_i16(cmd + 12);
		motor_right.target_speed_enc = spd;
		Usart_SendString("Right target: ");
		uart_put_i16(spd);
		Usart_SendString("\r\n");
	}
	/* ── motor stop ── */
	else if (strcmp(cmd, "motor stop") == 0)
	{
		motor_left.target_speed_enc  = 0;
		motor_right.target_speed_enc = 0;
		Usart_SendString("Motors stopped\r\n");
	}
	/* ── status ── */
	else if (strcmp(cmd, "status") == 0)
	{
		Usart_SendString("--- Motor Status ---\r\n");
		print_motor_line("L", &motor_left);
		print_motor_line("R", &motor_right);
	}
	/* ── enc on / off ── */
	else if (strcmp(cmd, "enc on") == 0)
	{
		enc_debug_enabled = 1;
		Usart_SendString("enc ON\r\n");
	}
	else if (strcmp(cmd, "enc off") == 0)
	{
		enc_debug_enabled = 0;
		Usart_SendString("enc OFF\r\n");
	}
	/* ── echo ── */
	else if (strncmp(cmd, "echo ", 5) == 0)
	{
		Usart_SendString(cmd + 5);
	}
	/* ── help ── */
	else if (strcmp(cmd, "help") == 0)
	{
		Usart_SendString("motor left <spd>  - set left motor speed (rpm*10)\r\n");
		Usart_SendString("motor right <spd> - set right motor speed (rpm*10)\r\n");
		Usart_SendString("motor stop        - emergency stop both motors\r\n");
		Usart_SendString("status            - show motor status\r\n");
		Usart_SendString("enc on/off        - toggle encoder debug print\r\n");
		Usart_SendString("echo <msg>        - echo message\r\n");
		Usart_SendString("help              - show this help\r\n");
	}
	else
	{
		Usart_SendString("? ");
		Usart_SendString(cmd);
	}
}

/* ===== 任务函数 ===== */

void Task_Uart_Init(void)
{
	Mod_Usart_Init();
}

void Task_Uart_Rx(void)
{
	Usart_Rx_Process();

	/* 编码器调试打印：enc on 时每 500ms 输出一次 */
	if (enc_debug_enabled)
	{
		static uint8_t _tick;
		if (++_tick >= 25)   /* 25 × 20ms = 500ms */
		{
			_tick = 0;
			print_motor_line("L", &motor_left);
			print_motor_line("R", &motor_right);
		}
	}
}

void Task_Uart_Tx(void)
{
	Usart_Tx_Process();
}
