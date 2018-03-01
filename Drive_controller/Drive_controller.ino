class LED {
  private:
    int pin;
    int brightness;

  public:
    LED(int pin) {
      this->pin = pin;
      this->brightness = 0;

      init();
    }

    // Speed, [-99..0..99]. 0 means the motor is stopped
    void setBrightness(int brightness){
      this->brightness = brightness > 99 ? 99 : (brightness < -99 ? -99 : brightness);
    }

    int getBrightness() const { return this->brightness; }

    void sync() {
      int brightness = this->brightness * 255 / 99;
      analogWrite(pin, brightness);
    }

  private:
    void init(){
       pinMode(pin, OUTPUT);
    }
};

class Motor {
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

    // Speed, [-99..0..99]. 0 means the motor is stopped
    void setSpeed(int speed){
      this->speed = speed > 99 ? 99 : (speed < -99 ? -99 : speed);
    }

    int getSpeed() const { return this->speed; }

    void sync() {
      int pwm = this->speed * 255 / 99;

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
        "Motor(L=" + String(motorLeft.getSpeed()) + "," +
              "R=" + String(motorRight.getSpeed()) +
        ")"
      );
      Serial.println(
        "LED(LF=" + String(ledLeftFront.getBrightness()) + "," +
            "RF=" + String(ledRightFront.getBrightness()) + "," +
            "LR=" + String(ledLeftRear.getBrightness()) + "," +
            "RR=" + String(ledRightRear.getBrightness()) + "," +
        ")"
      );
    }

    void stop()
    {
      motorLeft.setSpeed(0);
      motorRight.setSpeed(0);
    }

    // speed [-99..99]
    void leftMotor(int speed) {
      motorLeft.setSpeed(speed);
    }

    void rightMotor(int speed) {
      motorRight.setSpeed(speed);
    }

};

class Command {
  public:
    static const char COMMAND_SEPARATOR = ';';

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

Command nopCommand = Command("",  "NOP",              [](TankState& s) {});
Command commands[] = {
  Command("5", "Stop",             [](TankState& s) {s.stop();}),
  Command("6", "Rotate right",     [](TankState& s) {s.leftMotor(50);s.rightMotor(0);}),
  Command("4", "Rotate left",      [](TankState& s) {s.leftMotor(0);s.rightMotor(50);}),
  Command("8", "Full forward",     [](TankState& s) {s.leftMotor(99); s.rightMotor(99);}),
  Command("2", "Slow backward",    [](TankState& s) {s.leftMotor(-25);s.rightMotor(-25);})
};

class IO {
  private:
    String partialCommandCode;

  public:
    void init(){
      Serial.begin(9600);
    }

    Command getCommand() {
      while(Serial.available())
      {
        char commandCodeChar = Serial.read();

        if (commandCodeChar == Command::COMMAND_SEPARATOR) {
          return parseCommand(partialCommandCode);
        }
        else {
          partialCommandCode += String(commandCodeChar);
        }
      }

      return nopCommand;
    }

    void log(String message = "") {
      Serial.println(message);
    }

  private:
    Command parseCommand(String commandCode){
      for(unsigned int i; i < sizeof(commands)/sizeof(commands[0]); i++) {
        const Command& currentCommand =  commands[i];
        if(commandCode == currentCommand.getCode()) {
          return currentCommand;
        }
      }

      return nopCommand;
    }
};

TankState state;
IO io;

void setup() {
  io.init();

  io.log(">");

  for(unsigned int i; i < sizeof(commands)/sizeof(commands[0]); i++) {
    const Command& command =  commands[i];
    io.log(command.getCode() + " " + command.getDescription());
  }

  io.log();
}

void loop(){
  io.getCommand().apply(state);
  state.sync();
}
