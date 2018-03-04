// CS
// Hall sensors
// Bluetooth
// Complex Animation

class Animation {
private:
  unsigned long startTimeMillis;

public:
  Animation(unsigned long startTimeMillis) {
    this->startTimeMillis = startTimeMillis;
  }

  int calcValue(unsigned long timeMillis) {
    return 0;
  }

protected:
  unsigned long getStartTime() { return startTimeMillis; }
};

class ConstValue: public Animation {
private:
  int value;

public:
  ConstValue(unsigned long startTimeMillis, int value)
    : Animation(startTimeMillis) {
    this->value = value;
  }

  int calcValue(unsigned long timeMillis) {
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

  int calcValue(unsigned long timeMillis) {
    int value = startValue;
    unsigned long startTimeMillis = getStartTime();

    if (timeMillis >= startTimeMillis && timeMillis < startTimeMillis + durationMillis) {
      value = startValue + (timeMillis - startTimeMillis) * (endValue - startValue)/ durationMillis;
    }
    else if (timeMillis >= startTimeMillis + durationMillis){
      value = endValue;
    }

    return value;
  }
};

struct Range { // minValue <= value <= maxValue
  int minValue;
  int maxValue;

  int fit(int value) {
    return value > maxValue ? maxValue : (value < minValue ? minValue : value);
  }
};

class Animatable {
protected:
  Animatable() {
    this -> pAnimation = new ConstValue(now(), 0);
  }

  virtual void setValue(int value) = 0;
  virtual Range getRange() const = 0;

public:
  virtual int  getValue() const  = 0;

  void setValueNow(int newValue){
    delete this -> pAnimation;
    this -> pAnimation = new ConstValue(now(), newValue);
  }

  void animateLinearly(int newValue, int durationMillis){
    delete this-> pAnimation;
    this->pAnimation = new LinearAnimation(now(), getValue(), newValue, durationMillis);
  }

  void animateMeander(int minValue, int maxValue, int periodMillis, int offsetMillis){
  }

  void stopAnimation() {
    setValueNow(getValue());
  }

private:
  void sync() {
    setValue(pAnimation->calcValue(now()));
  }

  unsigned long now() { millis(); }

private:
  Animation* pAnimation;
};

class LED: public Animatable {
  private:
    int pin;
    int brightness;

  public:
    LED(int pin) {
      this->pin = pin;
      this->brightness = 0;

      init();
    }

    void sync() {
      analogWrite(pin, calcLogValue(brightness));
    }

    int getValue() const { return this->brightness; }
    virtual Range getRange() const {
      // Brightness, [0..99]. 0 means off, 99 - full
      return Range{ .minValue = 0, .maxValue = 99};
    };

  protected:
    void setValue(int brightness){
      this->brightness = getRange().fit(brightness);
    }

  private:
    void init(){
       pinMode(pin, OUTPUT);
    }

public:
    int calcLogValue(int value) {
      return pow (2, (value / logFactor)) - 1;
    }

    float calcLogFactor() {
      Range range = getRange();
      return (range.maxValue * log10(2))/(log10(maxLedValue + 1));
    }

  private:
    const float logFactor = calcLogFactor();
    const int maxLedValue = 255;
};

class Motor: public Animatable {
  private:
    int pinInA;
    int pinInB;
    int pinPwm;
    int pinCS;
    int pinEN;
    int speed;

  public:
    Motor(int pinInA, int pinInB, int pinPwm, int pinCS, int pinEN) {
      this->pinInA = pinInA;
      this->pinInB = pinInB;
      this->pinPwm = pinPwm;
      this->pinCS = pinCS;
      this->pinEN = pinEN;
      this->speed = 0;

      init();
    }

    void sync() {
      int pwm = this->speed * 255 / getRange().maxValue;

      if( pwm == 0) {
        digitalWrite(pinInA, LOW);
        digitalWrite(pinInB, LOW);
      }
      else if( pwm > 0 ){
        digitalWrite(pinInA, LOW);
        digitalWrite(pinInB, HIGH);
      }
      else {
        digitalWrite(pinInA, HIGH);
        digitalWrite(pinInB, LOW);
      }

      analogWrite(pinPwm, pwm);
    }

    int getValue() const { return this->speed; }
    virtual Range getRange() const { return Range{.minValue = -99, .maxValue = 99}; };

  protected:
    // Speed, [-99..0..99]. 0 means the motor is stopped
    void setValue(int speed){
      this->speed = getRange().fit(speed);
    }

  private:
    void init() {
      pinMode(pinInA, OUTPUT);
      pinMode(pinInB, OUTPUT);
      pinMode(pinPwm, OUTPUT);
      pinMode(pinCS,  OUTPUT);
      pinMode(pinEN,  INPUT_PULLUP);
    }
};

class TankState {
  private:
    const int FRONT_RIGHT_LED_PIN = 9;
    const int FRONT_LEFT_LED_PIN  = 6;
    const int REAR_RIGHT_LED_PIN  = 11;
    const int REAR_LEFT_LED_PIN   = 10;

    const int MOTOR_1_INA_PIN = 7; // D7
    const int MOTOR_1_INB_PIN = 8; // D8
    const int MOTOR_1_PWM_PIN = 5; // D5
    const int MOTOR_1_CS_PIN = A5;  // A5
    const int MOTOR_1_EN_PIN = A7;  // A7

    const int MOTOR_2_INA_PIN = 2; //D2
    const int MOTOR_2_INB_PIN = 4; // D4
    const int MOTOR_2_PWM_PIN = 3; // D3
    const int MOTOR_2_CS_PIN = A4;  // A4
    const int MOTOR_2_EN_PIN = A6;  // A6

  private:
    Motor motorLeft     = Motor(MOTOR_1_INA_PIN, MOTOR_1_INB_PIN, MOTOR_1_PWM_PIN, MOTOR_1_CS_PIN, MOTOR_1_EN_PIN);
    Motor motorRight    = Motor(MOTOR_2_INA_PIN, MOTOR_2_INB_PIN, MOTOR_2_PWM_PIN, MOTOR_2_CS_PIN, MOTOR_2_EN_PIN);
    LED   ledLeftFront  = LED(FRONT_LEFT_LED_PIN);
    LED   ledRightFront = LED(FRONT_RIGHT_LED_PIN);
    LED   ledLeftRear   = LED(REAR_LEFT_LED_PIN);
    LED   ledRightRear  = LED(REAR_RIGHT_LED_PIN);

  public:
    void sync() {
      motorLeft.sync();
      motorRight.sync();
      ledLeftFront.sync();
      ledRightFront.sync();
      ledLeftRear.sync();
      ledRightRear.sync();

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
  public:
    void init(){
      Serial.begin(9600);
    }

    String getData(int maxLength, char stopChar) const {
      String data;
      int length = 0;

      while((data.length() < maxLength) && Serial.available()) {
        char ch = Serial.read();
        if (stopChar == ch) break;
        data += String(Serial.read());
      }

      return data;
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
    for(unsigned int i; i < numCommands(); i++) {
      const Command& currentCommand =  commands[i];
      if(commandCode == currentCommand.getCode()) {
        return currentCommand;
      }
    }

    return NOP();
  }

  String commandDescriptions() const {
    String description;
    for(unsigned int i; i < numCommands(); i++) {
      const Command& command =  commands[i];
      description += command.getCode() + " " + command.getDescription();

      if (i < numCommands() - 1) {
        description += "\n";
      }
    }

    return description;
  }

  int numCommands() const;
};

Command Commands::nopCommand = Command("", "NOP", [](TankState& s) {});

Command Commands::commands[] = {
  Command("5", "Stop",             [](TankState& s) {s.stop();}),
  Command("6", "Rotate right",     [](TankState& s) {s.leftMotor(50);s.rightMotor(0);}),
  Command("4", "Rotate left",      [](TankState& s) {s.leftMotor(0);s.rightMotor(50);}),
  Command("8", "Full forward",     [](TankState& s) {s.leftMotor(99); s.rightMotor(99);}),
  Command("2", "Slow backward",    [](TankState& s) {s.leftMotor(-25);s.rightMotor(-25);})
};

int Commands::numCommands() const { return sizeof(commands)/sizeof(commands[0]); }

class CommandListener {
  private:
    String partialCommandCode;
    const IO& io;
    static const char COMMAND_SEPARATOR = ';';
    static const int MAX_COMMAND_LENGTH = 16;

  public:
    CommandListener(const IO& io): io(io) {
    }

    String maybeCommand() {
      return io.getData(MAX_COMMAND_LENGTH, COMMAND_SEPARATOR);
    }
};

TankState state;
IO io;
CommandListener listener = CommandListener(io);
Commands commands;

void setup() {
  io.init();
  io.log(commands.commandDescriptions());
  io.log(">");
}

void loop(){
  commands.findByCode(listener.maybeCommand()).apply(state);
  state.sync();
}
