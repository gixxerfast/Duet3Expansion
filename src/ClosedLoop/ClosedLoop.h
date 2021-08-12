/*
 * ClosedLoop.h
 *
 *  Created on: 9 Jun 2020
 *      Author: David
 */

#ifndef SRC_CLOSEDLOOP_CLOSEDLOOP_H_
#define SRC_CLOSEDLOOP_CLOSEDLOOP_H_

#include <RepRapFirmware.h>

#if SUPPORT_CLOSED_LOOP

#include <GCodes/GCodeResult.h>
#include <Hardware/SharedSpiDevice.h>
#include <CanMessageFormats.h>
#include <General/NamedEnum.h>
#include <ClosedLoop/Trigonometry.h>

class SpiEncoder;

namespace ClosedLoop
{
	// Possible tuning errors
	const uint8_t TUNE_ERR_NOT_FOUND_POLARITY			= 1 << 0;
	const uint8_t TUNE_ERR_NOT_ZEROED					= 1 << 1;
	const uint8_t TUNE_ERR_NOT_CHECKED_POLARITY			= 1 << 2;
	const uint8_t TUNE_ERR_NOT_CHECKED_CONTROL			= 1 << 3;
	const uint8_t TUNE_ERR_NOT_CHECKED_ENCODER_STEPS 	= 1 << 4;
	const uint8_t TUNE_ERR_INCORRECT_POLARITY 			= 1 << 5;
	const uint8_t TUNE_ERR_CONTROL_FAILED 				= 1 << 6;
	const uint8_t TUNE_ERR_SYSTEM_ERROR					= 1 << 7;
	const uint8_t TUNE_ERR_NOT_PERFORMED_MINIMAL_TUNE = TUNE_ERR_NOT_FOUND_POLARITY | TUNE_ERR_NOT_ZEROED | TUNE_ERR_NOT_CHECKED_POLARITY | TUNE_ERR_NOT_CHECKED_CONTROL | TUNE_ERR_NOT_CHECKED_ENCODER_STEPS;
	const uint8_t TUNE_ERR_TUNING_FAILURE = TUNE_ERR_INCORRECT_POLARITY | TUNE_ERR_CONTROL_FAILED | TUNE_ERR_SYSTEM_ERROR;

	// Tuning manoeuvres
	const uint8_t POLARITY_DETECTION_MANOEUVRE 			= 1 << 0;
	const uint8_t ZEROING_MANOEUVRE 					= 1 << 1;
	const uint8_t POLARITY_CHECK 						= 1 << 2;
	const uint8_t CONTROL_CHECK 						= 1 << 3;
	const uint8_t ENCODER_STEPS_CHECK 					= 1 << 4;
	const uint8_t CONTINUOUS_PHASE_INCREASE_MANOEUVRE 	= 1 << 5;
	const uint8_t STEP_MANOEUVRE 						= 1 << 6;
	const uint8_t ZIEGLER_NICHOLS_MANOEUVRE 			= 1 << 7;
	const uint8_t MINIMAL_TUNE = POLARITY_DETECTION_MANOEUVRE | ZEROING_MANOEUVRE | POLARITY_CHECK | CONTROL_CHECK | ENCODER_STEPS_CHECK;
	const uint8_t FULL_TUNE = (1 << 8) - 1;

	// Enumeration of closed loop recording modes
	enum RecordingMode : uint8_t
	{
		Immediate = 0,
		OnNextMove = 1,
	};

	void Init() noexcept;
	EncoderType GetEncoderType() noexcept;
	GCodeResult ProcessM569Point1(const CanMessageGeneric& msg, const StringRef& reply) noexcept;
	GCodeResult ProcessM569Point6(const CanMessageGeneric& msg, const StringRef& reply) noexcept;
	GCodeResult FindEncoderCountPerStep(const CanMessageGeneric& msg, const StringRef& reply) noexcept;
	void Diagnostics(const StringRef& reply) noexcept;

	void EnableEncodersSpi() noexcept;
	void DisableEncodersSpi() noexcept;

#ifdef EXP1HCE
	void TurnAttinyOff() noexcept;
#endif

	void TakeStep() noexcept;
	void SetStepDirection(bool) noexcept;
	bool GetClosedLoopEnabled() noexcept;
	void SetHoldingCurrent(float percent);
	void ResetError(size_t driver) noexcept;
	bool SetClosedLoopEnabled(bool, const StringRef&) noexcept;

	void Spin() noexcept;
	[[noreturn]] void TuningLoop() noexcept;

	void ControlMotorCurrents() noexcept;
	void Log() noexcept;

	uint8_t ReadLiveStatus() noexcept;

	void CollectSample() noexcept;
	GCodeResult StartDataCollection(const CanMessageStartClosedLoopDataCollection&, const StringRef&) noexcept;
	[[noreturn]] void DataCollectionLoop() noexcept;
	[[noreturn]] void DataTransmissionLoop() noexcept;
}

#ifdef EXP1HCL

// The encoder uses the standard shared SPI device, so we don't need to enable/disable it
inline void ClosedLoop::EnableEncodersSpi() noexcept { }
inline void ClosedLoop::DisableEncodersSpi() noexcept { }

#endif

#endif

#endif /* SRC_CLOSEDLOOP_CLOSEDLOOP_H_ */
