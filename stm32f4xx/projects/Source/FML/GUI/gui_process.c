/**
**************************************************************************************************
* @file        gui_process.c
* @author
* @version   v0.1.0
* @date        
* @brief
**************************************************************************************************
* @attention
*
**************************************************************************************************
*/
#include "gui_cmd_queue.h"
#include "gui_process.h"
#include "gui_conf.h"
#include "system_info.h"
#include "stm32_bsp_conf.h"
#include "gui_task.h"
/**
* @addtogroup    XXX 
* @{  
*/

/**
* @addtogroup    gui_process_Modules 
* @{  
*/

/**
* @defgroup      gui_process_IO_Defines 
* @brief         
* @{  
*/

/**
* @}
*/

/**
* @defgroup      gui_process_Macros_Defines 
* @brief         
* @{  
*/

/**
* @}
*/

/**
* @defgroup      gui_process_Constants_Defines 
* @brief         
* @{  
*/

/**
* @}
*/

/**
* @defgroup      gui_process_Private_Types
* @brief         
* @{  
*/
typedef struct
{
    uint8_t current_screen_id;
    uint8_t current_equip_id;
    uint8_t finger_name_buf[10];
    uint8_t finger_id_buf[10];
}s_CBvalue_t;

/**
* @}
*/

/**
* @defgroup      gui_process_Private_Variables 
* @brief         
* @{  
*/ 
uint8_t cmd_buffer[CMD_MAX_SIZE];                                                     //指令缓存

s_CBvalue_t s_CBvalue=
{
    .current_screen_id=99,
    .current_equip_id=1,
}; 
/**
* @}
*/

/**
* @defgroup      gui_process_Public_Variables 
* @brief         
* @{  
*/

/**
* @}
*/

/**
* @defgroup      gui_process_Private_FunctionPrototypes 
* @brief         
* @{  
*/
static void gui_Message_Process( GUI_Dacai_Proto_t * msg, uint16_t size );
static void gui_notify_handshake();
static void gui_notify_screen(uint16_t screen_id);
static void gui_notify_button(uint16_t screen_id, uint16_t control_id, uint8_t  state);
static void gui_notify_touchxy(uint8_t press,uint16_t x,uint16_t y);
static void gui_notify_text(uint16_t screen_id, uint16_t control_id, uint8_t *str);
static void gui_notify_progress(uint16_t screen_id, uint16_t control_id, uint32_t value);
static void gui_notify_slider(uint16_t screen_id, uint16_t control_id, uint32_t value);
static void gui_notify_meter(uint16_t screen_id, uint16_t control_id, uint32_t value);
static void gui_notify_menu(uint16_t screen_id, uint16_t control_id, uint8_t item, uint8_t state);
static void gui_notify_selector(uint16_t screen_id, uint16_t control_id, uint8_t  item);
static void gui_notify_timer(uint16_t screen_id, uint16_t control_id);
static void gui_notify_readflash(uint8_t status,uint8_t *_data,uint16_t length);
static void gui_notify_writeflash(uint8_t status);
static void gui_notify_readrtc(uint8_t year,uint8_t month,uint8_t week,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second);
static void text_screen_update_event();
/**
* @}
*/

/**
* @defgroup      gui_process_Functions 
* @brief         
* @{  
*/
/**
 * @brief    循环检查是否存在命令
 */

void Gui_CheckCMD_Loop_Process()
{   
    uint16_t size=0;
    
    size = GUI_Queue_Find_Cmd(cmd_buffer,CMD_MAX_SIZE);                          //从缓冲区中获取一条指令         
    
    if(size>0)                                              					 //接收到指令
    {                                                                           
        gui_Message_Process((GUI_Dacai_Proto_t *)cmd_buffer, size);              //指令处理
    }                                                                           
}

/**
 * @brief    消息处理流程
 * @param    msg:GUI_Dacai_Proto_t*  待处理消息
 * @param    size:uint16_t 消息长度
 */

static void gui_Message_Process( GUI_Dacai_Proto_t* msg, uint16_t size )
{
    uint8_t cmd_type = msg->cmd_type;                                                  //指令类型
    uint8_t ctrl_msg = msg->ctrl_msg;                                                  //消息的类型
    uint8_t control_type = msg->control_type;                                          //控件类型
    uint16_t screen_id = BIG2LITTLESWAP16(msg->screen_id);                             //画面ID
    uint16_t control_id = BIG2LITTLESWAP16(msg->control_id);                           //控件ID
    uint32_t value = BIG2LITTLESWAP32(msg->param);                                     //数值
    
    
    switch(cmd_type)                                                                
    {  
        case NOTIFY_TOUCH_PRESS:                                                        //触摸屏按下
        case NOTIFY_TOUCH_RELEASE:                                                      //触摸屏松开
        gui_notify_touchxy(cmd_buffer[1],BIG2LITTLESWAP16(cmd_buffer[2]),BIG2LITTLESWAP16(cmd_buffer[4])); 
        break;                                                                    
        case NOTIFY_WRITE_FLASH_OK:                                                     //写FLASH成功
        gui_notify_writeflash(1);                                                      
        break;                                                                    
        case NOTIFY_WRITE_FLASH_FAILD:                                                  //写FLASH失败
        gui_notify_writeflash(0);                                                      
        break;                                                                    
        case NOTIFY_READ_FLASH_OK:                                                      //读取FLASH成功
        gui_notify_readflash(1,cmd_buffer+2,size-6);                              	    //去除帧头帧尾
        break;                                                                    
        case NOTIFY_READ_FLASH_FAILD:                                                   //读取FLASH失败
        gui_notify_readflash(0,0,0);                                                   
        break;                                                                    
        case NOTIFY_READ_RTC:                                                           //读取RTC时间
        gui_notify_readrtc(cmd_buffer[2],cmd_buffer[3],cmd_buffer[4],cmd_buffer[5],cmd_buffer[6],cmd_buffer[7],cmd_buffer[8]);
        break;
        case NOTIFY_CONTROL:															//控件更新通知															//控件更新通知
        {
            if(ctrl_msg==MSG_GET_CURRENT_SCREEN)                                   		 //画面ID变化通知
            {
                gui_notify_screen(screen_id);                                            //画面切换调动的函数
            }
            else
            {
                switch(control_type)
                {
                    case kCtrlButton:                                                   //按钮控件
                    gui_notify_button(screen_id,control_id,msg->param[1]);                  
                    break;                                                             
                    case kCtrlText:                                                     //文本控件
                    gui_notify_text(screen_id,control_id,msg->param);                       
                    break;              
                    case kCtrlProgress:                                                 //进度条控件
                    gui_notify_progress(screen_id,control_id,value);                        
                    break;                                                             
                    case kCtrlSlider:                                                   //滑动条控件
                    gui_notify_slider(screen_id,control_id,value);                          
                    break;                                                             
                    case kCtrlMeter:                                                    //仪表控件
                    gui_notify_meter(screen_id,control_id,value);                           
                    break;                                                             
                    case kCtrlMenu:                                                     //菜单控件
                    gui_notify_menu(screen_id,control_id,msg->param[0],msg->param[1]);      
                    break;                                                              
                    case kCtrlSelector:                                                 //选择控件
                    gui_notify_selector(screen_id,control_id,msg->param[0]);                
                    break;                                                              
                    case kCtrlRTC:                                                      //倒计时控件
                    gui_notify_timer(screen_id,control_id);
                    break;
                    default:
                    break;
                }
            } 
            break;  
        } 
        case NOTIFY_HandShake:                                                          //握手通知                                                     
        gui_notify_handshake();
        break;
        default:
        break;
    }
}

/**
 * @brief    握手通知
 */

static void gui_notify_handshake()
{
    
}

/**
 * @brief    画面切换通知
 * @param    screen_id:uint16_t 当前画面ID
 */

static void gui_notify_screen(uint16_t screen_id)
{
    s_CBvalue.current_screen_id = screen_id;
    GuiTask_Send_Event(GUI_TASK_KEY_PROCESS_EVENT);    
}
/*! 
*  \brief  触摸坐标事件响应
*  \param press 1按下触摸屏，3松开触摸屏
*  \param x x坐标
*  \param y y坐标
*/
static void gui_notify_touchxy(uint8_t press,uint16_t x,uint16_t y)
{ 
    //TODO: 添加用户代码
}
/*! 
*  \brief  按钮控件通知
*  \details  当按钮状态改变(或调用GetControlValue)时，执行此函数
*  \param screen_id 画面ID
*  \param control_id 控件ID
*  \param state 按钮状态：0弹起，1按下
*/
static void gui_notify_button(uint16_t screen_id, uint16_t control_id, uint8_t  state)
{ 
    switch (screen_id)
    {
        case FINGERMANAGESCREENID:
//        GUI_TEXT_GetValue(FINGERMANAGESCREENID,FINGER_NAME_VALUCONTROLCID);
//        GUI_TEXT_GetValue(FINGERMANAGESCREENID,FINGER_ID_VALUCONTROLCID);
        break;
        default:
        break;
    }
}
/*! 
*  \brief  文本控件通知
*  \details  当文本通过键盘更新(或调用GetControlValue)时，执行此函数
*  \details  文本控件的内容以字符串形式下发到MCU，如果文本控件内容是浮点值，
*  \details  则需要在此函数中将下发字符串重新转回浮点值。
*  \param screen_id 画面ID
*  \param control_id 控件ID
*  \param str 文本控件内容
*/
static void gui_notify_text(uint16_t screen_id, uint16_t control_id, uint8_t *str)
{   
    uint8_t i=0;
    switch(screen_id)
    {
        case FINGERMANAGESCREENID:
        switch(control_id)
        {
            case FINGER_NAME_VALUCONTROLCID:
            {
                while(*str)
                {
                    s_CBvalue.finger_name_buf[i++]=*str++;
                }
                s_CBvalue.finger_name_buf[i]=0; 
            }
            case FINGER_ID_VALUCONTROLCID:
            {
                while(*str)
                {
                    s_CBvalue.finger_id_buf[i++]=*str++;
                }
                s_CBvalue.finger_id_buf[i]=0; 
            }
            default:
            break;
        }
        default:
        break;                              
    }                                 
}
/*!                                                                              
*  \brief  进度条控件通知                                                       
*  \details  调用GetControlValue时，执行此函数                                  
*  \param screen_id 画面ID                                                      
*  \param control_id 控件ID                                                     
*  \param value 值                                                              
*/                                                                              
static void gui_notify_progress(uint16_t screen_id, uint16_t control_id, uint32_t value)           
{  
    //TODO: 添加用户代码
}                                                                                

/*!                                                                              
*  \brief  滑动条控件通知                                                       
*  \details  当滑动条改变(或调用GetControlValue)时，执行此函数                  
*  \param screen_id 画面ID                                                      
*  \param control_id 控件ID                                                     
*  \param value 值                                                              
*/                                                                              
static void gui_notify_slider(uint16_t screen_id, uint16_t control_id, uint32_t value)             
{                                                             
    //TODO: 添加用户代码
}

/*! 
*  \brief  仪表控件通知
*  \details  调用GetControlValue时，执行此函数
*  \param screen_id 画面ID
*  \param control_id 控件ID
*  \param value 值
*/
static void gui_notify_meter(uint16_t screen_id, uint16_t control_id, uint32_t value)
{
    //TODO: 添加用户代码
}

/*! 
*  \brief  菜单控件通知
*  \details  当菜单项按下或松开时，执行此函数
*  \param screen_id 画面ID
*  \param control_id 控件ID
*  \param item 菜单项索引
*  \param state 按钮状态：0松开，1按下
*/
static void gui_notify_menu(uint16_t screen_id, uint16_t control_id, uint8_t item, uint8_t state)
{
    switch(screen_id)
    {
        case TEXTVALUESCREENID:
        s_CBvalue.current_equip_id=item+1;
        GuiTask_Send_Event(GUI_TASK_KEY_PROCESS_EVENT);
        break;
        default:
        break;
    }
}

/*! 
*  \brief  选择控件通知
*  \details  当选择控件变化时，执行此函数
*  \param screen_id 画面ID
*  \param control_id 控件ID
*  \param item 当前选项
*/
static void gui_notify_selector(uint16_t screen_id, uint16_t control_id, uint8_t  item)
{
    
}

/*! 
*  \brief  定时器超时通知处理
*  \param screen_id 画面ID
*  \param control_id 控件ID
*/
static void gui_notify_timer(uint16_t screen_id, uint16_t control_id)
{
    //TODO: 添加用户代码   
}

/*! 
*  \brief  读取用户FLASH状态返回
*  \param status 0失败，1成功
*  \param _data 返回数据
*  \param length 数据长度
*/
static void gui_notify_readflash(uint8_t status,uint8_t *_data,uint16_t length)
{
    //TODO: 添加用户代码
}

/*! 
*  \brief  写用户FLASH状态返回
*  \param status 0失败，1成功
*/
static void gui_notify_writeflash(uint8_t status)
{
    //TODO: 添加用户代码
}

/*! 
*  \brief  读取RTC时间，注意返回的是BCD码
*  \param year 年（BCD）
*  \param month 月（BCD）
*  \param week 星期（BCD）
*  \param day 日（BCD）
*  \param hour 时（BCD）
*  \param minute 分（BCD）
*  \param second 秒（BCD）
*/
static void gui_notify_readrtc(uint8_t year,uint8_t month,uint8_t week,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second)
{
    //TODO: 添加用户代码
}


/*! 
*  \brief  更新数据
*/ 
void GUI_Update(void)
{   
    switch(s_CBvalue.current_screen_id)
    {
        case TEXTVALUESCREENID:
        text_screen_update_event();
        break;
        case FINGERMANAGESCREENID:
        break;
        
        case CONFIGSCREENID:
        
        break;
        default:        
        break;
    }
}
static void text_screen_update_event()
{   
    uint8_t sprint_buf[100];
    GUI_TEXT_SetInt32(TEXTVALUESCREENID,EQUIP_ID_VALUEID,s_CBvalue.current_equip_id,0,4);
    GUI_GIF_Pause(TEXTVALUESCREENID,GIF1_VALUCONTROLCID);  
    sprintf((char *)sprint_buf,"%d.%d%d",g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].tem/100,g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].tem/10%10,g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].tem%10);
    GUI_TEXT_SetText(TEXTVALUESCREENID,TEXT1_VALUCONTROLCID,sprint_buf);
    sprintf((char *)sprint_buf,"%d.%d%d",g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].pre/100,g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].pre/10%10,g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].pre%10);
    GUI_TEXT_SetText(TEXTVALUESCREENID,TEXT2_VALUCONTROLCID,sprint_buf);
    sprintf((char *)sprint_buf,"%d.%d%d",g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].water/100,g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].water/10%10,g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].water%10);
    GUI_TEXT_SetText(TEXTVALUESCREENID,TEXT3_VALUCONTROLCID,sprint_buf);
    GUI_Record_SetEvent(RECORDSCREENID,RECORDCONTROLID,0,0);
    GUI_Record_SetEvent(RECORDSCREENID,RECORDCONTROLID,1,0);
    GUI_Record_SetEvent(RECORDSCREENID,RECORDCONTROLID,2,0);
    GUI_Record_SetEvent(RECORDSCREENID,RECORDCONTROLID,3,0);
    GUI_Record_SetEvent(RECORDSCREENID,RECORDCONTROLID,4,0);
    GUI_Record_SetEvent(RECORDSCREENID,RECORDCONTROLID,5,0);
    if(g_SystemInfo.Equip_Info[s_CBvalue.current_equip_id-1].run_state==1)
    {
        sprintf((char *)sprint_buf,"运行中");
        GUI_TEXT_SetText(TEXTVALUESCREENID,TEXT4_VALUCONTROLCID,sprint_buf);
        GUI_Icon_PlayFrame(TEXTVALUESCREENID,Frame1_VALUCONTROLCID,0);
        GUI_GIF_Stop(TEXTVALUESCREENID,GIF1_VALUCONTROLCID);
        GUI_GIF_Start(TEXTVALUESCREENID,GIF1_VALUCONTROLCID);
    }
    else 
    {
        
        sprintf((char *)sprint_buf,"未启动");
        GUI_TEXT_SetText(TEXTVALUESCREENID,TEXT4_VALUCONTROLCID,sprint_buf);
        GUI_Icon_PlayFrame(TEXTVALUESCREENID,Frame1_VALUCONTROLCID,1);
        GUI_GIF_Stop(TEXTVALUESCREENID,GIF1_VALUCONTROLCID);  
    }
}
/**
* @}
*/

/**
* @}
*/

/**
* @}
*/

