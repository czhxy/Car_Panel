#include "mod_motor.h"
#include "mod_dashboard_data.h"

/* Mod_Motor_Get_Speed: 返回 LVGL 仪表盘当前目标转速 (rpm_target)，与表盘显示一致。
 * 上层 CanProtocol_WheelCtlSend 会将值 *10 后按 int16 编码进控制帧。
 * Mod_Motor_Angle 已移除：控制帧角度字段直接填 0。 */
float Mod_Motor_Get_Speed(void)
{
    DashboardState snap = Dashboard_Data_GetSnapshot();
    return (float)snap.rpm_target;
}
