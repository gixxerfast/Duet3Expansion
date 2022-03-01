/*
 * SAMMYC21.h
 *
 *  Created on: 23 May 2020
 *      Author: David
 */

#ifndef SRC_CONFIG_SAMMYC21_H_
#define SRC_CONFIG_SAMMYC21_H_

#include <Hardware/PinDescription.h>

#define BOARD_TYPE_NAME		"SAMMYC21"
#define BOOTLOADER_NAME		"SAMMYC21"

// General features
#define HAS_VREF_MONITOR		0
#define HAS_VOLTAGE_MONITOR		1
#define HAS_12V_MONITOR			0
#define HAS_CPU_TEMP_SENSOR		1
#define HAS_ADDRESS_SWITCHES	0
#define HAS_BUTTONS				1

// Drivers configuration
#define SUPPORT_DRIVERS			1
#define HAS_SMART_DRIVERS		1
#define HAS_STALL_DETECT		1
#define SINGLE_DRIVER			0
#define SUPPORT_SLOW_DRIVERS	0
#define SUPPORT_DELTA_MOVEMENT	0
#define USE_EVEN_STEPS			0

#define ACTIVE_HIGH_STEP		1		// 1 = active high, 0 = active low
#define ACTIVE_HIGH_DIR			0		// 1 = active high, 0 = active low

#define SUPPORT_TMC51xx			0
#define SUPPORT_TMC2160			0
#define SUPPORT_TMC2660			0
#define SUPPORT_TMC22xx			1


constexpr size_t NumDrivers           = 2;
constexpr size_t MaxSmartDrivers      = NumDrivers;

#define TMC22xx_USES_SERCOM				1
#define TMC22xx_HAS_MUX					0
#define TMC22xx_SINGLE_DRIVER			0
#define TMC22xx_HAS_ENABLE_PINS			0
#define TMC22xx_VARIABLE_NUM_DRIVERS	0
#define TMC22xx_USE_SLAVEADDR			1
#define TMC22xx_DEFAULT_STEALTHCHOP		0

constexpr Pin GlobalTmc22xxEnablePin = PortAPin(9);

constexpr uint8_t TMC22xxSercomNumber = 3;
Sercom * const SERCOM_TMC22xx = SERCOM3;

constexpr Pin TMC22xxSercomTxPin = PortAPin(22);
constexpr GpioPinFunction TMC22xxSercomTxPinPeriphMode = GpioPinFunction::C;
constexpr Pin TMC22xxSercomRxPin = PortAPin(20);
constexpr GpioPinFunction TMC22xxSercomRxPinPeriphMode = GpioPinFunction::D;
constexpr uint8_t TMC22xxSercomRxPad = 2;

constexpr uint32_t DriversBaudRate = 100000;		// Use lower to allow for an inferior design than the 1LC. Old value is 200000;
constexpr uint32_t TransferTimeout = 10;									// any transfer should complete within 10 ticks @ 1ms/tick

constexpr float DriverSenseResistor = 0.110 /*0.091*/ + 0.02; /* + 0.003;*/					// in ohms. Added the 0.003 to make the max current a round 1600mA.
constexpr float DriverVRef = 180.0;											// in mV
constexpr float DriverFullScaleCurrent = DriverVRef/DriverSenseResistor;	// in mA
constexpr float DriverCsMultiplier = 32.0/DriverFullScaleCurrent;
constexpr float MaximumMotorCurrent = 1600.0;
constexpr float MaximumStandstillCurrent = 1200.0;
constexpr uint32_t DefaultStandstillCurrentPercent = 75;

PortGroup * const StepPio = &(PORT->Group[0]);		// the PIO that all the step pins are on
constexpr Pin StepPins[NumDrivers] = { PortAPin(11), PortAPin(13) };
constexpr Pin DirectionPins[NumDrivers] = { PortAPin(10), PortAPin(8) };
constexpr Pin DriverDiagPins[NumDrivers] = { PortAPin(6), PortAPin(7) };


#define SUPPORT_THERMISTORS		1
#define SUPPORT_SPI_SENSORS		0
#define SUPPORT_I2C_SENSORS		0
#define SUPPORT_LIS3DH			0
#define SUPPORT_DHT_SENSOR		0
#define SUPPORT_SDADC			0

#define USE_MPU					0
#define USE_CACHE				0

#define DIAG_SERCOM_NUMBER		5		// which SERCOM device we use for debugging output

constexpr bool UseAlternateCanPins = true;

constexpr size_t MaxPortsPerHeater = 1;

constexpr size_t NumThermistorInputs = 1;
constexpr float DefaultThermistorSeriesR = 2200.0;

constexpr Pin TempSensePins[NumThermistorInputs] = {PortBPin(8)};

constexpr Pin CanStandbyPin = PortAPin(27);

constexpr Pin ButtonPins[] = { PortBPin(9) };

constexpr Pin VinMonitorPin = PortAPin(3);
constexpr float VinDividerRatio = (47 /*60.4*/ + 4.7)/4.7;
constexpr float VinMonitorVoltageRange = VinDividerRatio * 3.3;				// we use the 5V supply as the voltage reference

// Diagnostic LEDs
constexpr Pin LedPins[] = { PortAPin(28) };
constexpr bool LedActiveHigh = true;

#if SUPPORT_SPI_SENSORS

// Shared SPI using pins PA16,17,18. If changing this, also change the available pins in the pin table.
constexpr uint8_t SspiSercomNumber = 1;
constexpr uint32_t SspiDataInPad = 2;
constexpr Pin SSPIMosiPin = PortAPin(16);
constexpr GpioPinFunction SSPIMosiPinPeriphMode = GpioPinFunction::C;
constexpr Pin SSPISclkPin = PortAPin(17);
constexpr GpioPinFunction SSPISclkPinPeriphMode = GpioPinFunction::C;
constexpr Pin SSPIMisoPin = PortAPin(18);
constexpr GpioPinFunction SSPIMisoPinPeriphMode = GpioPinFunction::C;

#endif

#if SUPPORT_I2C_SENSORS

// I2C using pins PA22,23. If changing this, also change the available pins in the pin table.
constexpr uint8_t I2CSercomNumber = 3;
constexpr Pin I2CSDAPin = PortAPin(22);
constexpr GpioPinFunction I2CSDAPinPeriphModes[NumDrivers] = {GpioPinFunction::C, GpioPinFunction::C};
constexpr Pin I2CSCLPin = PortAPin(23);
constexpr GpioPinFunction I2CSCLPinPeriphModes[NumDrivers] = {GpioPinFunction::C, GpioPinFunction::C};
#define I2C_HANDLER		SERCOM3_Handler

#endif

#if SUPPORT_LIS3DH
constexpr Pin Lis3dhInt1Pin = PortAPin(13);
#endif

// Table of pin functions that we are allowed to use
constexpr PinDescription PinTable[] =
{
	//	TC					TCC					ADC					SDADC				SERCOM in			SERCOM out	  Exint PinName
	// Port A
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA00 not on board
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA01 not on board
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		2,	"io2.in"		},	// PA02
	{ TcOutput::none,	TccOutput::none,	AdcInput::adc0_1,	AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"ate.vin"		},	// PA03 VinMon
	{ TcOutput::none,	TccOutput::tcc0_0E,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		4,	"io0.out"		},	// PA04 Servo
	{ TcOutput::none,	TccOutput::none,	AdcInput::adc0_5,	AdcInput::none,		SercomIo::none,		SercomIo::none,		5,	"io3.in"		},	// PA05
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		6,	"ate.d0.diag"	},	// PA06 driver 0 DIAG
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		7,	"ate.d1.diag"	},	// PA07 driver 1 DIAG
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"ate.d1.dir"	},	// PA08 driver 1 DIR
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA09 driver 0 & 1 EN
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"ate.d0.dir"	},	// PA10 driver 0 DIR
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"ate.d0.step"	},	// PA11 driver 0 STEP
	{ TcOutput::none,	TccOutput::tcc2_0E,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		12,	"pa12"	 		},	// PA12
#if SUPPORT_LIS3DH
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		13,	nullptr			},	// PA13 accelerometer INT1
#else
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"ate.d1.step"	},	// PA13 driver 1 STEP
#endif
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA14 crystal
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA15 crystal
#if SUPPORT_SPI_SENSORS
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::sercom1c,	Nx,	nullptr			},	// PA16 SPI
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::sercom1c,	SercomIo::none,		Nx,	nullptr			},	// PA17 SPI
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr	 		},	// PA18 SPI
#else
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::sercom1c,	0,	"pa16"			},	// PA16
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::sercom1c,	SercomIo::none,		1,	"pa17"			},	// PA17
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"pa18"	 		},	// PA18
#endif
	{ TcOutput::tc4_1,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		3,	"io1.in"		},	// PA19
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA20 USART RX
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"pa21"			},	// PA21
#if SUPPORT_I2C_SENSORS
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA22 I2C
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA23 I2C
#else
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr			},	// PA22 USART TX
	{ TcOutput::tc0_1,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"pa23"			},	// PA23
#endif
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"pa24"		},	// PA24
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	"pa25"		},	// PA25
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PA26 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PA27 CAN transceiver standby
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PA28 LED 0
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PA29 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PA30 swclk
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PA31 swdio

	// Port B
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB00 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB01 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB02 USB interface (SERCOM5 pad 0)
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB03 USB interface (SERCOM5 pad 1)
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB04 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB05 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB06 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		Nx,	nullptr		},	// PB07 not on chip
	{ TcOutput::none,	TccOutput::none,	AdcInput::adc0_2,	AdcInput::none,		SercomIo::none,		SercomIo::none,		8,	nullptr		},	// PB08
	{ TcOutput::none,	TccOutput::none,	AdcInput::none,		AdcInput::none,		SercomIo::none,		SercomIo::none,		9,	"!^button0"	},	// PB09 button recognised by bootloader
	// PB22/23 are used for CAN0, PB10/11 for CAN1
};

static constexpr size_t NumPins = ARRAY_SIZE(PinTable);
static_assert(NumPins == 32 + 10);		// 32 pins on port A (some missing), only PB08 and PB09 are brought out on this board

// Timer/counter used to generate step pulses and other sub-millisecond timings
TcCount32 * const StepTc = &(TC2->COUNT32);
constexpr IRQn StepTcIRQn = TC2_IRQn;
constexpr unsigned int StepTcClockId = TC2_GCLK_ID;
constexpr unsigned int StepTcNumber = 2;
#define STEP_TC_HANDLER		TC2_Handler

// Available UART ports
#define NUM_SERIAL_PORTS		1
constexpr IRQn Serial0_IRQn = SERCOM4_IRQn;

// DMA channel assignments
constexpr DmaChannel DmacChanTmcTx = 0;
constexpr DmaChannel DmacChanTmcRx = 1;
constexpr DmaChannel DmacChanAdc0Rx = 2;
constexpr DmaChannel DmacChanSdadcRx = 3;

constexpr unsigned int NumDmaChannelsUsed = 4;			// must be at least the number of channels used, may be larger. Max 12 on the SAMC21.

constexpr DmaPriority DmacPrioTmcTx = 0;
constexpr DmaPriority DmacPrioTmcRx = 3;
constexpr DmaPriority DmacPrioAdcRx = 2;

// Interrupt priorities, lower means higher priority. 0 can't make RTOS calls.
const NvicPriority NvicPriorityStep = 1;				// step interrupt is next highest, it can preempt most other interrupts
const NvicPriority NvicPriorityUart = 2;				// serial driver makes RTOS calls
const NvicPriority NvicPriorityPins = 2;				// priority for GPIO pin interrupts
const NvicPriority NvicPriorityCan = 3;
const NvicPriority NvicPriorityDmac = 3;				// priority for DMA complete interrupts
const NvicPriority NvicPriorityI2C = 4;

#endif /* SRC_CONFIG_SAMMYC21_H_ */
