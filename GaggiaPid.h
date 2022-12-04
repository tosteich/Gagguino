class GaggiaPid {
private:
  float iState = 0;
  float lastPoint = 0;
  float pTerm, dTerm;
  float Kp, Ki, Kd;
  float setPoint;
  float periodSec;
  int maxPower = 100;

public:
  GaggiaPid(int period) {
    periodSec = period / 1000.0;
  }

  void setupPid(float Kp, float Ki, float Kd, float targetPoint) {
    this->Kp = Kp;
    this->Ki = Ki;
    this->Kd = Kd;
    this->setPoint = targetPoint;
  }

  void setKp(float Kp) {
    this->Kp = Kp;
  }

  void setKi(float Ki) {
    this->Ki = Ki;
  }

  void setKd(float Kd) {
    this->Kd = Kd;
  }

  float getKp() {
    return Kp;
  }

  float getKi() {
    return Ki;
  }

  float getKd() {
    return Kd;
  }

  void setSetPoint(float setPoint) {
    this->setPoint = setPoint;
  }

  float getSetPoint() {
    return setPoint;
  }

  int getResult(float input) {
    // determine how badly we are doing
    float error = setPoint - input;

    pTerm = Kp * error;

    dTerm = Kd * (input - lastPoint) / periodSec;

    lastPoint = input;

    iState += error * Ki * periodSec;

    float result = constrain((pTerm + dTerm), 0, maxPower);

    if (Ki != 0) {
      iState = constrain(iState, (0 - result) / (Ki * periodSec), (maxPower - result) / (Ki * periodSec));
    }

    iState = constrain(iState, 0, maxPower);
    result = (int)constrain((result + iState), 0, maxPower);

    //Serial.println(result);
    return result;
  }
};
