
#include "ClosedLoop.h"

#if SUPPORT_CLOSED_LOOP

# include "AS5047D.h"
# include "AbsoluteEncoder.h"
# include "RelativeEncoder.h"

# if SUPPORT_TMC2160
#  include "Movement/StepperDrivers/TMC51xx.h"
# else
#  error Cannot support closed loop with the specified hardware
# endif

/*
 * The following sections, delimited by comments, implement different tuning moves.
 * The comment gives the name of the move, and a description for how it is implemented
 * for both relative and absolute encoders.
 * Each function should perform one 'iteration' of the tuning move, and return true if
 * the iteration was it's last. It will also be supplied with an argument representing
 * if this is it's first iteration.
 *
 * At the bottom of the file, ClosedLoop::PerformTune() is implemented to take advantage
 * of these function
 */


/*
 * Basic tuning
 * ------------------
 *
 *  - Increase the step phase by a little over 4096 counts and back again
 *  - ignore the points near the start position
 *  - feed the remaining (phase, encoder reading) points into a linear regression algorithm, separately on the forward and the reverse movements
 *  - the linear regression gives us the encoder offset and the counts per step
 *  - pass these figures to the ClosedLoop module. It will check the counts per step, set the forward/reverse encoder polarity flag, and set the zero position
 *
 *  Notes on linear regression:
 *  From https://en.wikipedia.org/wiki/Simple_linear_regression the formula to fit a straight line y = mx + c to a set of N (x, y) points is:
 *   m = sigma(i=0..(N-1): (xi - xm) * (yi - ym)) / sigma(i=0..(N-1): (xi - xm)^2)
 *   c = ym - m * xm
 *  where xi is the ith x, yi is the ith y, xm is the mean x, ym is the mean y
 *  In our case the x values are the motor phase values selected, which are spaced uniformly, so that xi = x0 + p*i
 *  So xm = (x0 + (x0 + p*(N-1)))/2 = x0 + p*(N-1)/2
 *  and (xi - xm) = x0 + p*i - x0 - p*(N-1)/2 = p*(i - (N-1)/2)
 *
 *  Expand the numerator in the equation for m to:
 *   sigma(i=0..(N-1): yi*(xi - xm)) - ym*sigma(i=0..(N-1): (xi - xm))
 *  Simplify this to:
 *   sigma(i=0..N-1): yi*p*(i - (N-1)/2)) - ym*sigma(i=0..(N-1): p*(i - (N-1)/2))
 *  and further to:
 *   p * sigma(i=0..N-1): yi*(i - (N-1)/2)) - ym*p*(N * (N-1)/2 - N * (N-1)/2)
 *  and even further to:
 *   p * (sigma(i=0..N-1): yi*(i - (N-1)/2))
 *  We can accumulate the first term as we take readings, and we can accumulate the sum of the y values so we can calculate ym at the end.
 *
 *  The denominator in the equation for m expands to:
 *   sigma(i=0..(N-1): p^2*(i - (N-1)/2)^2)
 *  Expand this to:
 *   p^2 * sigma(i=0..(N-1): i^2) - sigma(i=0..(N-1): i * (N-1)) + sigma(i=0..(N-1): ((N-1)/2)^2)
 *  Simplify to:
 *   p^2 * sigma(i=0..(N-1): i^2) - (N-1) * sigma(i=0..(N-1): i) + N * ((N-1)/2)^2
 *  Using sigma(i=0..(N-1): i) = N * (N-1)/2, sigma(i=0..(N-1): i^2) = (N * (N-1) * (2*N - 1))/6 we get:
 *   p^2 * ((N * (N-1) * (2*N - 1))/6 - (N-1)*N * (N-1)/2 + N * ((N-1)/2)^2)
 *  which simplifies to:
 *   p^2*(N^3-N)/12
 *  So numerator/denominator is:
 *   (sigma(i=0..N-1): yi*(i - (N-1)/2))) / (p*(N^3-N)/12)
 */

static bool BasicTuning(bool firstIteration) noexcept
{
	enum class BasicTuningState { forwardInitial = 0, forwards, reverseInitial, reverse };

	static BasicTuningState state;									// state machine control
	static uint16_t initialStepPhase;								// the step phase we started at
	static int32_t initialEncoderReading;							// this stores the reading at the start of a data collection phase
	static unsigned int stepCounter;								// a counter to use within a state
	static float regressionAccumulator;
	static float readingAccumulator;

	constexpr unsigned int NumDummySteps = 8;						// how many steps to take before we start collecting data
	constexpr uint16_t PhaseIncrement = 8;							// how much to increment the phase by on each step, must be a factor of 4096
	static_assert(4096 % PhaseIncrement == 0);
	constexpr unsigned int NumSamples = 4096/PhaseIncrement;		// the number of samples we take to d the linear regression
	constexpr float HalfNumSamplesMinusOne = (float)(NumSamples - 1) * 0.5;
	constexpr float Denominator = (float)PhaseIncrement * (fcube((float)NumSamples) - (float)NumSamples)/12.0;

	if (firstIteration)
	{
		state = BasicTuningState::forwardInitial;
		stepCounter = 0;
		ClosedLoop::SetForwardPolarity();
	}

	switch (state)
	{
	case BasicTuningState::forwardInitial:
		// In this state we move forwards a few microsteps to allow the motor to settle down
		ClosedLoop::desiredStepPhase += PhaseIncrement;
		ClosedLoop::SetMotorPhase(ClosedLoop::desiredStepPhase, 1.0);
		++stepCounter;
		if (stepCounter == NumDummySteps)
		{
			regressionAccumulator = readingAccumulator = 0.0;
			stepCounter = 0;
			initialStepPhase = ClosedLoop::desiredStepPhase;
			state = BasicTuningState::forwards;
		}
		break;

	case BasicTuningState::forwards:
		// Collect data and move forwards, until we have moved 4 full steps
		{
			const int32_t reading = ClosedLoop::encoder->GetReading();
			if (stepCounter == 0)
			{
				initialEncoderReading = reading;			// to reduce rounding error, get rid of any large constant offset when accumulating
			}
			readingAccumulator += (float)(reading - initialEncoderReading);
			regressionAccumulator += (float)(reading - initialEncoderReading) * ((float)stepCounter - HalfNumSamplesMinusOne);
		}

		if (stepCounter == NumSamples)
		{
			// Save the accumulated data
			const float yMean = readingAccumulator/NumSamples + (float)initialEncoderReading;
			const float slope = regressionAccumulator / Denominator;
			const float xMean = (float)initialStepPhase + (float)PhaseIncrement * HalfNumSamplesMinusOne;
			const float origin = yMean - slope * xMean;
			ClosedLoop::SaveBasicTuningResult(slope, origin, xMean, false);

			stepCounter = 0;
			state = BasicTuningState::reverseInitial;
		}
		else
		{
			ClosedLoop::desiredStepPhase += PhaseIncrement;
			ClosedLoop::SetMotorPhase(ClosedLoop::desiredStepPhase, 1.0);
			++stepCounter;
		}
		break;

	case BasicTuningState::reverseInitial:
		// In this state we move backwards a few microsteps to allow the motor to settle down
		ClosedLoop::desiredStepPhase -= PhaseIncrement;
		ClosedLoop::SetMotorPhase(ClosedLoop::desiredStepPhase, 1.0);
		++stepCounter;
		if (stepCounter == NumDummySteps)
		{
			regressionAccumulator = readingAccumulator = 0.0;
			stepCounter = 0;
			initialStepPhase = ClosedLoop::desiredStepPhase;
			state = BasicTuningState::reverse;
		}
		break;

	case BasicTuningState::reverse:
		// Collect data and move backwards, until we have moved 4 full steps
		{
			const int32_t reading = ClosedLoop::encoder->GetReading();
			if (stepCounter == 0)
			{
				initialEncoderReading = reading;			// to reduce rounding error, get rid of any large constant offset when accumulating
			}
			readingAccumulator += (float)(reading - initialEncoderReading);
			regressionAccumulator += (float)(reading - initialEncoderReading) * ((float)stepCounter - HalfNumSamplesMinusOne);
		}

		if (stepCounter == NumSamples)
		{
			// Save the accumulated data
			const float yMean = readingAccumulator/NumSamples + (float)initialEncoderReading;
			const float slope = regressionAccumulator / (-Denominator);			// negate the denominator because the phase increment was negative this time
			const float xMean = (float)initialStepPhase - (float)PhaseIncrement * HalfNumSamplesMinusOne;
			const float origin = yMean - slope * xMean;
			ClosedLoop::SaveBasicTuningResult(slope, origin, xMean, true);
			ClosedLoop::FinishedBasicTuning();									// call this when we have stopped and are ready to switch to closed loop control
			return true;														// finished tuning
		}
		else
		{
			ClosedLoop::desiredStepPhase -= PhaseIncrement;
			ClosedLoop::SetMotorPhase(ClosedLoop::desiredStepPhase, 1.0);
			++stepCounter;
		}
		break;
	}

	return false;
}


/*
 * Magnetic encoder calibration
 * ----------------------------
 *
 * Absolute:
 * 	- Move to a number of 'target positions'
 * 	- At each target position, record the current encoder reading
 * 	- Store this reading in the encoder LUT
 */

static bool EncoderCalibration(bool firstIteration) noexcept
{
	static int targetPosition;
	static int positionCounter;

	if (ClosedLoop::encoder->GetPositioningType() == EncoderPositioningType::relative)
	{
		return true;			// we don't do this tuning for relative encoders
	}

	AS5047D* absoluteEncoder = (AS5047D*) ClosedLoop::encoder;
	if (firstIteration) {
		absoluteEncoder->ClearLUT();
		targetPosition = 0;
		positionCounter = 0;
	}

	if (ClosedLoop::currentEncoderReading < targetPosition) {
		positionCounter += 1;
	} else if (ClosedLoop::currentEncoderReading > targetPosition) {
		positionCounter -= 1;
	} else {
		const float realWorldPos = absoluteEncoder->GetMaxValue() * positionCounter / (1024 * (360.0 / ClosedLoop::PulsePerStepToExternalUnits(ClosedLoop::encoderPulsePerStep, EncoderType::AS5047)));
		absoluteEncoder->StoreLUTValueForPosition(ClosedLoop::currentEncoderReading, realWorldPos);
		targetPosition += absoluteEncoder->GetLUTResolution();
	}

	if ((unsigned int) targetPosition >= absoluteEncoder->GetMaxValue()) {
		// We are finished
		absoluteEncoder->StoreLUT();
		return true;
	}

	ClosedLoop::desiredStepPhase = (positionCounter > 0 ? 0 : 4096) + positionCounter % 4096;
	ClosedLoop::SetMotorPhase(ClosedLoop::desiredStepPhase, 1);
	return false;
}


/*
 * Continuous Phase Increase
 * -------------
 *
 * Absolute:
 * Relative:
 *  - TODO!
 *
 */

#if 0	// not implemented

static bool ContinuousPhaseIncrease(bool firstIteration) noexcept
{
	return true;
}

#endif

/*
 * Step
 * -------------
 *
 * Absolute:
 * Relative:
 *  - Increase the target motor steps by 4
 *
 */

static bool Step(bool firstIteration) noexcept
{
	ClosedLoop::AdjustTargetMotorSteps(4.0);
	return true;
}


/*
 * Ziegler Nichols Manoeuvre
 * -------------
 *
 * Absolute:
 * Relative:
 *  - TODO!
 *
 */

#if 1
// TODO: Implement ziegler-Nichols move

#else
static bool ZieglerNichols(bool firstIteration) noexcept
{

	// We will need to restore these afterwards...
	const float prevKp = Kp;
	const float prevKi = Ki;
	const float prevKd = Kd;

	// Reset the PID controller
	Ki = 0;
	Kd = 0;
	Kp = 0;
	PIDITerm = 0;

	ultimateGain = 0;		// Reset the ultimate gain value
	int direction = 1;		// Which direction are we moving in

	float lowerBound = 0;
	float upperBound = 10000;

	while (upperBound - lowerBound > 100) {

		Kp = lowerBound + (upperBound - lowerBound) / 2;

		targetMotorSteps = currentMotorSteps + (direction * 10);

		// Flip the direction
		direction = -direction;

		unsigned int initialRiseTime = 0;		// The time it takes to initially meet the target

		float peakError = 0;			// The peak of the current oscillation
		float prevPeakError = 0;		// The peak of the previous oscillation
		unsigned int prevTimestamp = 0;			// The previous time of oscillation

		unsigned int oscillationCount = 0;		// The number of oscillations that have occurred

		float ewmaDecayFraction = 0;	// An EWMA of the decay fraction of oscillations
		float ewmaOscillationPeriod = 0;// An EWMA of the oscillation period

		// Run up to a maximum of 4096
		for (unsigned int time=0; time<16384; time++) {
			TaskBase::Take(10);		// TODO: Use delayuntil here? And run at PID frequency

			ControlMotorCurrents();

			float currentPosition = direction * currentMotorSteps;
			float targetPosition = direction * targetMotorSteps;
			float error = abs(currentPosition - targetPosition);

			// Search for the initial rise time
			if (initialRiseTime == 0) {
				if (currentPosition > targetPosition) {
					initialRiseTime = time;
				} else {
					continue;
				}
			}

			// Wait another two initial rise times for oscillations to occur
			if (time < 3 * initialRiseTime) {continue;}

			// We're now in the prime time for oscillations - check if they are actually happening:

			// Record data if we are above the target
			if (currentPosition > targetPosition) {
				peakError = max<float>(peakError, error);
				continue;
			}
			// Process data if we have just crossed the target
			float decayFraction;
			if (peakError > 0) {
				if (prevPeakError > 0) {
					decayFraction = peakError / prevPeakError;
					ewmaDecayFraction =
							ewmaDecayFraction == 0
							? decayFraction
							: 0.7 * ewmaDecayFraction + 0.3 * decayFraction;
					if (oscillationCount > 5) {
						ewmaOscillationPeriod =
								ewmaOscillationPeriod == 0
								? (time - prevTimestamp)
								: 0.3 * ewmaOscillationPeriod + 0.7 * (time - prevTimestamp);
					}
				}
				oscillationCount++;
				prevPeakError = peakError;
				peakError = 0;
				prevTimestamp = time;
			}

			PIDPTerm = ewmaOscillationPeriod;
			PIDDTerm = (time - prevTimestamp);

			// Wait for at least 5 oscillations
			if (oscillationCount < 5) {
				continue;
			}

			// Check that the next 5 oscillations all keep the average decay fraction above 98%
			if (ewmaDecayFraction < 0.98) {
				// No oscillations, this is the new lower bound.
				lowerBound = Kp;
				break;
			}
			if (oscillationCount >= 10) {
				// Oscillations found! This is the new upper bound.
				upperBound = Kp;
				oscillationPeriod = ewmaOscillationPeriod;
				break;
			}

			// If we time out of this loop, assume no oscillations
			if (time == 16383) {
				lowerBound = Kp;
			}

		}
	}

	ultimateGain = upperBound;
	Kp = prevKp;
	Ki = prevKi;
	Kd = prevKd;

	tuning &= ~ZIEGLER_NICHOLS_MANOEUVRE;
}
#endif


/*
 * ClosedLoop::PerformTune()
 * -------------------------
 *
 * Makes use of the above tuning functions.
 *
 */

// This is called from every iteration of the closed loop control loop if tuning is enabled
void ClosedLoop::PerformTune() noexcept
{
	static bool newTuningMove = true;						// indicates if a tuning move has just finished

	// Check we are in direct drive mode and we have an encoder
	if (SmartDrivers::GetDriverMode(0) != DriverMode::direct || encoder == nullptr ) {
		tuningError |= TUNE_ERR_SYSTEM_ERROR;
		tuning = 0;
		return;
	}

	// Run one iteration of the one, highest priority, tuning move
	if (tuning & BASIC_TUNING_MANOEUVRE) {
		if (encoder->GetPositioningType() == EncoderPositioningType::absolute && (tuning & ENCODER_CALIBRATION_MANOEUVRE)) {
			((AS5047D*)encoder)->ClearLUT();				//TODO this assumes that any absolute encoder is a AS5047D. Make ClearLUT a virtual method?
		}
		newTuningMove = BasicTuning(newTuningMove);
		if (newTuningMove) {
			tuningError &= ~TUNE_ERR_NOT_DONE_BASIC;
			tuning &= ~BASIC_TUNING_MANOEUVRE;				// we can do encoder calibration after basic tuning
		}
	} else if (tuning & ENCODER_CALIBRATION_MANOEUVRE) {
		newTuningMove = EncoderCalibration(newTuningMove);
		if (newTuningMove) {
			tuning = 0;
		}
	} else if (tuning & STEP_MANOEUVRE) {
		newTuningMove = Step(newTuningMove);
		if (newTuningMove) {
			tuning = 0;
		}
#if 0	// not implemented
	} else if (tuning & CONTINUOUS_PHASE_INCREASE_MANOEUVRE) {
		newTuningMove = ContinuousPhaseIncrease(newTuningMove);
		if (newTuningMove) {
			tuning = 0;
		}
	} else if (tuning & ZIEGLER_NICHOLS_MANOEUVRE) {
		newTuningMove = ZieglerNichols(newTuningMove);
		if (newTuningMove) {
			tuning = 0;
		}
#endif
	} else {
		tuning = 0;
		newTuningMove = true;								// ready for next time
	}
}

#endif	// #if SUPPORT_CLOSED_LOOP
