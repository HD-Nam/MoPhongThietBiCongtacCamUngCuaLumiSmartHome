/* Include files */
#include "system_stm32f4xx.h"
#include "timer.h"
#include "eventman.h"
#include "led.h"
#include "melody.h"
#include "lightsensor.h"
#include "temhumsensor.h"
#include "eventbutton.h"
#include "button.h"
#include "Ucglib.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	EVENT_EMPTY,
	EVENT_APP_INIT,
	EVENT_APP_FLUSHMEM_READY,
} event_api_t, *event_api_p;

typedef enum {
	STATE_APP_STARTUP,
	STATE_APP_IDLE,
	STATE_APP_RESET
} state_app_t;

/* Khai báo hàm nguyên mẫu */
static ucg_t ucg;
static state_app_t eCurrentState;
static uint8_t IdTimer = NO_TIMER;
static uint8_t levelLed = 0;
static uint8_t IdTimerCancle;

static void AppStateManager(uint8_t);
static void SetStateApp(state_app_t);
static state_app_t GetStateApp(void);
void LoadConfiguration(void);
void DeviceStateMachine(uint8_t event);
void AppInitCommon(void);

void LedUp(void *data);
void LedDown(void *data);
void cancleTimer(void *data);

void AppInitCommon() {
	SystemCoreClockUpdate();   // Cấu hình clock của hệ thống là 84Mhz
	TimerInit();   // Khởi tạo timer system tick để xử lý các sự kiện thời gian
	EventSchedulerInit(AppStateManager);   // Khởi tạo bộ đệm lưu trữ danh sách sự kiện của chương trình khi chaỵ với tham số là con trỏ hàm
	EventButton_Init();   // Cấu hình chân GPIO của các nút nhấn trên mạch
	BuzzerControl_Init();   // Cấu hình chân GPIO của còi trên mạch
	LedControl_Init();   // Cấu hình chân GPIO của các led RGB trên mạch
	LightSensor_Init(ADC_READ_MODE_DMA);   // Cấu hình ngoại vi ADC hoạt động ở chế độ DMA để đọc dữ liệu của cảm biến ánh sáng trên mạch
	TemHumSensor_Init();   // Cấu hình ngoại vi I2C để giao tiếp với cảm biến nhiệt độ - độ ẩm

	// Setup LCD
	Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
	ucg_ClearScreen(&ucg);
	ucg_SetFont(&ucg, ucg_font_helvR08_tf);
	ucg_SetColor(&ucg, 0, 255, 255, 255);
	ucg_SetColor(&ucg, 1, 0, 0, 0);
	ucg_SetRotate180(&ucg);
}

/**
 * @func	AppStateManager
 * @brief	Manager state application
 * @param	None
 * @retval	None
 */
static void AppStateManager(uint8_t event){
	switch (GetStateApp())
	{
		case STATE_APP_STARTUP:
			if (event == EVENT_APP_INIT){
				LoadConfiguration();
				SetStateApp(STATE_APP_IDLE);
			}
			break;

		case STATE_APP_IDLE:
			DeviceStateMachine(event);
			break;

		case STATE_APP_RESET:
			break;

		default:
			break;
	}
}

static void SetStateApp(state_app_t state)
{
	/* Set state of application */
	eCurrentState = state;
}

static state_app_t GetStateApp(void)
{
	/* Return state of application */
	return eCurrentState;
}


/*Câu 1: In dòng chữ lên LCD
 */
void LoadConfiguration(void){
	ucg_DrawString (&ucg, 12, 12, 0, "IOT");
	ucg_DrawString (&ucg, 12, 24, 0, "Programming by");
	ucg_DrawString (&ucg, 12, 36, 0, "Lumi Smarthome");
}

/* Câu 2: Nhấn nút B3 năm lần khi đó tất cả các Led trên kit mở rộng sẽ nháy năm lần
 * với cường độ sáng là 50% và hiên thị thông tin sau thiết bị lên màn hình LCD
 */
static uint8_t count = 0;   // Biến đếm cho việc nhấp nút
static uint8_t brightness = 0;   // Biến độ sáng của đèn LED

// Hàm thực hiện chớp đèn LED
void blinkLed(void *data)
{
	//Kiểm tra xem đã thực hiện chớp đèn đủ 10 lần chưa
    if (count < 10) {
    	// Đảm bảo rằng độ sáng thay đổi giữa 0 và 50
        if (brightness == 0) {
            brightness = 50;
        } else {
            brightness = 0;
        }

        // Đặt độ sáng cho đèn LED màu xanh
        LedControl_SetAllColor(LED_COLOR_GREEN, brightness);

        // Tăng biến đếm
        count++;
    }
}

// Hàm xử lý trạng thái thiết bị
void DeviceStateMachine(uint8_t event)
{
	switch (event){
		case EVENT_OF_BUTTON_0_PRESS_5_TIMES:
		{
			// Dừng bất kỳ bộ đếm thời gian nào đang chạy
			if(IdTimer != NO_TIMER)
			{
				TimerStop(IdTimer);
				IdTimer = NO_TIMER;
			}

			// Đặt lại biến đếm
			count = 0;

			// Khởi động một bộ đếm thời gian mới (BlinkTimer) với chu kỳ 500ms và 10 lần lặp
			IdTimer = TimerStart("BlinkTimer", 500, 10,(void*) blinkLed, NULL);

			// Xóa màn hình
			ucg_ClearScreen(&ucg);

			// Hiển thị thông tin về thiết bị
			ucg_DrawString(&ucg, 0, 12, 0, "DEVICE: Board STM32");
			ucg_DrawString(&ucg, 0, 24, 0, "Nucleo");
			ucg_DrawString(&ucg, 0, 36, 0, "CODE: ");
			ucg_DrawString(&ucg, 0, 48, 0, "STM32F401RE_NUCLEO");
			ucg_DrawString(&ucg, 0, 60, 0, "MANUFACTURER: ");
			ucg_DrawString(&ucg, 0, 72, 0, "STMicroelectronics");
			ucg_DrawString(&ucg, 0, 84, 0, "KIT EXPANSION: ");
			ucg_DrawString(&ucg, 0, 96, 0, "Lumi Smarthome");
			ucg_DrawString(&ucg, 0, 108, 0, "PROJECT: ");
			ucg_DrawString(&ucg, 0, 120, 0, "Simulator touch switch");
		} break;

/* Câu 3: Nhấn các nút B1, B2, B5, B4 một lần để điều khiển bật/tắt màu tương ứng RED, GREEN, BLUE, WHITE
* của tất cả các led RGB trên kit mở rộng và còi sẽ kêu một bíp một lần
*/
		// Biến đếm cho nút 1
		static uint8_t button1count = 0;

		case EVENT_OF_BUTTON_1_PRESS_LOGIC:
		{
			// Kiểm tra xem nút 1 đã được nhấn bao nhiêu lần
			if(button1count == 0)
			{
				// Bật đèn LED màu đỏ với độ sáng 50
				LedControl_SetAllColor(LED_COLOR_RED, 50);

				// Đặt âm thanh beep
				BuzzerControl_SetMelody(pbeep);

				// Tăng biến đếm cho nút 1 lên 1
				button1count = 1;
			}else
			{
				// Tắt đèn LED màu đỏ
				LedControl_SetAllColor(LED_COLOR_RED, 0);

				// Đặt âm thanh beep
				BuzzerControl_SetMelody(pbeep);

				// Đặt biến đếm cho nút 1 về 0
				button1count = 0;
			}

		} break;

		// Biến đếm cho nút 2
		static uint8_t button2count = 0;
		case EVENT_OF_BUTTON_2_PRESS_LOGIC:
		{
			if(button2count == 0)
			{
				LedControl_SetAllColor(LED_COLOR_GREEN, 50);
				BuzzerControl_SetMelody(pbeep);
				button2count = 1;
			}else{
				LedControl_SetAllColor(LED_COLOR_GREEN, 0);
				BuzzerControl_SetMelody(pbeep);
				button2count = 0;
			}
		} break;

		// Biến đếm cho nút 4
		static uint8_t button4count = 0;
		case EVENT_OF_BUTTON_4_PRESS_LOGIC:
		{
			if(button4count == 0)
			{
				LedControl_SetAllColor(LED_COLOR_WHITE, 50);
				BuzzerControl_SetMelody(pbeep);
				button4count = 1;
			}else{
				LedControl_SetAllColor(LED_COLOR_WHITE, 0);
				BuzzerControl_SetMelody(pbeep);
				button4count = 0;
			}
		} break;

		// Biến đếm cho nút 5
		static uint8_t button5count = 0;
		case EVENT_OF_BUTTON_5_PRESS_LOGIC:
		{
			if(button5count == 0)
			{
				LedControl_SetAllColor(LED_COLOR_BLUE, 50);
				BuzzerControl_SetMelody(pbeep);
				button5count = 1;
			}else{
				LedControl_SetAllColor(LED_COLOR_BLUE, 0);
				BuzzerControl_SetMelody(pbeep);
				button5count = 0;
			}
		} break;

/* Câu 4: Nhấn giữ B1/B5 để điều khiển tăng/giảm cường độ sáng của led RGB:
Nhấn giữ nút B1 với thời gian T lớn hơn một giây để điều khiển tăng độ sáng của led và nhà ra thì dừng điều khiển.
Nhấn giữ nút B5 với thời gian T lớn hơn một giây để điều khiển giảm độ sáng của led và nhà ra thì dừng điều khiển.
 */
		// Xử lý sự kiện khi nút 1 được giữ trong 1s
		case EVENT_OF_BUTTON_1_HOLD_1S:
		{
			// Dừng bất kỳ bộ đếm thời gian nào đang chạy
			if(IdTimer != NO_TIMER)
			{
				TimerStop(IdTimer);
				IdTimer = NO_TIMER;
			}

			// Khởi động một bộ đếm thời gian mới (LedUp) với chu kỳ 50ms và lặp vô hạn
			IdTimer = TimerStart("LedUp", 50, TIMER_REPEAT_FOREVER, (void*)LedUp, NULL);
		} break;

		// Xử lý sự kiện khi nút 5 được giữ trong 1s
		case EVENT_OF_BUTTON_5_HOLD_1S:
		{
			// Dừng bất kỳ bộ đếm thời gian nào đang chạy
			if(IdTimer != NO_TIMER)
			{
				TimerStop(IdTimer);
				IdTimer = NO_TIMER;
			}

			// Khởi động một bộ đếm thời gian mới (LedDown) với chu kỳ 50ms và lặp vô hạn
			IdTimer = TimerStart("LedDown", 50, TIMER_REPEAT_FOREVER, (void*)LedDown, NULL);
		} break;

		// Xử lý sự kiện khi nút 1 được nhả ra sau giữ trong 1s
		case EVENT_OF_BUTTON_1_RELEASED_1S:
		{
			// Dùng bất kỳ bộ đếm thời gian nào đang chạy
			if(IdTimer != NO_TIMER)
			{
				TimerStop(IdTimer);
				IdTimer = NO_TIMER;
			}
		} break;

		// Xử lý sự kiện khi nút 5 được nhả ra sau giữ trong 1s
		case EVENT_OF_BUTTON_5_RELEASED_1S:
		{
			// Dừng bất kỳ bộ đếm thời gian nào đang chạy
			if(IdTimer != NO_TIMER)
			{
				TimerStop(IdTimer);
				IdTimer = NO_TIMER;
			}
		} break;

		default:
			break;
	}
}

/* Câu 5: Cập nhật thông số nhiệt độ, độ ẩm và cường độ ánh sáng của các cảm biến lên màn hình LCD.
 */

// Hàm thực hiện tăng độ sáng đèn LED lên
void LedUp(void *data)
{
	if(levelLed < 100)
	{
		LedControl_SetColorGeneral(LED_KIT_ID0, LED_COLOR_GREEN, levelLed);
		levelLed ++;
	}
}

// Hàm thực hiện giảm độ sáng đèn LED xuống
void LedDown(void *data)
{
	if(levelLed > 0)
	{
		LedControl_SetColorGeneral(LED_KIT_ID0, LED_COLOR_GREEN, levelLed);
		levelLed --;
	}
}

// Biến đệm để lưu trữ dữ liệu cảm biến
static char src1[20];
static char src2[20];
static char src3[20];
static uint8_t count_clear = 0;

// Hàm thực hiện quét và hiển thị dữ liệu từ nhiều cảm biến
void Task_multiSensorScan()
{
	uint8_t temp = TemHumSensor_GetTemp();
	uint8_t humi = TemHumSensor_GetHumi();
	uint16_t light= LightSensor_MeasureUseDMAMode();

	if(count_clear == 0){
	ucg_ClearScreen(&ucg);
	count_clear = 1;
	}

	//hiển thị nhiệt độ
	memset(src1,0,sizeof(src1));
	sprintf(src1,"Temp = %d oC", temp);
	ucg_DrawString(&ucg, 0, 12, 0, src1);


	//hiển thị độ ẩm
	memset(src2,0,sizeof(src2));
	sprintf(src2,"Humi = %3d %%", humi);
	ucg_DrawString(&ucg, 0, 48, 0, src2);


	//hiển thị ánh sáng
	memset(src3,0,sizeof(src3));
	sprintf(src3,"Light = %d lux", light);
	ucg_DrawString(&ucg, 0, 60, 0, src3);


}

// Biến lưu trữ thời gian cập nhật cuối cùng và khoảng thời gian cập nhật
static uint32_t lastUpdateTime;
static uint32_t updateInterval = 2000;

// Hàm hủy bỏ bộ đếm thời gian
void cancleTimer(void *data){
	if(IdTimerCancle != NO_TIMER){
		TimerStop(IdTimerCancle);
	}
}

// Hàm quét và hiển thị dữ liệu từ cảm biến trong khoảng thời gian cố định
void MultiSensorScan()
{
        uint32_t currentTime = GetMilSecTick();

        if (currentTime - lastUpdateTime >= updateInterval) {

            lastUpdateTime = currentTime;
            Task_multiSensorScan();

            // Khởi động một bộ đếm thời gian để hủy bỏ sau 500ms
            IdTimerCancle = TimerStart("delay", 500, 0, (void*)cancleTimer, NULL);
        }

}

int main(void) {
	AppInitCommon();   // Khởi tạo các tài nguyên cần sử dụng
	SetStateApp(STATE_APP_STARTUP);   // Khởi tạo chương trình chính chạy đầu tiên để lấy trạng thái cũ của thiết bị trước khi tắt nguồn
	EventSchedulerAdd(EVENT_APP_INIT);   // Thêm các sự kiện khởi tạo EVENT_APP_INIT vào hàm đợi sự kiện
	lastUpdateTime = GetMilSecTick();   // Lưu trữ thời điểm cuối cùng cập nhật

	while(1){
		processTimerScheduler();   // Xử lý các sự kiện theo thời gian đã cài đặt trước
		processEventScheduler();   // Xử lý các sự kiện khi có trạng thái ấn nút
		MultiSensorScan();   // Quét và hiển thị dữ liệu từ các cảm biến
	}
}
