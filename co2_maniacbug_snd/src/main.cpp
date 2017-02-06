/**
	Whoever continues the development of this
*/

#include "FreeRTOS_AVR.h"	//
#include <Arduino.h>	//
#include "basic_io_avr.h"	//
#include <cozir.h>
#include <stdio.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <utility/task.h>
#include <RF24Network.h>
#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <EEPROM.h>
#include "printf.h"

/**
	Alamat node yang menjadi sink. Library routing RF24Network mendefinisikan sink
	memiliki address 00
*/
#define SINK_ADDR 00
/**
	Hop maksimal yang dapat ditempuh paket sebelum di-drop dalam hop.
	Penggunaaan maximum hop disebabkan oleh algoritma routing yang belum efisien
	TODO: buat implementasi algoritma mesh networking (AODV atau semacamnya) untuk
	Arduino
*/
#define MAX_HOP_CNT 10

/**
	Handle untuk tiap-tiap task
*/
TaskHandle_t temp, co2, hum, light, print, route;

/**
	Global object untuk sensor CO2
*/
SoftwareSerial sws(13,12);
COZIR czr(&sws);
/**
	Global object untuk perangkat radio nRF
*/
RF24 radio(48, 46);
/**
	Global object untuk network radio
*/
RF24Network network(radio);
/**
	Alamat dari node ini
*/
uint16_t addr = 0;

/**
	Tipe data yang digunakan untuk menampung bacaan dari sensor temperatur.
	ucStatus digunakan untuk menyimpan informasi apakah data sudah diperbarui
	oleh sensor
*/
typedef struct {
	float usVal;
	bool ucStatus;
} resultF;

/**
	Tipe data yang digunakan untuk menampung bacaan dari sensor CO2.
	ucStatus digunakan untuk menyimpan informasi apakah data sudah diperbarui
	oleh sensor
*/
typedef struct {
	uint32_t ulCO2Val;
	bool ucStatus;
} resultCO2;

/**
	Global variable untuk menampung bacaan dari sensor
*/
static volatile resultF resTemp = {0.0, false};
static volatile resultCO2 resCO2 = {0, false};
static volatile resultF resHum = {0.0, false};
static volatile uint16_t resLight = 0;
static volatile time_t time = 0;

/**
	Semafor untuk masing-masing variabel global.
	TODO: Cari dan gunakan implementasi mutex untuk RTOS pada Arduino
	TODO: untuk saat ini sistem ini belum menggunakan mutex
*/
// SemaphoreHandle_t sem_temp, sem_co2, sem_hum, sem_time;

/**
	Task untuk melakukan pembacaan sensor temperatur
*/
static void vTempReader ( void *pvParameter){
	for( ;; ){
		vTaskSuspendAll();
		// coba ambil semafor dari temperatur
		// if(xSemaphoreTake(sem_temp, (TickType_t)0) == pdTRUE)
		// {
			if(resTemp.ucStatus == false){
				float cel = czr.Celsius();
				resTemp.usVal = cel;
				resTemp.ucStatus = true;
			}
		// 	xSemaphoreGive(sem_temp);
		// }
		xTaskResumeAll();
		vTaskDelay((TickType_t) 100*portTICK_PERIOD_MS);
	}
}

/**
	Task untuk melakukan pembacaan sensor CO2
*/
static void vCO2Reader ( void *pvParameter){
	for( ;; ){
		vTaskSuspendAll();
		// if(xSemaphoreTake(sem_co2, (TickType_t)0) == pdTRUE)
		// {
			if(resCO2.ucStatus == false){
				unsigned long co2_v = czr.CO2();
				resCO2.ulCO2Val = co2_v;
				resCO2.ucStatus = true;
			}
		// 	xSemaphoreGive(sem_co2);
		// }
		xTaskResumeAll();
		vTaskDelay((TickType_t) 250*portTICK_PERIOD_MS);
		}
}

/**
	Task untuk melakukan pembacaan sensor kelembapan
*/
static void vHumidityReader ( void *pvParameter){
  for( ;; ){
    vTaskSuspendAll();
		// if(xSemaphoreTake(sem_hum, (TickType_t)0))
    // {
      if(resHum.ucStatus == false){
        float cel = czr.Humidity();
        resHum.usVal = cel;
        resHum.ucStatus = true;
      // }
			// xSemaphoreGive(sem_hum);
    }
    xTaskResumeAll();
		vTaskDelay((TickType_t) 100*portTICK_PERIOD_MS);
  }
}

/**
	This function is a task to print to serial
	DEPRECATED: fungsi ini tidak lagi dijalankan sebagai task
	tapi dipanggil oleh vTaskRoute(). task routing disatukan dengan task print
*/
static void vPrintTask ( void *pvParameter ){
	// for( ;; ){
	// 	vTaskSuspendAll();
		// if( xSemaphoreTake(sem_temp,(TickType_t)0) == pdTRUE &&
		// 		xSemaphoreTake(sem_co2,(TickType_t)0) == pdTRUE &&
		// 		xSemaphoreTake(sem_hum,(TickType_t)0) == pdTRUE &&
		// 		xSemaphoreTake(sem_time,(TickType_t)0) == pdTRUE
		// 	)
		// {
			if(resCO2.ucStatus && resTemp.ucStatus && resHum.ucStatus){
				Serial.print(addr); Serial.print(F(","));
				Serial.print(time);
				Serial.print(F(","));
				Serial.print(resCO2.ulCO2Val);
				Serial.print(F(","));
				Serial.print(resTemp.usVal);
				Serial.print(F(","));
				Serial.print(resHum.usVal);
				Serial.print(F(","));
				Serial.print(resLight);
				Serial.println(F(","));

				resTemp.ucStatus = false;
				resCO2.ucStatus = false;
				resHum.ucStatus = false;
			}
		// 	xSemaphoreGive(sem_temp);
		// 	xSemaphoreGive(sem_hum);
		// 	xSemaphoreGive(sem_co2);
		// 	xSemaphoreGive(sem_time);
		// }
	// 	xTaskResumeAll();
	// 	vTaskDelay((TickType_t) 100*portTICK_PERIOD_MS);
	// }
}

/**
	Task untuk melakukan pembacaan sensor cahaya
 	Pada task ini dilakukan sampling dari bacaan sensor 5 kali dalam 1 detik
*/
static void vLightReader ( void *pvParameter){
	while(1)
	{
		vTaskSuspendAll();
		uint16_t cur_light = analogRead(0);
		resLight = (cur_light*50 + resLight*50)/100;
		xTaskResumeAll();
		vTaskDelay((TickType_t) 500*portTICK_PERIOD_MS);
	}
}

/**
	Paket data yang akan dikirimkan melalui radio
*/
typedef struct
{
  uint64_t timestamp;
  float temperature;
  uint32_t co2;
  float humidity;
  uint16_t light;
	uint16_t hop_cnt;
} payload_t;

/**
	Task untuk melakukan routing dan mengirimkan data
*/
static void vTaskRoute(void* pvParameter)
{
	while(1)
	{
		network.update();
		vTaskSuspendAll();
		payload_t payload = {0, resTemp.usVal, resCO2.ulCO2Val, resHum.usVal, resLight, 0};
		xTaskResumeAll();

		if(network.available())
		{
			payload_t payload;
			RF24NetworkHeader header;
			bool ok = network.read(header, &payload, sizeof(payload));
			if(header.to_node != addr && payload.hop_cnt > 0)
			{
				// kurangi 1 hop
				payload.hop_cnt--;
				// rebroadcast packet
				ok = network.write(header, &payload, sizeof(payload));
			}
		}

		// proses routing selesai, lakukan pengiriman data sendiri
		// if(xSemaphoreTake(sem_time,(TickType_t)0) == pdTRUE)
		// {
			time = RTC.get();
			// printf("update rtc");
		// 	xSemaphoreGive(sem_time);
		// }
		payload.timestamp = (uint32_t) time;
		// set header
		RF24NetworkHeader header;
		header.to_node = SINK_ADDR;
		// printf("packet destination: %d\n\r", header.to_node);
		bool ok = network.write(header, &payload, sizeof(payload));
		vPrintTask(NULL);
		vTaskDelay((TickType_t)1000*portTICK_PERIOD_MS);
	}
}


void setup(void)
{
	EEPROM.begin();
	// ambil alamat node
	addr = (EEPROM.read(0x0ffe) << 8) | EEPROM.read(0x0fff);

	// inisialisasi semua sensor
	sws.begin(9600);
	Serial.begin(57600);
	printf_begin();
	printf("RF24Network/examples/helloworld_tx/\n\r");

	SPI.begin();
	radio.begin();
	network.begin(/*channel*/ 90, /*node address*/ addr);

	radio.setPALevel(RF24_PA_MIN);

	// radio initialization
	radio.printDetails();

	// initialize semaphore
	// if((sem_temp = xSemaphoreCreateBinary()) == NULL)
	// 	printf("Temp semaphore failed to initialize\n\r");
	// if ((sem_co2 = xSemaphoreCreateBinary()) == NULL)
	// 	printf("CO2 semaphore failed to initialize\n\r");
	// if ((sem_hum = xSemaphoreCreateBinary()) == NULL)
	// 	printf("Humidity semaphore failed to initialize\n\r");
	// if ((sem_time = xSemaphoreCreateBinary()) == NULL)
	// 	printf("Time semaphore failed to initialize\n\r");

	printf("Node address: %d\n\r", addr);

	// node_type = NODE_SINK;

	// load task-task ke memori
	xTaskCreate( vTempReader, "TempReader", configMINIMAL_STACK_SIZE + 250, NULL, 1, &temp);
	xTaskCreate( vCO2Reader, "CO2Reader", 500, NULL, 1, &co2);
	xTaskCreate( vHumidityReader, "HumidityReader", configMINIMAL_STACK_SIZE + 100, NULL, 1, &hum);
	xTaskCreate( vLightReader, "LightReader", 200, NULL, 2, &light);
	// xTaskCreate( vPrintTask, "PrintTask", 400, NULL, 1, &print);
	xTaskCreate( vTaskRoute, "RoutingTask", 400, NULL, 1, &route);
	// xTaskCreate( vTaskSvc, "ServiceTask", configMINIMAL_STACK_SIZE, NULL, 1, &svc);

	/*
	* UNCOMMENT TO VIEW AMOUNT OF FREE HEAP LEFT
	*/
	printf("Free Heap: %d\n\r", freeHeap());

	// jalankan task-task
	printf("STARTING SCHEDULER\n\r");
	vTaskStartScheduler();
	//Serial.print(czr.ReadAutoCalibration());

	//czr.CalibrateFreshAir();
	//czr.CalibrateKnownGas(1416);
	while(1)
		;
}

void loop(void) {}
// vim:ai:cin:sts=2 sw=2 ft=cpp
