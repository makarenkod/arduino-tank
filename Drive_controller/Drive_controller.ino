// CS
// Hall sensors
// Bluetooth
// Complex Animation
//

struct Range { // minValue <= value <= maxValue
  int minValue;
  int maxValue;

  int fit(int value) {
    return value >= maxValue ? maxValue : (value <= minValue ? minValue : value);
  }
};

class Utils {
  public:
    static long now() { return long(millis()); }
};

class Device {
  private:
    long lastSyncTimeMillis;

  protected:
    Device(){
      lastSyncTimeMillis = 0;
    }

  public:
    virtual void init() { }
    
    void sync(long now){
        lastSyncTimeMillis = now;
        onSync(lastSyncTimeMillis);
    }

    virtual String state() { return ""; }
  protected:
    virtual void onSync(long now){ }
};

class Animation {
private:
  long startTimeMillis;

public:
  Animation(long startTimeMillis) {
    this->startTimeMillis = startTimeMillis;
  }

  virtual ~Animation() {}
  virtual int calcValue(long timeMillis) = 0;

protected:
  long getStartTime() { return startTimeMillis; }
};

class ConstValue: public Animation {
private:
  int value;

public:
  ConstValue(unsigned long startTimeMillis, int value)
    : Animation(startTimeMillis) {
    this->value = value;
  }

  int calcValue(long timeMillis) override {
    return value;
  }
};

class LinearAnimation: public Animation {
  private:
    int startValue;
    int endValue;
    int durationMillis;

  public:
    LinearAnimation(unsigned long startTimeMillis, int startValue, int endValue, int durationMillis): Animation(startTimeMillis) {
      this->startValue = startValue;
      this->endValue = endValue;
      this->durationMillis = durationMillis;
    }

    int calcValue(long timeMillis) override {
      int value = startValue;
      long startTimeMillis = getStartTime();

      if ((timeMillis >= startTimeMillis) && (timeMillis < startTimeMillis + durationMillis)) {
        value = startValue + (timeMillis - startTimeMillis) * (endValue - startValue)/ durationMillis;
      }
      else if (timeMillis >= startTimeMillis + durationMillis){
        value = endValue;
      }

      return value;
    }
};

class BlinkingAnimation: public Animation {
  private:
    Range range;
    unsigned long periodMillis;
    unsigned long dutyCycle;

  public:
    BlinkingAnimation(
      unsigned long startTimeMillis,
      const Range& range,
      unsigned long periodMillis,
      unsigned long dutyCycle = 50
    )
      : Animation(startTimeMillis) {

      this->range = range;
      this->periodMillis = periodMillis;

      Range dutyCycleRange = Range{ .minValue = 0, .maxValue = 100};
      this->dutyCycle = dutyCycleRange.fit(dutyCycle);
    }

    int calcValue(long timeMillis) override {
      unsigned long phase = (timeMillis - getStartTime()) % periodMillis;
      int value = range.minValue;

      if (phase < (periodMillis * dutyCycle) / 100l) {
        value = range.maxValue;
      }

      return value;
    }
};

class Animatable {
  private:
    Animation* pAnimation;
  protected:
    Animatable() {
      pAnimation = new ConstValue(Utils::now(), 0);
    }

    virtual ~Animatable(){
      delete pAnimation;
      pAnimation = NULL;
    }

    virtual void setValue(int value) = 0;
    virtual Range getRange() const = 0;
  public:
    virtual int  getValue() const  = 0;

    void setValueNow(int newValue){
      delete pAnimation;
      pAnimation = new ConstValue(Utils::now(), newValue);
    }

    void animateLinearly(int newValue, int durationMillis){
      delete pAnimation;
      pAnimation = new LinearAnimation(Utils::now(), getValue(), newValue, durationMillis);
    }

    void animateMeander(int periodMillis, int dutyCycle){
      delete pAnimation;
      pAnimation = new BlinkingAnimation(Utils::now(), getRange(), periodMillis, dutyCycle);
    }

    void stopAnimation() {
      setValueNow(getValue());
    }

protected:
  virtual void onSync(long now) {
    setValue(pAnimation->calcValue(now));
  }
};

class LED: public Animatable, public Device {
  private:
    int pin;
    int brightness;

  public:
    LED(int pin) {
      this->pin = pin;
      this->brightness = 0;
    }

    virtual void onSync(long now) {
      Animatable::onSync(now);
      analogWrite(pin, calcLogValue(getValue()));
    }

    virtual int getValue() const { return brightness; }
    virtual Range getRange() const {
      // Brightness, [0..99]. 0 means off, 99 - full
      return Range{ .minValue = 0, .maxValue = 99};
    };

  protected:
    virtual void setValue(int brightness){
      this->brightness = getRange().fit(brightness);
    }

  public:
    virtual void init(){
       pinMode(pin, OUTPUT);
    }

  public:
    int calcLogValue(int value) {
      return pow (2, (value / logFactor)) - 1;
    }

    float calcLogFactor(int maxValue) {
      Range range = getRange();
      return (range.maxValue * log10(2))/(log10(maxValue + 1));
    }

  private:
    const int maxLedValue = 255;
    const float logFactor = calcLogFactor(maxLedValue);
};

class Motor: public Animatable, public Device {
  private:
    int pinInA;
    int pinInB;
    int pinPwm;
    int pinCS;
    int pinEN;
    int speed;
    boolean error;
    int current;

  public:
    Motor(int pinInA, int pinInB, int pinPwm, int pinCS, int pinEN) {
      this->pinInA = pinInA;
      this->pinInB = pinInB;
      this->pinPwm = pinPwm;
      this->pinCS = pinCS;
      this->pinEN = pinEN;
      this->speed = 0;
      this->error = false;
      this->current = 0;
    }

    virtual void onSync(long now) {
      Animatable::onSync(now);

      error = LOW == digitalRead(pinEN);
      current = analogRead(pinCS);

      int pwm = speed * 255 / getRange().maxValue;

      digitalWrite(pinInA, pwm < 0 ? HIGH : LOW);
      digitalWrite(pinInB, pwm > 0 ? HIGH : LOW);
      analogWrite(pinPwm, pwm);
    }

    virtual int getValue() const { return speed; }
    virtual Range getRange() const {
      return Range{.minValue = -99, .maxValue = 99};
    }

    virtual String state() {
      return String(error ? "ERR" : "OK") + ";" + "I" + String(current) + ";";
    }

    boolean isError() const {
        return error;
    }

    void cleanError() {
        error = false;
    }


  protected:
    // Speed, [-99..0..99]. 0 means the motor is stopped
    virtual void setValue(int speed){
      this->speed = getRange().fit(speed);
    }

  public:
    virtual void init() {
      pinMode(pinInA, OUTPUT);
      pinMode(pinInB, OUTPUT);
      pinMode(pinPwm, OUTPUT);
      pinMode(pinCS,  INPUT_PULLUP); // !!! may need external pull-up resistor
      pinMode(pinEN,  INPUT_PULLUP);
    }
};

/*
volatile byte half_revolutions;
 unsigned int rpm;
 unsigned long timeold;
 void setup()
 {
   rpm = 0;
   timeold = 0;
 }
 void loop()
 {
   if (half_revolutions >= 20) { 
     //Update RPM every 20 counts, increase this for better RPM resolution,
     //decrease for faster update
     rpm = 30*1000/(millis() - timeold)*half_revolutions;
     timeold = millis();
     half_revolutions = 0;
   }
 }
*/
 class HallSensor: public Device {
  private:
    int pin;
    int id;
    int rpm;
    long lastTimeMillis;
    int updatePeriodMillis;
    
  public:  
    // id: [0..3]
    HallSensor(int id, int pin, int updatePeriodMillis) {
       this->pin = pin;
       this->id = id;
       this->updatePeriodMillis = updatePeriodMillis;
       this->rpm = 0;
       this->lastTimeMillis = Utils::now();
    }
    
    virtual void init() {
      attachInterrupt(pin, interruptHandlers[id], RISING);
    }
    
    virtual String state() { return ""; }

    int getRPM() const {
      return rpm;  
    }
    
  protected:
    virtual void onSync(long now) {
       if (now > lastTimeMillis + updatePeriodMillis) { 
         detachInterrupt(pin);
         rpm = (halfRevolutions[id] * 1000 * 60 / 2) / (now - lastTimeMillis);
         lastTimeMillis = now;
         halfRevolutions[id] = 0;
         attachInterrupt(pin, interruptHandlers[id], RISING);
       }
    }

  private:
    static volatile byte halfRevolutions[4];
    static void interruptHandler0();
    static void interruptHandler1();
    static void interruptHandler2();
    static void interruptHandler3();
    static void (* interruptHandlers[4])();
};

volatile byte HallSensor::halfRevolutions[4] = {0, 0, 0, 0};

void HallSensor::interruptHandler0() { ++halfRevolutions[0]; }
void HallSensor::interruptHandler1() { ++halfRevolutions[1]; }
void HallSensor::interruptHandler2() { ++halfRevolutions[2]; }
void HallSensor::interruptHandler3() { ++halfRevolutions[3]; }

void (* HallSensor::interruptHandlers[4])() = {interruptHandler0, interruptHandler1, interruptHandler2, interruptHandler3};
 
class TankState {
  // Nano: PWM: 3, 5, 6, 9, 10, and 11. 
  private: //PINs
  
    const int FRONT_RIGHT_LED_PIN = 9;  // D9,  PWM
    const int FRONT_LEFT_LED_PIN  = 6;  // D6,  PWM
    const int REAR_RIGHT_LED_PIN  = 11; // D11, PWM
    const int REAR_LEFT_LED_PIN   = 10; // D10, PWM

    const int MOTOR_1_INA_PIN = 7; // D7
    const int MOTOR_1_INB_PIN = 8; // D8
    const int MOTOR_1_PWM_PIN = 5; // D5, PWM
    const int MOTOR_1_CS_PIN = A5; // A5
    const int MOTOR_1_EN_PIN = A7; // A7
    const int MOTOR_1_HS_1 = 0; // ???
    const int MOTOR_1_HS_2 = 0; // ???    

    const int MOTOR_2_INA_PIN = 2; // D2
    const int MOTOR_2_INB_PIN = 4; // D4
    const int MOTOR_2_PWM_PIN = 3; // D3, PWM
    const int MOTOR_2_CS_PIN = A4; // A4
    const int MOTOR_2_EN_PIN = A6; // A6
    const int MOTOR_2_HS_1 = 0; // ???
    const int MOTOR_2_HS_2 = 0; // ???
    
  private:
    const int updatePeriodMillis = 1000;
      
    Motor       motorLeft     = Motor(MOTOR_1_INA_PIN, MOTOR_1_INB_PIN, MOTOR_1_PWM_PIN, MOTOR_1_CS_PIN, MOTOR_1_EN_PIN);
    HallSensor  motorLeftHs1  = HallSensor(0, MOTOR_1_HS_1, updatePeriodMillis); 
    HallSensor  motorLeftHs2  = HallSensor(1, MOTOR_1_HS_2, updatePeriodMillis);
    Motor       motorRight    = Motor(MOTOR_2_INA_PIN, MOTOR_2_INB_PIN, MOTOR_2_PWM_PIN, MOTOR_2_CS_PIN, MOTOR_2_EN_PIN);
    HallSensor  motorRightHs1 = HallSensor(2, MOTOR_2_HS_1, updatePeriodMillis); 
    HallSensor  motorRightHs2 = HallSensor(3, MOTOR_2_HS_2, updatePeriodMillis);
    LED         ledLeftFront  = LED(FRONT_LEFT_LED_PIN);
    LED         ledRightFront = LED(FRONT_RIGHT_LED_PIN);
    LED         ledLeftRear   = LED(REAR_LEFT_LED_PIN);
    LED         ledRightRear  = LED(REAR_RIGHT_LED_PIN);
    
    Device* devices[10] = {
      &motorLeft, &motorRight, 
      &motorLeftHs1, &motorLeftHs2, &motorRightHs1, &motorRightHs2,
      &ledLeftFront, &ledRightFront, &ledLeftRear, &ledRightRear
    };
    
    boolean showStatus  = false;

  public:
    void init() {
      for(unsigned int i = 0; i < sizeof(devices)/sizeof(devices[0]); i++){
        Device* pDevice = devices[i];
        pDevice->init();
      }
    }

    void sync(long now) {
      for(unsigned int i = 0; i < sizeof(devices)/sizeof(devices[0]); i++){
        Device* pDevice = devices[i];
        pDevice->sync(now);
      }

      if (motorLeft.isError() || motorRight.isError()) {
        stop();
        blinkError();
      }

      if (showStatus) {
        showStatus = false;
        Serial.println(
          "Motor(L=" + String(motorLeft.getValue()) + "," +
                "R=" + String(motorRight.getValue()) +
          ")"
        );

        Serial.println(
          "LED(LF=" + String(ledLeftFront.getValue()) + "," +
              "RF=" + String(ledRightFront.getValue()) + "," +
              "LR=" + String(ledLeftRear.getValue()) + "," +
              "RR=" + String(ledRightRear.getValue()) + "," +
          ")"
        );
      }
    }

    void stop()
    {
      motorLeft.setValueNow(0);
      motorRight.setValueNow(0);
    }

    // speed [-99..99]
    void leftMotor(int speed) {
      motorLeft.setValueNow(speed);
    }

    void rightMotor(int speed) {
      motorRight.setValueNow(speed);
    }

    void lights(boolean on) {
      int value = on ? 99 : 0;
      int duration = 1000;
      ledLeftFront.animateLinearly(value, duration);
      ledRightFront.animateLinearly(value, duration);
      ledLeftRear.animateLinearly(value, duration);
      ledRightRear.animateLinearly(value, duration);
    }

    void blinkRight() {
        int periodMillis = 1000;
        int dutyCycle = 50;
        ledRightFront.animateMeander(periodMillis, dutyCycle);
        ledRightRear.animateMeander(periodMillis, dutyCycle);
        ledLeftFront.setValueNow(0);
        ledLeftRear.setValueNow(0);
    }

    void blinkLeft() {
      int periodMillis = 1000;
      int dutyCycle = 50;
      ledLeftFront.animateMeander(periodMillis, dutyCycle);
      ledLeftRear.animateMeander(periodMillis, dutyCycle);
      ledRightFront.setValueNow(0);
      ledRightRear.setValueNow(0);
    }

    void blinkError() {
        int periodMillis = 500;
        int dutyCycle = 50;
        ledRightFront.animateMeander(periodMillis, dutyCycle);
        ledRightRear.animateMeander(periodMillis, dutyCycle);
        ledLeftFront.animateMeander(periodMillis, dutyCycle);
        ledLeftRear.animateMeander(periodMillis, dutyCycle);
    }

    void recover() {
      motorLeft.cleanError();
      motorRight.cleanError();
    }

    void status() {
      showStatus = true;
    }
};

class Command {
  private:
    String code;
    String description;
    void (*action)(TankState&);

  public:

    Command(String code, String description, void (*action)(TankState&)) {
      this->code = code;
      this->description = description;
      this->action = action;
    }

    String  getCode() const { return code; }
    String  getDescription() const { return description; }
    void    apply(TankState& state) const { action(state); }
};

class IO {
  private:
       String data;
  public:
    void init(){
      Serial.begin(9600);
    }

    String getData(unsigned int maxLength, char stopChar) {
      String commandCode;

      while((data.length() < maxLength) && Serial.available()) {
        char ch = Serial.read();
        if (stopChar == ch) {
          commandCode = data;
          data = "";
          break;
        }
        data += String(ch);
      }

      return commandCode;
    }

    void log(String message = "") {
      Serial.println(message);
    }
};

class Commands {
private:
  static Command nopCommand;
  static Command commands[];

public:
  const Command& NOP() { return nopCommand; }

  const Command& findByCode(String commandCode) {
    for(unsigned int i = 0; i < numCommands(); i++) {
      const Command& currentCommand =  commands[i];
      if(commandCode == currentCommand.getCode()) {
        return currentCommand;
      }
    }

    return NOP();
  }

  String commandDescriptions() const {
    String description;
    for(unsigned int i = 0; i < numCommands(); i++) {
      const Command& command =  commands[i];
      description += command.getCode() + " " + command.getDescription();

      if (i < numCommands() - 1) {
        description += "\n";
      }
    }

    return description;
  }

  unsigned int numCommands() const;
};

Command Commands::nopCommand = Command("", "NOP", [](TankState& s) {});

Command Commands::commands[] = {
  Command("5", "Stop",             [](TankState& s) {s.stop();}),
  Command("6", "Rotate right",     [](TankState& s) {s.leftMotor(50); s.rightMotor(0);}),
  Command("4", "Rotate left",      [](TankState& s) {s.leftMotor(0);  s.rightMotor(50);}),
  Command("8", "Full forward",     [](TankState& s) {s.leftMotor(99); s.rightMotor(99);}),
  Command("2", "Slow backward",    [](TankState& s) {s.leftMotor(-25);s.rightMotor(-25);}),
  Command("L", "Lights up",        [](TankState& s) {s.lights(true);}),
  Command("O", "Lights down",      [](TankState& s) {s.lights(false);}),
  Command("BR", "Blink right",     [](TankState& s) {s.blinkRight();}),
  Command("BL", "Blink left",      [](TankState& s) {s.blinkLeft();}),
  Command("R",  "Recover",         [](TankState& s) {s.recover();}),
  Command("S",  "Status",          [](TankState& s) {s.status();})
};

unsigned int Commands::numCommands() const { return sizeof(commands)/sizeof(commands[0]); }

TankState state;
IO io;
Commands commands;
String prompt = "Enter a command, followed by semicolon >";
static const char COMMAND_SEPARATOR = ';';
static const unsigned int MAX_COMMAND_LENGTH = 16;

void setup() {
  state.init();
  io.init();
  io.log(commands.commandDescriptions());
  io.log(prompt);
}

void loop(){
  String commandCode = io.getData(MAX_COMMAND_LENGTH, COMMAND_SEPARATOR);
  long now = Utils::now();

  if(commandCode.length() > 0) {
    Command command = commands.findByCode(commandCode);
    command.apply(state);
    state.sync(now);
    io.log(prompt);
  }
  else {
    state.sync(now);
  }
}
