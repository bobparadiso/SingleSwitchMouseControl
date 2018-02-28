//runs on an Arduino Micro (for the mouse emulation support)
//uses an OLED display (128x64)
#include <Mouse.h>

#include <SPI.h>
#include <Wire.h>
#include <Servo.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SWITCH_PIN 9
#define ENABLE_PIN 10
#define LED_PIN 13

#define DEBOUNCE_THRESHOLD 20

Adafruit_SSD1306 display;

//state for button debounce
volatile uint32_t lastButtonTime;
volatile uint8_t lastButtonState;
volatile uint8_t debouncedButtonState = 0;
ISR(PCINT0_vect)
{
	uint32_t time = millis();
	uint8_t instantaneous = digitalRead(SWITCH_PIN) == LOW;
	
	//latch last state if applicable
	if (time - lastButtonTime > DEBOUNCE_THRESHOLD)
		debouncedButtonState = lastButtonState;
	
	lastButtonTime = time;
	lastButtonState = instantaneous;
}

//state is validated/updated at request time
uint8_t getDebouncedButtonState()
{
	if (millis() - lastButtonTime > DEBOUNCE_THRESHOLD)
		debouncedButtonState = lastButtonState;
	
	return debouncedButtonState;
}

//
void setupButton()
{
	pinMode(SWITCH_PIN, INPUT_PULLUP);
	delay(100);
	
	//init state
	lastButtonState = digitalRead(SWITCH_PIN) == LOW;
	lastButtonTime = 0;

	PCICR |= _BV(PCIE0); //enable pin change interrupts
	PCMSK0 = 0b00100000;
}

#define MOUSE_SPEED 1
#define LOOP_INTERVAL 7

//
#define DIR_CHANGE_TIME 300
#define LEFT_CLICK_TIME 2000
#define RIGHT_CLICK_TIME 3000
#define DOUBLE_CLICK_TIME 4000
enum DIR {STOP, RIGHT, DOWN, LEFT, UP, NUM_DIR};
DIR dir = STOP;

//
void displayCurrentDir()
{
	switch(dir)
	{
	case STOP: displayText("ready"); break;
	case RIGHT: displayText("RIGHT"); break;
	case DOWN: displayText("DOWN"); break;
	case LEFT: displayText("LEFT"); break;
	case UP: displayText("UP"); break;
	}
}

//
void updateMode1()
{
	switch(dir)
	{
	case RIGHT: Mouse.move(MOUSE_SPEED, 0); break;
	case DOWN: Mouse.move(0, MOUSE_SPEED); break;
	case LEFT: Mouse.move(-MOUSE_SPEED, 0); break;
	case UP: Mouse.move(0, -MOUSE_SPEED); break;
	}
	
	if (getDebouncedButtonState())
	{
		uint32_t startTime = millis();
		displayText("");
		uint32_t elapsed;
		while(getDebouncedButtonState())
		{
			elapsed = millis() - startTime;
			//remember to put these in descending time order
			if (elapsed > DOUBLE_CLICK_TIME)
				displayText("STOP");
			else if (elapsed > RIGHT_CLICK_TIME)
				displayText("Dbl\nClick");
			else if (elapsed > LEFT_CLICK_TIME)
				displayText("R\nClick");
			else if (elapsed > DIR_CHANGE_TIME)
				displayText("L\nClick");
		}
				
		//remember to put these in ascending time order
		if (elapsed < DIR_CHANGE_TIME)
		{
			dir = (dir + 1) % NUM_DIR;
			if (dir == STOP)
				dir = dir + 1;
			
			displayCurrentDir();
		}
		else if (elapsed < LEFT_CLICK_TIME)
		{
			Mouse.click(MOUSE_LEFT);
			dir = STOP;
			displayCurrentDir();
		}
		else if (elapsed < RIGHT_CLICK_TIME)
		{
			Mouse.click(MOUSE_RIGHT);
			dir = STOP;
			displayCurrentDir();
		}
		else if (elapsed < DOUBLE_CLICK_TIME)
		{
			Mouse.click(MOUSE_LEFT);
			delay(100);
			Mouse.click(MOUSE_LEFT);
			dir = STOP;
			displayCurrentDir();
		}
		else
		{
			dir = STOP;
			displayCurrentDir();
		}
	}
}

//
void initDisplay()
{
	//generate the high voltage from the 3.3v line internally
	//i2c addr 0x3C (<< 1 = 0x78)
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

	display.setTextSize(4);
	display.setTextColor(WHITE);
}

//
void displayText(char *str)
{
	display.clearDisplay();
	display.setCursor(0, 0);
	display.print(str);
	display.display();	
}

//
void setup()
{
	initDisplay();
	displayCurrentDir();
	
	pinMode(ENABLE_PIN, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);
	setupButton();

	Mouse.begin();
	
	while(1)
	{
		uint8_t enabled = !digitalRead(ENABLE_PIN);
		uint8_t pressed = getDebouncedButtonState();
		digitalWrite(LED_PIN, enabled && pressed);
		if (!enabled)
			continue;
		
		updateMode1();
		
		delay(LOOP_INTERVAL);
	}

}

//
void loop() {}
