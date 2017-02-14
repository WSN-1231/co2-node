
/**
	Whoever continues the development of this
*/

// this node address
#define NODE_ADDR 0x0

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

TaskHandle_t temp, co2, hum, light, print, route;

SoftwareSerial sws(13,12);
COZIR czr(&sws);
RF24 radio(48, 46);
RF24Network network(radio);

// Address node ini
uint16_t addr = 0;

typedef struct {
	float usVal;
	bool ucStatus;
} resultF;

typedef struct {
	uint32_t ulCO2Val;
	bool ucStatus;
} resultCO2;

static volatile resultF resTemp = {0.0, false};
static volatile resultCO2 resCO2 = {0, false};
static volatile resultF resHum = {0.0, false};
static volatile uint16_t resLight = 0;
static volatile time_t time = 0;

// SemaphoreHandle_t sem_temp, sem_co2, sem_hum, sem_time;

static void vTempReader ( void *pvParameter){
	for( ;; ){
		// printf("temp\n\r");
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

static void vCO2Reader ( void *pvParameter){
	for( ;; ){
		// printf("co2\n\r");
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

static void vHumidityReader ( void *pvParameter){
  for( ;; ){
		// printf("hum\n\r");
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
		vTaskDelay((TickType_t) random(10)+100*portTICK_PERIOD_MS);
  }
}

static void vLightReader ( void *pvParameter){
	while(1)
	{
		vTaskSuspendAll();
		uint16_t cur_light = analogRead(0);
		resLight = (cur_light*50 + resLight*50)/100;
		xTaskResumeAll();
		vTaskDelay((TickType_t) random(10)+100*portTICK_PERIOD_MS);
	}
}

/**
	This function is a task to print to serial
*/

static void vPrintTask ( void *pvParameter ){
	for( ;; ){
		// printf("print\n\r");
		time = RTC.get();
		vTaskSuspendAll();
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
		xTaskResumeAll();
		vTaskDelay((TickType_t) 1000*portTICK_PERIOD_MS);
	}
}


// When did we last send?
unsigned long last_sent;

// How many have we sent already
unsigned long packets_sent;

// Structure of our payload
typedef struct
{
  uint64_t timestamp;
  float temperature;
  uint32_t co2;
  float humidity;
  uint16_t light;
	uint16_t hop_cnt;
} payload_t;
// typedef struct
// {
// 	uint32_t time;
// 	uint32_t counter;
// } payload_t;

static void vTaskRoute(void* pvParameter)
{
	while(1)
	{
		// printf("route\n\r");
		vTaskSuspendAll();
		// pump network
		network.update();

		if(network.available())
		{
			payload_t payload;
			RF24NetworkHeader header;
			bool ok = network.read(header, &payload, sizeof(payload));
			Serial.print(header.from_node); Serial.print(F(","));
			Serial.print((uint32_t) payload.timestamp); Serial.print(F(","));
			Serial.print(payload.co2); Serial.print(F(","));
			Serial.print(payload.temperature); Serial.print(F(","));
			Serial.print(payload.humidity); Serial.print(F(","));
			Serial.print(payload.light); Serial.println(",");
		}
		xTaskResumeAll();
		vTaskDelay((TickType_t) 100*portTICK_PERIOD_MS);
	}
}

void setup(void)
{
	EEPROM.begin();
	// ambil alamat node
	// addr = (EEPROM.read(0xfffe) << 8) | EEPROM.read(0xffff);
	addr = NODE_ADDR;

	sws.begin(9600);
	printf_begin();
	Serial.begin(57600);
	// printf("RF24Network/examples/helloworld_tx/");

	SPI.begin();
	radio.begin();
	network.begin(/*channel*/ 90, /*node address*/ addr);

	// TODO: set power level radio sesuai kebutuhan
	radio.setPALevel(RF24_PA_LOW);

	// initialize semaphore
	// TODO: enable mutex usage
	// if((sem_temp = xSemaphoreCreateBinary()) == NULL)
	// 	printf("Temp semaphore failed to initialize\n\r");
	// if ((sem_co2 = xSemaphoreCreateBinary()) == NULL)
	// 	printf("CO2 semaphore failed to initialize\n\r");
	// if ((sem_hum = xSemaphoreCreateBinary()) == NULL)
	// 	printf("Humidity semaphore failed to initialize\n\r");
	// if ((sem_time = xSemaphoreCreateBinary()) == NULL)
	// 	printf("Time semaphore failed to initialize\n\r");

	// printf("Node address: %d\n\r", addr);

	// node_type = NODE_SINK;

	xTaskCreate( vTempReader, "TempReader", configMINIMAL_STACK_SIZE + 250, NULL, 1, &temp);
	xTaskCreate( vCO2Reader, "CO2Reader", 500, NULL, 1, &co2);
	xTaskCreate( vHumidityReader, "HumidityReader", configMINIMAL_STACK_SIZE + 100, NULL, 1, &hum);
	xTaskCreate( vLightReader, "LightReader", 200, NULL, 2, &light);
	xTaskCreate( vPrintTask, "PrintTask", 400, NULL, 1, &print);
	xTaskCreate( vTaskRoute, "RoutingTask", 400, NULL, 1, &route);
	// xTaskCreate( vTaskSvc, "ServiceTask", configMINIMAL_STACK_SIZE, NULL, 1, &svc);

	/*
	* UNCOMMENT TO VIEW AMOUNT OF FREE HEAP LEFT
	*/
	// printf("Free Heap: %d\n\r", freeHeap());
	//
	// printf("STARTING SCHEDULER\n\r");
	vTaskStartScheduler();
	//Serial.print(czr.ReadAutoCalibration());

	//czr.CalibrateFreshAir();
	//czr.CalibrateKnownGas(1416);
	while(1)
		;
}

void loop(void) {}
// vim:ai:cin:sts=2 sw=2 ft=cpp
