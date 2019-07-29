/*
 * SharedSpiDevice.cpp
 *
 *  Created on: 28 Jul 2019
 *      Author: David
 */

#include "SharedSpiDevice.h"

#if SUPPORT_SPI_SENSORS

#include "IoPorts.h"
#include "DmacManager.h"
#include "peripheral_clk_config.h"

constexpr uint32_t DefaultSharedSpiClockFrequency = 2000000;
constexpr uint32_t SpiTimeout = 10000;

static void InitSpi()
{
	// Temporary fixed pin assignment
	gpio_set_pin_function(PortCPin(16), PINMUX_PC16C_SERCOM6_PAD0);		// MOSI
	gpio_set_pin_function(PortCPin(17), PINMUX_PC17C_SERCOM6_PAD1);		// SCLK
	gpio_set_pin_function(PortCPin(19), PINMUX_PC19C_SERCOM6_PAD3);		// MISO

	// Enable the clock
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM0_GCLK_ID_CORE, CONF_GCLK_SERCOM0_CORE_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM0_GCLK_ID_SLOW, CONF_GCLK_SERCOM0_SLOW_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_mclk_set_APBAMASK_SERCOM0_bit(MCLK);

	// Set up the SERCOM
	const uint32_t regCtrlA = SERCOM_SPI_CTRLA_MODE(3) | SERCOM_SPI_CTRLA_DIPO(3) | SERCOM_SPI_CTRLA_DOPO(0) | SERCOM_SPI_CTRLA_FORM(0)
							| SERCOM_SPI_CTRLA_CPOL | SERCOM_SPI_CTRLA_CPHA;
	const uint32_t regCtrlB = 0;											// 8 bits, slave select disabled, receiver disabled for now
	const uint32_t regCtrlC = 0;											// not 32-bit mode

	if (!hri_sercomusart_is_syncing(SERCOM_SSPI, SERCOM_USART_SYNCBUSY_SWRST))
	{
		uint32_t mode = regCtrlA & SERCOM_USART_CTRLA_MODE_Msk;
		if (hri_sercomusart_get_CTRLA_reg(SERCOM_SSPI, SERCOM_USART_CTRLA_ENABLE))
		{
			hri_sercomusart_clear_CTRLA_ENABLE_bit(SERCOM_SSPI);
			hri_sercomusart_wait_for_sync(SERCOM_SSPI, SERCOM_USART_SYNCBUSY_ENABLE);
		}
		hri_sercomusart_write_CTRLA_reg(SERCOM_SSPI, SERCOM_USART_CTRLA_SWRST | mode);
	}
	hri_sercomusart_wait_for_sync(SERCOM_SSPI, SERCOM_USART_SYNCBUSY_SWRST);

	hri_sercomusart_write_CTRLA_reg(SERCOM_SSPI, regCtrlA);
	hri_sercomusart_write_CTRLB_reg(SERCOM_SSPI, regCtrlB);
	hri_sercomusart_write_CTRLC_reg(SERCOM_SSPI, regCtrlC);
	hri_sercomusart_write_BAUD_reg(SERCOM_SSPI, SERCOM_SPI_BAUD_BAUD(CONF_GCLK_SERCOM0_CORE_FREQUENCY/(2 * DefaultSharedSpiClockFrequency) - 1));
	hri_sercomusart_write_DBGCTRL_reg(SERCOM_SSPI, SERCOM_I2CM_DBGCTRL_DBGSTOP);			// baud rate generator is stopped when CPU halted by debugger

#if 0	// if using DMA
	// Set up the DMA descriptors
	// We use separate write-back descriptors, so we only need to set this up once, but it must be in SRAM
	DmacSetBtctrl(SspiRxDmaChannel, DMAC_BTCTRL_VALID | DMAC_BTCTRL_EVOSEL_DISABLE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_BEATSIZE_BYTE
								| DMAC_BTCTRL_DSTINC | DMAC_BTCTRL_STEPSEL_DST | DMAC_BTCTRL_STEPSIZE_X1);
	DmacSetSourceAddress(SspiRxDmaChannel, &(SERCOM_SSPI->SPI.DATA.reg));
	DmacSetDestinationAddress(SspiRxDmaChannel, rcvData);
	DmacSetDataLength(SspiRxDmaChannel, ARRAY_SIZE(rcvData));

	DmacSetBtctrl(SspiTxDmaChannel, DMAC_BTCTRL_VALID | DMAC_BTCTRL_EVOSEL_DISABLE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_BEATSIZE_BYTE
								| DMAC_BTCTRL_SRCINC | DMAC_BTCTRL_STEPSEL_SRC | DMAC_BTCTRL_STEPSIZE_X1);
	DmacSetSourceAddress(SspiTxDmaChannel, sendData);
	DmacSetDestinationAddress(SspiTxDmaChannel, &(SERCOM_SSPI->SPI.DATA.reg));
	DmacSetDataLength(SspiTxDmaChannel, ARRAY_SIZE(sendData));

	DmacSetInterruptCallbacks(SspiRxDmaChannel, RxDmaCompleteCallback, nullptr, 0U);
#endif

	SERCOM_SSPI->SPI.CTRLA.bit.ENABLE = 1;		// keep the SPI enabled all the time so that the SPCLK line is driven
}

static inline void DisableSpi()
{
	// Don't disable the whole SPI between transmissions because that causes the clock output to go high impedance
	SERCOM_SSPI->SPI.CTRLB.bit.RXEN = 0;
}

static inline void EnableSpi()
{
	SERCOM_SSPI->SPI.CTRLB.bit.RXEN = 1;
	hri_sercomusart_wait_for_sync(SERCOM_SSPI, SERCOM_USART_SYNCBUSY_CTRLB);
}

// Wait for transmitter ready returning true if timed out
static inline bool waitForTxReady()
{
	uint32_t timeout = SpiTimeout;
	while (!(SERCOM_SSPI->SPI.INTFLAG.bit.DRE))
	{
		if (--timeout == 0)
		{
			return true;
		}
	}
	return false;
}

// Wait for transmitter empty returning true if timed out
static inline bool waitForTxEmpty()
{
	uint32_t timeout = SpiTimeout;
	while (!(SERCOM_SSPI->SPI.INTFLAG.bit.TXC))
	{
		if (!timeout--)
		{
			return true;
		}
	}
	return false;
}

// Wait for receive data available returning true if timed out
static inline bool waitForRxReady()
{
	uint32_t timeout = SpiTimeout;
	while (!(SERCOM_SSPI->SPI.INTFLAG.bit.RXC))
	{
		if (--timeout == 0)
		{
			return true;
		}
	}
	return false;
}

// SharedSpiDevice class members
SharedSpiDevice::SharedSpiDevice(uint32_t clockFreq, SpiMode m, bool polarity)
	: clockFrequency(clockFreq), csPin(NoPin), mode(m), csActivePolarity(polarity)
{
}

void SharedSpiDevice::InitMaster()
{
	static bool commsInitDone = false;

	IoPort::SetPinMode(csPin, (csActivePolarity) ? OUTPUT_LOW : OUTPUT_HIGH);

	if (!commsInitDone)
	{
		InitSpi();
		commsInitDone = true;
	}
}

void SharedSpiDevice::SetupMaster() const
{
	DisableSpi();
	hri_sercomusart_write_BAUD_reg(SERCOM_SSPI, SERCOM_SPI_BAUD_BAUD(CONF_GCLK_SERCOM0_CORE_FREQUENCY/(2 * clockFrequency) - 1));

	uint32_t regCtrlA = SERCOM_SPI_CTRLA_MODE(3) | SERCOM_SPI_CTRLA_DIPO(3) | SERCOM_SPI_CTRLA_DOPO(0) | SERCOM_SPI_CTRLA_FORM(0)
							| SERCOM_SPI_CTRLA_CPOL | SERCOM_SPI_CTRLA_CPHA;
	if (((uint8_t)mode & 2) != 0)
	{
		regCtrlA |= SERCOM_SPI_CTRLA_CPOL;
	}
	if (((int8_t)mode & 1) == 0)							// the bit is called CPHA but is actually NPCHA
	{
		regCtrlA |= SERCOM_SPI_CTRLA_CPHA;
	}
	hri_sercomusart_write_CTRLA_reg(SERCOM_SSPI, regCtrlA);
	EnableSpi();
}

void SharedSpiDevice::Select() const
{
	IoPort::WriteDigital(csPin, csActivePolarity);
}

void SharedSpiDevice::Deselect() const
{
	IoPort::WriteDigital(csPin, !csActivePolarity);
}

bool SharedSpiDevice::TransceivePacket(const uint8_t* tx_data, uint8_t* rx_data, size_t len) const
{
	for (uint32_t i = 0; i < len; ++i)
	{
		uint32_t dOut = (tx_data == nullptr) ? 0x000000FF : (uint32_t)*tx_data++;
		if (waitForTxReady())
		{
			return false;
		}

		// Write to transmit register
		SERCOM_SSPI->SPI.DATA.reg = dOut;

		// Some devices are transmit-only e.g. 12864 display, so don't wait for received data if we don't need to
		if (rx_data != nullptr)
		{
			// Wait for receive register
			if (waitForRxReady())
			{
				return false;
			}

			// Get data from receive register
			const uint8_t dIn = (uint8_t)SERCOM_SSPI->SPI.DATA.reg;
			*rx_data++ = dIn;
		}
	}

	// If we didn't wait to receive, then we need to wait for transmit to finish and clear the receive buffer
	if (rx_data == nullptr)
	{
		waitForTxEmpty();
		(void)SERCOM_SSPI->SPI.DATA.reg;
	}

	return true;	// success
}

#endif

// End
