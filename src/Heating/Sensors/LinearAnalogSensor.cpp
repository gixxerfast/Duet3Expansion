/*
 * LinearAnalogSensor.cpp
 *
 *  Created on: 16 Apr 2019
 *      Author: David
 */

#include "LinearAnalogSensor.h"

#if SUPPORT_THERMISTORS

#include "Platform.h"
#include "CanMessageGenericParser.h"

// ADC resolution
// For the theory behind ADC oversampling, see http://www.atmel.com/Images/doc8003.pdf
static constexpr unsigned int AdcOversampleBits = 2;										// we use 2-bit oversampling when using a filtered channel
static constexpr int32_t UnfilteredAdcRange = 1u << AnalogIn::AdcBits;						// The readings we pass in should be in range 0..(AdcRange - 1)
static constexpr int32_t FilteredAdcRange = 1u << (AnalogIn::AdcBits + AdcOversampleBits);	// The readings we pass in should be in range 0..(AdcRange - 1)

LinearAnalogSensor::LinearAnalogSensor(unsigned int sensorNum)
	: SensorWithPort(sensorNum, "Linear analog"), lowTemp(DefaultLowTemp), highTemp(DefaultHighTemp), filtered(true), adcFilterChannel(-1)
{
	CalcDerivedParameters();
}

GCodeResult LinearAnalogSensor::Configure(const CanMessageGenericParser& parser, const StringRef& reply)
{
	bool seen = false;
	if (!ConfigurePort(parser, reply, PinAccess::readAnalog, seen))
	{
		return GCodeResult::error;
	}

	if (parser.GetFloatParam('B', lowTemp))
	{
		seen = true;
	}
	if (parser.GetFloatParam('C', highTemp))
	{
		seen = true;
	}
	if (parser.GetBoolParam('F', filtered))
	{
		seen = true;
	}

	if (seen)
	{
		const bool wasFiltered = filtered;
		CalcDerivedParameters();
		if (adcFilterChannel >= 0)
		{
			Platform::GetAdcFilter(adcFilterChannel)->Init(0);
		}
		else if (wasFiltered)
		{
			reply.copy("filtering not supported on this port");
			return GCodeResult::warning;
		}
	}
	else
	{
		CopyBasicDetails(reply);
		reply.catf(", %sfiltered, range %.1f to %.1f", (filtered) ? "" : "un", (double)lowTemp, (double)highTemp);
	}
	return GCodeResult::ok;
}

void LinearAnalogSensor::Poll()
{
	if (filtered && adcFilterChannel >= 0)
	{
		const volatile ThermistorAveragingFilter * const tempFilter = Platform::GetAdcFilter(adcFilterChannel);
		if (tempFilter->IsValid())
		{
			const int32_t averagedTempReading = tempFilter->GetSum()/(tempFilter->NumAveraged() >> AdcOversampleBits);
			SetResult((averagedTempReading * linearIncreasePerCount) + lowTemp, TemperatureError::success);
		}
		else
		{
			SetResult(TemperatureError::notReady);
		}
	}
	else
	{
		SetResult((port.ReadAnalog() * linearIncreasePerCount) + lowTemp, TemperatureError::success);
	}
}

void LinearAnalogSensor::CalcDerivedParameters()
{
	adcFilterChannel = Platform::GetAveragingFilterIndex(port);
	if (adcFilterChannel < 0)
	{
		filtered = false;
	}
	linearIncreasePerCount = (highTemp - lowTemp)/((filtered) ? FilteredAdcRange : UnfilteredAdcRange);
}

#endif	//SUPPORT_THERMISTORS

// End
