#include "StepperServo.h"

#define CONTROL_INTERVAL_MS 100
#define ANGLE_MIN -90.0
#define ANGLE_MAX 90.0

#define ADC_MIN 130
#define ADC_MAX 750

#define ADC_MIN_POT 50
#define ADC_MAX_POT 900

#define STEPS 200
#define STEP_FACTOR 16
#define STEP_MAX (+STEP_FACTOR * STEPS / 4)  // 90 degrees
#define STEP_MIN (-STEP_FACTOR * STEPS / 4)  // -90 degrees

#define MAX_INTEGRAL 30

#define TEETH_BIG_GEAR 82.0
#define TEETH_SMALL_GEA 10.0

StepperServo::StepperServo(Thread &thr, Connector &uext)
    : Actor(thr),
      Device(thr),
      _uext(uext),
      _pulser(uext.toPin(LP_TXD)),
      _pinDir(uext.getDigitalOut(LP_SCL)),
      _pinEnable(uext.getDigitalOut(LP_SDA)),
      _adcPot(
          ADC::create(uext.toPin(LP_RXD))),  // _adcPot(uext.getADC(LP_RXD)),
      _measureTimer(thr, 4, 20, true),
      _controlTimer(thr, 3, CONTROL_INTERVAL_MS, true),
      _reportTimer(thr, 2, 500, true),
      stepsPerRotation("stepper/stepsPerRotation", STEPS * STEP_FACTOR) {
  _pulser.divider = 80;
  _pulser.intervalSec = 0.001;
  _pulser.autoReload = true;
  stepsPerRotation = STEPS * STEP_FACTOR;
}

StepperServo::~StepperServo() {}

void StepperServo::init() {
  INFO(" servo init().");
  INFO(" Checking potentiometer....");
  Erc rc = _adcPot.init();
  if (rc != E_OK) {
    stop("Potentiometer initialization failed");
  }
  stopOutOfRange(_adcPot.getValue());
  _pulser.init();
  _pinDir.setMode(DigitalOut::DOUT_PULL_UP);
  _pinEnable.setMode(DigitalOut::DOUT_PULL_UP);
  _pinDir.init();
  _pinDir.write(1);
  _pinEnable.init();
  _pinEnable.write(0);

  angleTarget >> ([&](const int &angle) {
    if (angleTarget() < ANGLE_MIN) angleTarget = ANGLE_MIN;
    if (angleTarget() > ANGLE_MAX) angleTarget = ANGLE_MAX;
    int target = scale(angleTarget(), -90.0, 90.0, STEP_MIN, STEP_MAX);
    target = target / STEP_FACTOR;
    target = target * STEP_FACTOR;
    stepTarget = target;
  });

  _measureTimer >> ([&](TimerMsg tm) {
    int adc = _adcPot.getValue();
    stopOutOfRange(adc);  // stop if needed
    _potFilter.addSample(adc);
    //    adcPot.on(adc);
    measureAngle();
    if (angleMeasured() == 0) stepMeasured = 0;  // correct missed steps
  });

  stepTarget >> ([&](const int &st) {
    if (isRunning()) {
      if (true /*isDriving()*/) {
        if (!_pulser.busy()) {  // previous stepped stopped

          int delta = st - stepMeasured();
          stepMeasured = st;
          if (delta < 0) {
            _direction = -1;
            _pinDir.write(0);
            _pulser.ticks = -delta;
          } else {
            _direction = 1;
            _pinDir.write(1);
            _pulser.ticks = delta;
          }
          if (_pulser.ticks()) {
            _pinEnable.write(0);
            _pulser.start();
          }
        } else {
          INFO("pulser busy ");
        }
      } else {
        INFO("not DRIVING");
        holdAngle();
      }
    } else {
      INFO("not RUNNING");
      stopStepper();
    }
  });

  _controlTimer >> ([&](TimerMsg tm) { /*
    measureAngle();
    if (isRunning())
    {angleTarget800
      if (isDriving())
      {
        if (measureAngle())
        {
        _error = angleTarget() - angleMeasured();
          error = _error;
          output = PID(_error, CONTROL_INTERVAL_MS / 1000.0);
          int direction = output() < 0 ? 0 : 1;
          _pinDir.write(direction);
          if (abs(output()) > 3)
          {
            _pulser.intervalSec = 0.001;
            _pulser.ticks = 100;
            _pulser.start();
            _pinEnable.write(0); // negatisudo apt install net-tools

          else
          {
            holdAngle();800     _pulser.start();

      }
      else
      {
        holdAngle();
      }
    }
    else if (isPaused())
    { // isDriving false
      holdAngle();
    }
    else
    { // stopped
      stopStepper();
    }
  */
  });
  adcPot.pass(true);
  angleTarget.pass(true);
  angleMeasured.pass(true);
  stepTarget.pass(true);
  stepMeasured.pass(true);
  if (!stopOutOfRange(_adcPot.getValue())) run();
}

void StepperServo::holdAngle() {
  _pulser.stop();
  _pulser.ticks = 0;
  _pinEnable.write(0);  // enable current
}

void StepperServo::stopStepper() {
  _pulser.stop();
  _pulser.intervalSec = 0.001;
  _pulser.ticks = 0;
  _pinEnable.write(1);  // disable: stop current through stepper motor
}

bool StepperServo::stopOutOfRange(int adc) {
  if (adc < ADC_MIN_POT || adc > ADC_MAX_POT) {
    if (deviceState() != STOPPED) {
      std::string s;
      string_format(s, " Potentiometer angle out of Range : %d < %d < %d ",
                    ADC_MIN_POT, adc, ADC_MAX_POT);
      stop(s.c_str());
    }
    return true;
  }
  return false;
}

bool StepperServo::measureAngle() {
  adcPot = _potFilter.getMedian();  // noise filtering
  if (_potFilter.isReady()) {
    angleMeasured = scale(adcPot(), ADC_MIN, ADC_MAX, ANGLE_MIN, ANGLE_MAX);
    return true;
  }
  return false;
}

float StepperServo::scale(float x, float x1, float x2, float y1, float y2) {
  float y = y1 + ((x - x1) / (x2 - x1)) * (y2 - y1);
  return y;
}

/*float StepperServo::PID(float err, float interval)
{
  integral = integral() + (err * interval);
  derivative = (err - _errorPrior) / interval;
  float integralPart = KI() * integral();
  if (integralPart > MAX_INTEGRAL)
    integral = MAX_INTEGRAL / KI();
  if (integralPart < -MAX_INTEGRAL)
    integral = -MAX_INTEGRAL / KI();
  float output = KP() * err + KI() * integral() + KD() * derivative();
  _errorPrior = err;
  return output;
}*/
