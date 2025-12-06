#include <LittleFS.h>
#include "config.h"


// Check if Bluetooth configs are enabled
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Bluetooth Serial object
BluetoothSerial SerialBT;

// Watch objects
TTGOClass *watch;
TFT_eSPI *tft;
BMA *sensor;

uint32_t sessionId = 30;

volatile uint8_t state;
volatile bool irqBMA = false;
volatile bool irqButton = false;
volatile uint32_t steps = 0;
volatile float distance = 0.0f;

long updateTimeout = 0;
long last = 0;

bool sessionStored = false;
bool sessionSent = false;
bool displayActive = true;

void initHikeWatch()
{
    // LittleFS
    if(!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LITTLEFS Mount Failed");
        return;
    }

    // Stepcounter
    // Configure IMU
    // Enable BMA423 step count feature
    // Reset steps
    // Turn on step interrupt

    Acfg cfg;
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    cfg.range = BMA4_ACCEL_RANGE_2G;
    cfg.bandwidth = BMA4_ACCEL_OSR4_AVG1;
    cfg.perf_mode = BMA4_CONTINUOUS_MODE;

    // Configure the BMA423 accelerometer
    sensor->accelConfig(cfg);

    sensor->enableAccel();

    pinMode(BMA423_INT1, INPUT);
    attachInterrupt(BMA423_INT1, [] {
        // Set interrupt to set irq value to 1
        irqBMA = 1;
    }, RISING); //It must be a rising edge

    // Enable BMA423 step count feature
    sensor->enableFeature(BMA423_STEP_CNTR, true);

    // Reset steps
    sensor->resetStepCounter();

    // Turn on step interrupt
    sensor->enableStepCountInterrupt();

    pinMode(AXP202_INT, INPUT_PULLUP);
    attachInterrupt(AXP202_INT, [] {
        irqButton = true;
    }, FALLING);

    //!Clear IRQ unprocessed first
    watch->power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);
    watch->power->clearIRQ();

    return;
}

void sendDataBT(fs::FS &fs, const char * path)
{
    /* Sends data via SerialBT */
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }
    Serial.println("- read from file:");
    while(file.available()){
        SerialBT.write(file.read());
    }
    file.close();
}

void sendSessionBT()
{
    // Read session and send it via SerialBT
    watch->tft->fillRect(0, 0, 240, 240, TFT_BLACK);
    watch->tft->drawString("Sending session", 20, 80);
    watch->tft->drawString("to Hub", 80, 110);

    delay(1000);

    // Sending session id
    sendDataBT(LITTLEFS, "/id.txt");
    SerialBT.write(';');
    // Sending steps
    sendDataBT(LITTLEFS, "/steps.txt");
    SerialBT.write(';');
    // Sending distance
    sendDataBT(LITTLEFS, "/distance.txt");
    SerialBT.write(';');
    // Send connection termination char
    SerialBT.write('\n');
}


void saveIdToFile(uint16_t id)
{
    char buffer[10];
    itoa(id, buffer, 10);
    writeFile(LITTLEFS, "/id.txt", buffer);
}

void saveStepsToFile(uint32_t step_count)
{
    char buffer[10];
    itoa(step_count, buffer, 10);
    writeFile(LITTLEFS, "/steps.txt", buffer);
}

void saveDistanceToFile(float distance)
{
    char buffer[10];
    dtostrf(distance, 5, 2, buffer);
    writeFile(LITTLEFS, "/distance.txt", buffer);
}

void deleteSession()
{
    deleteFile(LITTLEFS, "/id.txt");
    deleteFile(LITTLEFS, "/distance.txt");
    deleteFile(LITTLEFS, "/steps.txt");
    deleteFile(LITTLEFS, "/coord.txt");
}

bool startButtonPressed()
{
    int16_t x, y;
    if(watch->getTouch(x,y)) 
    {
        if(x < 170 && x > 70 && y < 100 && y > 70) 
        {
            return true;
        }
    }
    return false;
}

bool sendButtonPressed()
{
    int16_t x, y;
    if(watch->getTouch(x,y)) 
    {
        if(x < 170 && x > 70 && y < 210 && y > 180) 
        {
            return true;
        }
    }
    return false;
}

void sleep()
{
     irqButton = false;
        watch->power->readIRQ();
        if(displayActive) 
        {
            watch->closeBL();
            watch->displaySleep();
            displayActive = false;
        }
        else 
        {
            watch->openBL();
            watch->displayWakeup();
            displayActive = true;
        }
        watch->power->clearIRQ();
}

void setup()
{
    Serial.begin(9600);
    watch = TTGOClass::getWatch();
    watch->begin();
    watch->openBL();

    //Receive objects for easy writing
    tft = watch->tft;
    sensor = watch->bma;
    
    initHikeWatch();

    state = 1;

    SerialBT.begin("Hiking Watch");
}

void loop()
{
    switch (state)
    {
        case 1:
        {
            /* Initial stage */
            //Basic interface
            watch->tft->fillScreen(TFT_BLACK);
            watch->tft->setTextFont(4);
            watch->tft->setTextColor(TFT_WHITE, TFT_BLACK);
            watch->tft->drawString("Hiking Watch",  45, 25, 4);
            // watch->tft->drawString("Press button", 50, 80);
            // watch->tft->drawString("to start session", 40, 110);
            watch->tft->drawRect(70, 70, 100, 30, TFT_WHITE);
            watch->tft->drawString("START", 80, 73);

            watch->tft->setTextFont(2);
            watch->tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            watch->tft->drawString("Send data via Bluetooth", 45, 155);
            watch->tft->setTextFont(4);
            watch->tft->setTextColor(TFT_WHITE, TFT_BLACK);
            watch->tft->drawRect(70, 180, 100, 30, TFT_WHITE);
            watch->tft->drawString("SEND", 90, 183);
            

            bool exitSync = false;

            //Bluetooth discovery
            while (1)
            {
                /* Bluetooth sync */
                if (SerialBT.available())
                {
                    char incomingChar = SerialBT.read();
                    if (incomingChar == 'c' and sessionStored and not sessionSent)
                    {
                        sendSessionBT();
                        sessionSent = true;
                    }

                    if (sessionSent && sessionStored) {
                        // Update timeout before blocking while
                        updateTimeout = 0;
                        last = millis();
                        while(1)
                        {
                            updateTimeout = millis();

                            if (SerialBT.available())
                                incomingChar = SerialBT.read();
                            if (incomingChar == 'r')
                            {
                                Serial.println("Got an R");
                                // Delete session
                                deleteSession();
                                sessionStored = false;
                                sessionSent = false;
                                incomingChar = 'q';
                                exitSync = true;
                                break;
                            }
                            else if ((millis() - updateTimeout > 2000))
                            {
                                Serial.println("Waiting for timeout to expire");
                                updateTimeout = millis();
                                sessionSent = false;
                                exitSync = true;
                                break;
                            }
                        }
                    }
                }
                if (exitSync)
                {
                    delay(1000);
                    watch->tft->fillRect(0, 0, 240, 240, TFT_BLACK);
                    watch->tft->drawString("Hiking Watch",  45, 25, 4);
                    // watch->tft->drawString("Press button", 50, 80);
                    // watch->tft->drawString("to start session", 40, 110);
                    exitSync = false;
                }

                if(startButtonPressed()) 
                {
                    state = 2;
                }

                if(sendButtonPressed())
                {
                    state = 5;
                    break;
                }

                if(irqButton) 
                {
                    sleep();
                    break;
                }
                
                if (state == 2) {
                    if (sessionStored)
                    {
                        watch->tft->fillRect(0, 0, 240, 240, TFT_BLACK);
                        watch->tft->drawString("Overwriting",  55, 100, 4);
                        watch->tft->drawString("session", 70, 130);
                        delay(1000);
                    }
                    break;
                }
            }
            break;
        }
        case 2:
        {
            /* Hiking session initalisation */

            watch->tft->fillRect(0, 0, 240, 240, TFT_BLACK);
            watch->tft->drawString("Starting hike", 45, 100);
            delay(1000);
            
            steps = 0;
            distance = 0.0f;
            state = 3;
            break;
        }
        case 3:
        {
            /* Hiking session ongoing */
            watch->tft->drawRect(70, 180, 100, 30, TFT_WHITE);
            watch->tft->drawString("STOP", 90, 183);
            distance = steps*0.0007;
            watch->tft->setCursor(20, 70);
            watch->tft->print("Steps: ");
            watch->tft->print(steps);

            watch->tft->setCursor(20, 100);
            watch->tft->print("Distance: ");
            watch->tft->print(distance);
            watch->tft->print(" km");

            if(irqBMA)
            {
                irqBMA = 0;
                bool rlst;

                do 
                {
                    // Read the BMA423 interrupt status,
                    // need to wait for it to return to true before continuing
                    rlst =  sensor->readInterrupt();
                } while (!rlst);

                if(sensor->isStepCounter()) 
                {
                    // steps = sensor->getCounter();
                    steps++;
                }
            }

            // Stop session if stop button is pressed again
            if(sendButtonPressed()) 
            {
                state = 4;
                break;
            }
            delay(20);
            break;
            
        }
        case 4:
        {
            //Save hiking session data
            watch->tft->fillRect(0, 0, 240, 240, TFT_BLACK);
            watch->tft->drawString("Saving",  55, 100, 4);
            watch->tft->drawString("session", 55, 130);
            saveIdToFile(1);
            saveDistanceToFile(distance);
            saveStepsToFile(steps);
            steps = 0;
            distance = 0.0f;
            state = 1;  
            delay(1000);
            break;
        }
        case 5:
        {
            // Send session data by BT
            sendSessionBT();
            delay(1000);
            state = 1;
            break;
        }
        default:
        {
            // Restart watch
            ESP.restart();
            break;
        }
    }

    /*      IRQ     */
    if (irqButton) 
    {
       sleep();
    }
}