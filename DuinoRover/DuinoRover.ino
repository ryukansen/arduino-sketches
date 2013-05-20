/*
Arduino powered autonomous robot with bluetooth manual control mode in addition to the autonomous mode
 
 */

#include <AFMotor.h>
#include <SoftwareSerial.h>
#include <NewPing.h>

#define LOG_ENABLED true

#define BT_TX_PIN 3
#define BT_RX_PIN 10

#define TRIGGER_PIN  14  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     15  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 500 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

#define MODERATE_SPEED 200
#define REVERSE_SPEED  200
#define TURNING_SPEED 255
#define TOP_SPEED  255
#define MIN_SPEED  125

NewPing forwardSonar(14,15, MAX_DISTANCE); // Forward sonar
NewPing leftSonar(16,17, MAX_DISTANCE); // Left sonar
NewPing rightSonar(18,19, MAX_DISTANCE);  // Right sonar

AF_DCMotor leftMotor(3);
AF_DCMotor rightMotor(4);

#define MOVING_AVG_SAMPLES 3
int forwardDistances[MOVING_AVG_SAMPLES] = {
  -1, -1, -1};
int forwardDistanceIndex = 0;

enum motion{
  stateForward,stateReverse,stateLeftTurn,stateRightTurn,stateStopped};
motion _motion;

SoftwareSerial BTSerial(BT_RX_PIN, BT_TX_PIN);

void setup()
{
  if(LOG_ENABLED) Serial.begin(115200);
  BTSerial.begin(9600);
  forward();
  delay(500);
  _motion = stateStopped;
}


int prevDistance = 0;
int currentSpeed = 0;


long reverseStartTime = -1;
int reversingTime;

long turnStartTime = -1;
long turnTime;

long forwardDistanceStartTime = -1;

boolean autoMode = true;
void loop()
{
  char ch;
  if (BTSerial.available() > 0){
    ch = BTSerial.read();
    if(LOG_ENABLED){
      Serial.println(ch);
    }
    if(ch == 'a'){
      brake();
      _motion = stateStopped;
      autoMode = !autoMode; 
      if(LOG_ENABLED){
        Serial.print("Auto mode: ");
        Serial.println(autoMode);
      }
    }
  }
  if(autoMode){
    runAutoMode();
  } 
  else {
    runManualMode(ch);
  }
}

void runManualMode(char ch){
  switch (ch) {
  case 'U': // up
    if(LOG_ENABLED) Serial.println("Moving forward...");
    _motion = stateForward;
    forward();
    break;
  case 'D': // down
    if(LOG_ENABLED) Serial.println("Moving backward...");
    _motion = stateReverse;
    reverse();
    break;
  case 'L': // left
    if(LOG_ENABLED) Serial.println("Turning left...");
    _motion = stateLeftTurn;
    turnLeft();
    break;
  case 'R': // right
    if(LOG_ENABLED) Serial.println("Turning right...");
    _motion = stateRightTurn;
    turnRight();
    break;
  case 'C':
    if(LOG_ENABLED) Serial.println("Stopped");
    _motion = stateStopped;
    brake();
    break;
  default:
    break;
  }
}

float _forwardDistance;

boolean _stateChanged;

void runAutoMode(){
  switch(_motion){
  case stateForward:
    { 
      /*forwardDistances[forwardDistanceIndex] = getDistance(forwardSonar);
       forwardDistanceIndex++;
       if(forwardDistanceIndex > MOVING_AVG_SAMPLES - 1){
       forwardDistanceIndex = 0;
       }
       float forwardDistance = getMovingAverage();*/

      //float forwardDistance;// = getDistance(forwardSonar);
      if(millis() - forwardDistanceStartTime >= 30){
        _forwardDistance = getDistance(forwardSonar);
        //if(LOG_ENABLED) Serial.println("====================================================");
        forwardDistanceStartTime = millis();
        if(_forwardDistance == 0) {
          _forwardDistance = 20;
        }
      }

      // Move the Rover
      if(prevDistance == -1){
        prevDistance = _forwardDistance;
      }  
      if(_forwardDistance > 10){
        _motion = stateForward;
        if(_stateChanged){
          if(LOG_ENABLED){
            Serial.print("Forward distance: ");
            Serial.println(_forwardDistance);
          }
          forward();
          _stateChanged = false;
        }
        //prevDistance = forwardDistance;
      } 
      else {
        brake();
        _motion = stateReverse;
        _stateChanged = true;
        reversingTime = random(600);
        reverseStartTime = millis();
      } 
      break;
    }
  case stateReverse: 
    {
      if(millis() - reverseStartTime < reversingTime){
        if(_stateChanged){
          if(LOG_ENABLED) Serial.println("Reversing...");
          reverse(); // continue reversing 
          _stateChanged = false;
        }
      } 
      else {  
        brake();
        turnTime = random(600);
        turnStartTime = millis();
        makeTurnDecision();
      }
      break;
    }
  case stateLeftTurn:
    {
      if(millis() - turnStartTime < turnTime){
        if(_stateChanged){
          if(LOG_ENABLED) Serial.println("Turning left...");
          turnLeft();
          _stateChanged = false;
        }
      } 
      else {
        _forwardDistance = getDistance(forwardSonar);
        if(_forwardDistance > 10){
          brake();
          _motion = stateForward;
          _stateChanged = true;
          forwardDistanceStartTime = millis();
        } 
        else {
          turnTime = random(500);
          turnStartTime = millis();
        }
      }
      break;
    }
  case stateRightTurn:
    {
      if(millis() - turnStartTime < turnTime){
        if(_stateChanged){
          if(LOG_ENABLED) Serial.println("Turning right...");
          turnRight();
          _stateChanged = false;
        }
      } 
      else {
        _forwardDistance = getDistance(forwardSonar);
        if(_forwardDistance > 10){
          brake();
          _motion = stateForward;
          _stateChanged = true;
          forwardDistanceStartTime = millis();
        } 
        else {
          turnTime = random(500);
          turnStartTime = millis();
        }
      }
      break;
    }
  case stateStopped: 
    {
      if(LOG_ENABLED) Serial.println("Stopped");
      _motion = stateForward;
      _stateChanged = true;
      _forwardDistance = forwardSonar.ping_median(10)/US_ROUNDTRIP_CM;
      forwardDistanceStartTime = millis();
      break;
    }
  default:
    break;
  }
}

float getMovingAverage(){
  float total;
  int ommittedPositions = 0;
  for(int i = 0; i < MOVING_AVG_SAMPLES; i++){
    if(forwardDistances[i] != -1){
      total += forwardDistances[i];
    } 
    else {
      ommittedPositions++;
    }
  }
  return total/(MOVING_AVG_SAMPLES-ommittedPositions);
}

void makeTurnDecision(){
  float leftDistance = getDistance(leftSonar);
  float rightDistance = getDistance(rightSonar);

  if(LOG_ENABLED){
    Serial.print("-------- Left distance:"); 
    Serial.println(leftDistance);
    Serial.print("++++++++ Right distance:"); 
    Serial.println(rightDistance);
  }

  leftDistance = (leftDistance != 0) ? leftDistance : 15;
  rightDistance = (rightDistance != 0) ? rightDistance : 15;

  // Decide whether to turn right or left
  if(leftDistance > rightDistance){
    _motion = stateLeftTurn;
  } 
  else if (leftDistance < rightDistance){
    _motion = stateRightTurn;
  } 
  else { // Random turn
    if (random(2) == 0) {
      _motion = stateLeftTurn;
    } 
    else {
      _motion = stateRightTurn;
    }
  }
  _stateChanged = true;
}

float getDistance(NewPing &sonar){
  //return sonar.ping_median(3)/US_ROUNDTRIP_CM;
  return sonar.ping_cm();
}

void randomTurn(){
  if (random(2) == 0) {
    turnLeft();
  } 
  else {
    turnRight(); 
  }
}

void forward(){
  leftMotor.setSpeed(TOP_SPEED);
  rightMotor.setSpeed(TOP_SPEED);
  leftMotor.run(FORWARD);
  rightMotor.run(FORWARD); 
}

void forward(int currentDistance){
  // start slow and increase speed
  Serial.println("Forward...");
  /*if(currentSpeed == 0){
   currentSpeed = MIN_SPEED; 
   }
   if(currentDistance > 40){ // increase speed
   if(currentSpeed < MODERATE_SPEED){
   if(currentSpeed < MIN_SPEED + 10){
   currentSpeed += 5; 
   } 
   else {
   currentSpeed += 2;
   }
   }
   // currentSpeed = MODERATE_SPEED;
   } 
   else { // reduce speed
   if(currentSpeed > MIN_SPEED){
   currentSpeed -= 10;
   }
   }*/

  // TODO: temporarilly setting to max speed
  currentSpeed = TOP_SPEED;
  if(LOG_ENABLED){
    Serial.print("Current speed: ");
    Serial.println(currentSpeed);
  }

  leftMotor.setSpeed(currentSpeed);
  rightMotor.setSpeed(currentSpeed);
  leftMotor.run(FORWARD);
  rightMotor.run(FORWARD); 
}

void reverse(){
  leftMotor.setSpeed(REVERSE_SPEED);
  rightMotor.setSpeed(REVERSE_SPEED);
  leftMotor.run(BACKWARD);
  rightMotor.run(BACKWARD);
}

void brake(){
  leftMotor.setSpeed(0);
  rightMotor.setSpeed(0);
  leftMotor.run(BRAKE);
  rightMotor.run(BRAKE);
  currentSpeed = 0;
}

void turnLeft(){
  turn(leftMotor, rightMotor);
}

void turnRight(){
  turn(rightMotor, leftMotor);
}

void turn(AF_DCMotor &motor1, AF_DCMotor &motor2){
  motor1.setSpeed(TURNING_SPEED);
  motor2.setSpeed(TURNING_SPEED - 50);
  motor1.run(FORWARD);
  motor2.run(BACKWARD);
}



















































