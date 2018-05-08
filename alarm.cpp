#include <Key.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

int countdownTime = 10;
int pirSensor = 3;
int buzzerPin = 0;
//int greenPin = 3;
//int redPin = 1;
byte rowPins[4] = {13, 15, 16, 1};
byte colPins[3] = {2, 14, 12};
//OLED SCL/SDA pins: D1, D2
unsigned char nullarray[4];

unsigned char keys[4][3] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

Adafruit_SSD1306 display;
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 3);

unsigned char currentPassword[4] = {'0', '0', '0', '0'};
int lastBan = 0;
void setup(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("cagriari.com");
  display.setTextSize(2);
  display.setCursor(0,8);
  display.println("Loading...");
  display.display();
  delay(1000);
  pinMode(pirSensor, INPUT);
  pinMode(buzzerPin, OUTPUT);
 // pinMode(redPin, OUTPUT);
 // pinMode(greenPin, OUTPUT);

  keypad.setDebounceTime(50);

  //digitalWrite(redPin, HIGH);
  //digitalWrite(greenPin, HIGH);

  
  unsigned char eepromPassword[4] = {EEPROM.read(0), EEPROM.read(1), EEPROM.read(2), EEPROM.read(3)};
  
  //if EEPROM password is set & valid, use it
  //if not, use default password 0000
  bool eepromOK = (eepromPassword[0] != NULL);
  for(int i = 0;i < 4;i++){
    if(isStrDigit(eepromPassword[i]) || isStrDigit(eepromPassword[i])){
      eepromOK = false;
    }
  }

  if(eepromOK)
    memcpy(currentPassword, eepromPassword, 4);
}

bool isStrDigit(unsigned char data){
   for(int i = 0;i < 10;i++){
     if(String(data) == String(i))
       return true;
   }
   
   return false;
}

bool alarmActive = false;
bool countingDown = false;
int failCount = 0;
unsigned long countdownStarted = 0;
bool changingPassword = false;
bool passwordConfirm = false;
bool passwordSetting = false;
void loop(){
  display.clearDisplay();
   display.setTextSize(1);
   display.setCursor(0, 0);
  if(alarmActive){
    display.println("ALARM ACTIVE\n\n\nEnter password now!");
    digitalWrite(buzzerPin, HIGH);
  } else if(countingDown) {
    display.clearDisplay();
    display.println(String((countdownStarted / 1000 + countdownTime) - millis() / 1000) + "s left");
    display.setTextSize(1);
    display.setCursor(0, 24);
    display.println("Enter password!");
  } else {
    digitalWrite(buzzerPin, LOW);
    display.println("Waiting..");
  }


  if(changingPassword){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println((passwordConfirm ? "Current password:" : "New password:"));
  }

  //if the alarm is not already triggered & active, start counting down
  if(digitalRead(pirSensor) == HIGH && !alarmActive && !countingDown){
    countingDown = true;
    countdownStarted = millis();
  }

  //if countdown is over, activate alarm
  alarmActive = countingDown && millis() - countdownStarted >= countdownTime * 1000;

  listenKeys();

  display.display();
  int currentBan = millis() - (millis() % 30);
  if(lastBan != currentBan)
    failCount = 0;
  lastBan = currentBan;
  delay(10);
}
unsigned char enteredPassword[4] = {NULL, NULL, NULL, NULL};
unsigned char changePasswordArray[4] = {'*', '*', NULL, NULL};
void listenKeys(){
  unsigned char key = keypad.getKey();
  Serial.println(key);
  if(key == NO_KEY){//(alarmActive || countingDown || changingPassword) && key == NO_KEY){
    display.setTextSize(2);
    display.setCursor(0, 8);
    for(int i = 0;i < 4; i++)
    display.print((enteredPassword[i] != NULL ? "* " : "_ "));
    display.display();
    return;
  }
  if(key == NO_KEY)
    return;
  if(key == '#'){
    changingPassword = false;
    passwordConfirm = false;
    passwordSetting = false;
    memcpy(enteredPassword, nullarray, 4);
    return;
  }

  int digitsEntered = 0;
  display.setTextSize(2);
  display.setCursor(0, 8);
  
  for(int i = 0;i < 4; i++){
    if(enteredPassword[i] != NULL)
      digitsEntered++;
  }
  enteredPassword[digitsEntered] = (unsigned char)key;
  for(int i = 0;i < 4; i++)
    display.print((enteredPassword[i] != NULL ? "* " : "_ "));
    
  digitsEntered++;


  bool resetPasswordChange = false;
  if(digitsEntered == 4){
    bool resetPasswordChange = true;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 8);
    if(passwordSetting){
      changingPassword = false;
      passwordSetting = false;
      setNewPassword(enteredPassword);
      failCount = 0;
      memcpy(enteredPassword, nullarray, 4);
      return;
    }

    if(failCount > 3){
      display.println("BLOCKED");
      display.setTextSize(1);
      display.println("Too many tries, " + String(millis() % 30) + "s left");
    } else if(compareArrays(enteredPassword, currentPassword)){
      if(changingPassword && passwordConfirm){
        resetPasswordChange = false;
        passwordConfirm = false;
        passwordSetting = true;
        memcpy(enteredPassword, nullarray, 4);
        return;
      } else {
        display.println("CORRECT");
        //digitalWrite(greenPin, HIGH);
        display.display();
        delay(1000);
        //digitalWrite(greenPin, LOW);
      }
      alarmActive = false;
      countingDown = false;
    } else {
      failCount++;
      display.println("WRONG");
      //digitalWrite(redPin, HIGH);
      display.display();
      delay(1000);
      //digitalWrite(redPin, LOW);
    }
    memcpy(enteredPassword, nullarray, 4);
  }

  if(resetPasswordChange){
    changingPassword = false;
    passwordConfirm = false;
    passwordSetting = false;
  }

  if(compareArrays(enteredPassword, changePasswordArray) && !changingPassword){
    memcpy(enteredPassword, nullarray, 4);
    changingPassword = true;
    passwordConfirm = true;
    passwordSetting = false;
  }
}

bool compareArrays(unsigned char firstArr[4], unsigned char secondArr[4]){
  for(int i = 0; i < 4; i++){
    if(firstArr[i] != secondArr[i])
      return false;
  }
  return true;
}

void setNewPassword(unsigned char newPassword[4]){
  for(int i = 0;i < 4;i++)
    EEPROM.write(i, newPassword[i]);
  memcpy(currentPassword, newPassword, 4);
}
