// Markku Asikainen 250470  &   Arttu Räty 2434991

/*
 *  ======== main.c ========
 */
 
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Header files */
#include <ti/drivers/I2C.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>

/* Board Header files */
#include "Board.h"

/* jtkj Header files */
#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h" 

/* Task Stacks */
#define STACKSIZE 4096
Char labTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];

//game functions
void updateChar();
void updateHighscore();

//display functions
void gameOver();
void createChar();
void createObstacles();


/* jtkj: Display */
Display_Handle hDisplay;

//variables
float ax, ay, az, gx, gy, gz; //raw data
float left_tilt, right_tilt, up_tilt, down_tilt;
uint16_t highscores[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint16_t score = 0;
uint8_t roadBuffer[5];
int press = 0;
int choice = 1;
char gameRXMsg[9];
int jumps = 10;


//states
//basic game state
enum state {MENU, GAME};
enum state gameState = MENU; 
//char status
enum status {ALIVE = 1, DEAD = 0};
enum status charStatus = DEAD;
//obstacles as bitmasks
enum obstacles {RIGHT_BONUS = 0x01, RIGHT_SIDE = 0x02, RIGHT_MOVING = 0x04, RIGHT_STATIC = 0x08, LEFT_STATIC = 0x10, LEFT_MOVING = 0x20, LEFT_SIDE = 0x40, LEFT_BONUS = 0x80};
//character position left/right
enum charState {LEFT = 0, RIGHT};
enum charState charPos = LEFT;
//character position up/down
enum jump {UP, DOWN};
enum jump jumpState = DOWN; 
//boolean that checks if there is new part coming
enum boolean {BOOL1, BOOL0};
enum boolean newPart;


/* jtkj: Pin Button1 configured as power button */
static PIN_Handle hPowerButton;
static PIN_State sPowerButton;
PIN_Config cPowerButton[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};
PIN_Config cPowerWake[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE
};

/* jtkj: Pin Button0 configured as input */
static PIN_Handle hButton0;
static PIN_State sButton0;
PIN_Config cButton0[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

/* jtkj: Leds */
static PIN_Handle hLed;
static PIN_State sLed;
PIN_Config cLed[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
/*MPU global variables*/
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};    
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

/* jtkj: Handle power button */
Void powerButtonFxn(PIN_Handle handle, PIN_Id pinId) {

    Display_clear(hDisplay);
    Display_close(hDisplay);
    Task_sleep(100000 / Clock_tickPeriod);

	PIN_close(hPowerButton);

    PINCC26XX_setWakeup(cPowerWake);
	Power_shutdown(NULL,0);
}

/* JTKJ: HERE YOUR HANDLER FOR BUTTON0 PRESS */
Void Button0Fxn(PIN_Handle handle, PIN_Id pinId) {
  
    char payload[8];
    press = 1;
    if (gameState == GAME){
        sprintf(payload,"Pts: %d", score); 
        Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));
        PIN_setOutputValue( hLed, Board_LED0, !PIN_getOutputValue( Board_LED0 ) );
        StartReceive6LoWPAN();
        press = 0;    
    }
}



//updates character position on screen and adds bonus scores
void updateChar(){
    if(charStatus == ALIVE){
        if(jumpState == UP){ //while character is jumping
                if(charPos == LEFT){
                    if(roadBuffer[4] & LEFT_STATIC || roadBuffer[4] & LEFT_MOVING ){
                        jumps--;
                    }
                    else if(roadBuffer[4] & LEFT_BONUS){
                        score = score + 5;
                    }
                }
                else if (charPos == RIGHT){
                    if(roadBuffer[4] & RIGHT_STATIC || roadBuffer[4] & RIGHT_MOVING){
                        jumps--;
                    }
                    else if(roadBuffer[4] & RIGHT_BONUS){
                        score = score + 5;
                    }
                }
            }
        if(jumpState == DOWN){ //while character is down
            if(charPos == LEFT){
                if(roadBuffer[4] & LEFT_STATIC || roadBuffer[4] & LEFT_MOVING ){
                    charStatus = DEAD;
                    jumps = 10;
                }
                else if(roadBuffer[4] & LEFT_BONUS){
                    score = score + 5;
                    jumps++;
                }
            }
            else if (charPos == RIGHT){
                if(roadBuffer[4] & RIGHT_STATIC || roadBuffer[4] & RIGHT_MOVING){
                    charStatus = DEAD;
                    jumps = 10;
                }
                else if(roadBuffer[4] & RIGHT_BONUS){
                    score = score + 5;
                    jumps++;
                }
            }
        }
    }
}


//updates highscore into list
void updateHighscore(){
    uint32_t i;
    uint32_t j;
    for(i = 0; i < 10; i++) {
        if(score > highscores[i]){
            for(j = 9; j > i; j--){
                highscores[j] = highscores[j-1];
            }
        }
        highscores[j] = score;
        return;
    }
}


//prints gameover screen
void gameOver(){
    char endscore[8];
    Display_print0(hDisplay, 1, 1, "GAME OVER");
    Display_print0(hDisplay, 2, 1, "END SCORE:");
    sprintf(endscore, "%d", score);
    Display_print0(hDisplay, 3, 1, endscore);
    score = 0;
}


//prints character on screen
void createChar(){
    if(charPos == LEFT){
        if(jumpState == DOWN){
            Display_print0(hDisplay, 5, 2, "    _  ");
            Display_print0(hDisplay, 6, 2, "  _| |_");
            Display_print0(hDisplay, 7, 2, " Q(O_o)");
        }
        else if(jumpState == UP) {
                Display_print0(hDisplay, 5, 2, "Q(@_@)");
            }
        }
    else {
        if(jumpState == DOWN){
            Display_print0(hDisplay, 5, 7, "    _  ");
            Display_print0(hDisplay, 6, 7, "  _| |_");
            Display_print0(hDisplay, 7, 7, " Q(o_O)");
        }
        else if(jumpState == UP) {
            Display_print0(hDisplay, 5, 7, "Q(@_@)");
        }
    }
}


//prints obstacles/bonuses on screen and displays received messages
void createObstacles(){
    char msg[14];
    uint8_t k;
    uint8_t s = 1;
    for (k = 0; k < 5; k++){
        sprintf(msg, "%s", gameRXMsg);
        Display_print0(hDisplay, 11, 0, msg);
        
        if(roadBuffer[k] & LEFT_BONUS){
            Display_print0(hDisplay, 1+k, 2, " (@) ");
        }
        if(roadBuffer[k] & LEFT_SIDE){
            Display_print0(hDisplay, 1+k, 1, "--");
            roadBuffer[k+1] = ((roadBuffer[k] & 0xBF) | 0x20);
        }
        if(roadBuffer[k] & LEFT_MOVING){
            Display_print0(hDisplay, 1+k, 2, "---");
            roadBuffer[k+1] = ((roadBuffer[k] & 0xDF) | 0x10); 
        }
        if(roadBuffer[k] & LEFT_STATIC){
            Display_print0(hDisplay, 1+k, 2, "----");
        }
        if(roadBuffer[k] & RIGHT_STATIC){
            Display_print0(hDisplay, 1+k, 10, "----");            
        }
        if(roadBuffer[k] & RIGHT_MOVING){
            Display_print0(hDisplay, 1+k, 10, " ---");
            roadBuffer[k+1] = ((roadBuffer[k] & 0xFB) | 0x08);
        }
        if(roadBuffer[k] & RIGHT_SIDE){
            Display_print0(hDisplay, 1+k, 14, " --");
            roadBuffer[k+1] = ((roadBuffer[k] & 0xFD) | 0x04);
        }
        if(roadBuffer[k] & RIGHT_BONUS){
            Display_print0(hDisplay, 1+k, 10, " (@) ");
        }
    }
}    
    
    
    
/* jtkj: Communication Task */
Void commTask(UArg arg0, UArg arg1) {
    char payload[16];
    uint16_t senderAddr;
    uint32_t c;
    
    while (gameState == MENU) {
        ;   //at the beginning, do nothing while in menu
        }


    // Radio to receive mode
	int32_t result = StartReceive6LoWPAN();
	if(result != true) {
		System_abort("Wireless receive mode failed");
	}
	
    StartReceive6LoWPAN();
    while (1) {
        if (GetRXFlag() == true) {
            memset(payload,0,16);
            Receive6LoWPAN(&senderAddr, payload, 16);
            System_printf(payload);
            System_flush();
            if(gameState == GAME){ //get payload from received message
                for (c = 0; c < 8; c++){
                    gameRXMsg[c] = payload[c+1];
                }
                for (c = 4; c > 0; c--){
                    roadBuffer[c] = roadBuffer[c-1];
                }
                roadBuffer[0] = payload[0];
        		newPart = BOOL1;
        		score++; //increase gamescore
            }
            else if(gameState == MENU){
                    for (c = 4; c > 0; c--){ // empty buffer if in menu
                        roadBuffer[c] = 0;
                    }
                }
            }
        }

    	// COMMUNICATION WHILE LOOP DOES NOT USE Task_sleep
    	// (It has lower priority than main loop)
}




/* JTKJ: laboratory exercise task */
Void labTask(UArg arg0, UArg arg1) {
    
    char str[80];
 //   double paine, lampo;
    float ax, ay, az, gx, gy, gz;

    I2C_Handle      i2c;
    I2C_Params      i2cParams;
	I2C_Handle i2cMPU;
	I2C_Params i2cMPUParams;

    /* jtkj: Create I2C for usage */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    /*
    i2c = I2C_open(Board_I2C0, &i2cParams); //for other sensors
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    */
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }
    
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // WAIT 100MS FOR THE SENSOR TO POWER UP
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    
    
    // JTKJ: SETUP BMP280 SENSOR HERE
    /*bmp280_setup(&i2c);*/
    mpu9250_setup(&i2cMPU);


    /* jtkj: Init Display */
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL) {
        System_abort("Error initializing Display\n");
    }

    /* jtkj: Check that Display works */
    Display_clear(hDisplay);
    uint16_t address = IEEE80154_MY_ADDR;
    sprintf(str, "%d", address);
    Display_print0(hDisplay, 5, 1, str);
    Display_clear(hDisplay);


    // jtkj: main loop
    while (1) {
        

    	mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);


        //menu choice
        if (abs(gx) > 50.0) {
            choice++;
            if (choice > 3) {
                choice = 1;
            } 
        } 
        
        
        //menu
        switch (choice) {
            case 1: //game
                gameState = MENU;
                Display_print0(hDisplay, 1, 1, "=>GAME");
                Display_print0(hDisplay, 2, 1, "  HIGHSCORE");
                Display_print0(hDisplay, 3, 1, "  CALIBRATION");
                if (press == 1) {
                    press = 0;
                    gameState = GAME; //starts game
                    charStatus = ALIVE; //changes character state to alive
                    while (gameState == GAME) {
                        if (charStatus == ALIVE) { //while character is alive
                            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
                            while (newPart == BOOL0){ //no new road part incoming
                                Task_sleep(50000 / Clock_tickPeriod);
                            }
                            jumpState = DOWN;
                            newPart = BOOL0;
                            Display_clear(hDisplay);
                            //these if-else statements move character around the screen
                            if(ax <= left_tilt){
                                charPos = LEFT;
                            }
                            else if (ax >= right_tilt){
                                charPos = RIGHT;
                            }
                            if (jumps > 0){ //if player has jumps left
                                if(ay > down_tilt || ay < up_tilt){
                                jumpState = UP;
                                } 
                            }
                            //functions for gamelogic and display
                            createObstacles();
                            updateChar();
                            createChar();
                            System_flush();
                        } //shows gameover -screen when character dies
                            else if (charStatus == DEAD){
                                updateHighscore();
                                Display_clear(hDisplay);
                                gameOver();
                                System_flush();
                                gameState = MENU;
                                Task_sleep(3000000 / Clock_tickPeriod);
                            }
                    }
                }
                break;    
            case 2: //highscore
                gameState = MENU;
                Display_print0(hDisplay, 1, 1, "  GAME");
                Display_print0(hDisplay, 2, 1, "=>HIGHSCORE");
                Display_print0(hDisplay, 3, 1, "  CALIBRATION");
                if (press == 1) {
                    press = 0;
                    Display_clear(hDisplay);
                    char text[17];
                    Display_print0(hDisplay, 1, 1, "HIGHSCORES");
                    uint32_t s;
                    for (s = 0; s < 10; s++){ //print highscores into screen
                        sprintf(text, "%d", highscores[s]);
                        Display_print0(hDisplay, s+2 , 1, text);
                    }
            		Task_sleep(3000000 / Clock_tickPeriod);
            		Display_clear(hDisplay);
            		System_flush();
                }
                break;
            case 3: //calibration
                gameState = MENU;
                Display_print0(hDisplay, 1, 1, "  GAME");
                Display_print0(hDisplay, 2, 1, "  HIGHSCORE");
                Display_print0(hDisplay, 3, 1, "=>CALIBRATION");
                if (press == 1) {
                    press = 0;
                    float ax_values[100];
                    float ay_values[100];
                    float ay_zero[100];
                    float ax_zero[100];
                    
                    Display_clear(hDisplay);
                    uint8_t time = 0; //calibration timer
                    //calibrate zero values
                    Display_print0(hDisplay, 1, 1, "Keep it STEADY");
                    Task_sleep(2000000 / Clock_tickPeriod);
                    while(time <= 100){
                        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);         
                        
                        
                        sprintf(str, "Ay: %f\n", ay);
                        System_printf(str);
                        Display_print0(hDisplay, 2, 1, str);                        
                        System_flush();
                        
                        sprintf(str, "Ax: %f\n", ax);
                        System_printf(str);
                        Display_print0(hDisplay, 3, 1, str);
                        System_flush();

                            
                        ay_zero[time] = ay;
                        ax_zero[time] = ax;
                            
                        time += 1;
                    }
                    time = 0;
                    Display_clear(hDisplay);
                    
                    float ay_zero_avg = ay_zero[0];
                    float ax_zero_avg = ax_zero[0];
                    uint8_t i;
                    //calculate averages for zero position
                    for (i=0; i <= 100; i++){
                        ay_zero_avg = ay_zero_avg + abs(ay_zero[i]);
                        ax_zero_avg = ax_zero_avg + abs(ax_zero[i]);
                    }
                    i = 0;
                    Display_print0(hDisplay, 1, 1, "Tilt DOWN");
                    Task_sleep(2000000 / Clock_tickPeriod);
                    //calibrate Y-axis values
                    while(time <= 100){
                        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);         
                        
                    	sprintf(str, "Ay: %f\n", ay);
                        System_printf(str);
                        Display_print0(hDisplay, 2, 1, str);
                        System_flush(); 
                        
                        ay_values[time] = ay;
                        
                        time += 1;
                    }
                    time = 0;
                    Display_clear(hDisplay);
                    
                    Display_print0(hDisplay, 1, 1, "Tilt RIGHT");
                    Task_sleep(2000000 / Clock_tickPeriod);
                    //calibrate X-axis values
                    while(time <= 100){
                        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz); 
                        
                    	sprintf(str, "Ax: %f\n", ax);
                        System_printf(str);
                        Display_print0(hDisplay, 2, 1, str);  
                        System_flush();

                        
                        ax_values[time] = ax;
                        
                        time += 1;
                    }
                    Display_clear(hDisplay);
                    //averages for down / right tilt
                    float ay_avg = ay_values[0];
                    float ax_avg = ax_values[0];
                    
                    for (i=0; i <= 100; i++){
                        ay_avg = ay_avg + ay_values[i];
                        ax_avg = ax_avg + ax_values[i];
                    }
                    ay_avg = ay_avg / 101;
                    ax_avg = ax_avg / 101;
                    //calculate tilt thresholds from average values
                    left_tilt = ax_zero_avg - ax_avg;
                    right_tilt = ax_avg;
                    up_tilt = ay_zero_avg - ay_avg;
                    down_tilt = ay_avg;
                    
                    sprintf(str,"right tilt: %f\ndown tilt: %f\n left tilt: %f\n up tilt: %f\n", right_tilt, down_tilt, left_tilt, up_tilt);
                    System_printf(str);
                    Display_print0(hDisplay, 1, 1, "Calibration OK!");
                    Task_sleep(2000000 / Clock_tickPeriod);
                    System_flush();
                }
                break;
        }
        
        Task_sleep(100000 / Clock_tickPeriod);  
    }
}

Int main(void) {

    // Task variables
	Task_Handle hLabTask;
	Task_Params labTaskParams;
	Task_Handle hCommTask;
	Task_Params commTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();

	/* jtkj: Power Button */
	hPowerButton = PIN_open(&sPowerButton, cPowerButton);
	if(!hPowerButton) {
		System_abort("Error initializing power button shut pins\n");
	}
	if (PIN_registerIntCb(hPowerButton, &powerButtonFxn) != 0) {
		System_abort("Error registering power button callback function");
	}

    // JTKJ: INITIALIZE BUTTON0 HERE
    hButton0 = PIN_open(&sButton0, cButton0);
    	if(!hButton0) {
		System_abort("Error initializing power button shut pins\n");
	}
	if (PIN_registerIntCb(hButton0, &Button0Fxn) != 0) {
		System_abort("Error registering power button callback function");
	}
    /* jtkj: Init Leds */
    hLed = PIN_open(&sLed, cLed);
    if(!hLed) {
        System_abort("Error initializing LED pin\n");
    }

    /* jtkj: Init Main Task */
    Task_Params_init(&labTaskParams);
    labTaskParams.stackSize = STACKSIZE;
    labTaskParams.stack = &labTaskStack;
    labTaskParams.priority=2;

    hLabTask = Task_create(labTask, &labTaskParams, NULL);
    if (hLabTask == NULL) {
    	System_abort("Task create failed!");
    }

    /* jtkj: Init Communication Task */
    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;

    Init6LoWPAN();
    
    hCommTask = Task_create(commTask, &commTaskParams, NULL);
    if (hCommTask == NULL) {
    	System_abort("Task create failed!");
    }

    // jtkj: Send OK to console
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

