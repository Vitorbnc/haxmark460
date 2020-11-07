// Code by Vitor Barbosa, based on the haxmark project
/*
 * IMPORTANT: 
 * Reflexive IR sensor is LOW when carrier is present
 * Break-beam IR sensor is HIGH when carrier is present  
 * You will need to negate all the sensor read logic when switching sensor type
 * 
 * Timings here will work for 1200dpi, the printer demo page is 300dpi by default, unless you've changed it before
 * Even the slightiest change in print quality settings may change timings, and the printer will emit a PAPER JAM error
 * 
 * Printer sensors: They're mostly IR break-beam sensors with 3 wires:
 * Black->GND
 * White->sensor signal (go from 5V to 0V when connect to printer). It's an OPEN COLLECTOR output, so you will need a 10k pull-up to 5V if using just the sensor
 * Colored->Tied to 5V trough 190R resistor, goes to IR led anode
 * 
 * Printer clutch: 2 wires(red and black)
 * The black wire goes from 24V to 0V when printer is pulling paper, this is also a collector output. 
 * Use a diode for protection and a 5V pull-up, you may keep it connected to the printer
 */

/*Paper Jam Guide
 * 251.19-> took too long to activate PPIN after activating MPF
 * 201.02-> took too long to activate EXIT after activating PPIN
 */


// Printer settings
#define PRINTRES        1200            // Resolution, either 1200 or 2400 (dpi)

// All delay times are measured in milliseconds
#define T_HALL_L        6               // Hall sensor timeout. After this time
                                        // is elapsed without a pin change, the
                                        // motor is considered stopped

#define T_POWRON        20000            // Delay to allow printer to start up

// All delays in milliseconds

//Delays with respect to activation of PPIN, ie, PPIN falling edge
#define T_EXIT_L        1609//1607//1666//1609//830
#define T_PPIN_L        540//300//400//250//540 // Time to activate PPINSO, used without a second mark on the carrier
#define T_MNPF_H        2121//2129//2085//2121
#define T_PPIN_H        2756//2743//2715//2756
#define T_EXIT_H        4267//4397//4404//4267

//Delays with respect to activation of MPF, ie, MPF falling edge
//#define T_EXIT_L        2181//
//#define T_PPIN_L        300//540 Time to activate PPINSO, used without a second mark on the carrier
//#define T_MNPF_H        2698//
//#define T_PPIN_H        3327//
//#define T_EXIT_H        4991//


typedef enum
{
    PowerOnWait,                        // Power-on test state with all outputs
                                        // enabled

    Idle,                               // Waiting for print job

    PullPaper,                          // Printer asking for paper input

    PaperInside,                        // Paper inside, wait for second mark

    Transfer,                           // Toner transfer

    Exit                                // Finish print

    

} State_t;

#define P_LED_RD        6            // Red status LED
#define P_LED_YE        5            // Yellow status LED, motor running
#define P_LED_GN        4            // Green status LED
#define P_PPINSO        9            // Paper in sensor output
#define P_MNPFSO        10            // Manual paper feed sensor output
#define P_EXITSO        11            // Exit sensor output
#define P_CLUTCH        12            // Clutch control pin

#define P_OPANEL        8            // Operator panel. It's the blue wire on E460DN
                                        // CAUTION: Do not change (INT0 is used
                                       // for monitoring this input)!
                                       
#define P_CLUTCHINS     7            // Clutch input line, is low when printer pulls paper in
#define P_NPPINS        2            // New paper in sensor
#define P_HALL_1        3            // Hall sensor from motor

volatile uint32_t MillisecondCounter = 0;
volatile uint16_t HallSensorTimer = T_HALL_L;
volatile State_t State = PowerOnWait;
volatile uint8_t HallSensorBuffer = 0;

State_t lastState = PowerOnWait;

bool lastNPPINS = true;
bool lastPanel = true;

#define DEBUG true
volatile bool hallPulse = false;
volatile bool panelPulse = false;
volatile bool NPPINSPulse = false;


void setup() {
  if(DEBUG)
	Serial.begin(115200);

  pinMode(P_LED_GN, OUTPUT);
  pinMode(P_LED_YE, OUTPUT);
  pinMode(P_LED_RD, OUTPUT);

  pinMode(P_PPINSO,OUTPUT);
  pinMode(P_MNPFSO,OUTPUT);
  pinMode(P_EXITSO,OUTPUT);

  pinMode(P_CLUTCH,OUTPUT);

  pinMode(P_OPANEL, INPUT);
  pinMode(P_HALL_1, INPUT);
  pinMode(P_NPPINS, INPUT);
  pinMode(P_CLUTCHINS,INPUT);

  //attachInterrupt(digitalPinToInterrupt(P_OPANEL),ISRPanelSignal,FALLING);
  attachInterrupt(digitalPinToInterrupt(P_NPPINS),ISRNPPINS,FALLING); // FALLING for break beam ir sensor
  attachInterrupt(digitalPinToInterrupt(P_HALL_1),ISRHallSignal,CHANGE);

//// TIMER 0 ////

    // Enable CTC mode
    TCCR0A = (1 << WGM01);

    // Select clock F_CPU/64 
    TCCR0B = (1 << CS01) | (1 << CS00);

    // Set the value that you want to count to
    // 1kHz interrupt frequency
    OCR0A = 0xF9; //249 for 16MHz or 124 for 8MHz

    // Enable OC0A interrupt
    TIMSK0 = (1 << OCIE0A);

    //Enable Interrupts
    sei();

    MillisecondCounter = 0;
    State = PowerOnWait;

}

//Called every 1ms
ISR(TIMER0_COMPA_vect)
{
    MillisecondCounter++;
    if(MillisecondCounter == 0)
    {
        MillisecondCounter = UINT32_MAX;
    }

    if(State != PowerOnWait)
    {
        HallSensorTimer++;
        if(HallSensorTimer == 0)
        {
            HallSensorTimer = UINT16_MAX;
        }
//        if(HallSensorTimer < T_HALL_L)
//            digitalWrite(P_LED_YE,HIGH);
//        else
//            digitalWrite(P_LED_YE,LOW);
        
    }
}

// External interrupt. Triggered by control panel
void ISRPanelSignal(){
	panelPulse = true;
    // Just reset everything
    //if(State == PullPaper || State == ClutchDelay)
    if(State != PowerOnWait)
    {
        State = Idle;
    }
}

// External interrupt. Triggered by hall sensor
void ISRHallSignal(){
    HallSensorTimer = 0;
	hallPulse = true;
}


// External interrupt. Triggered by NPPINS 
void ISRNPPINS(){
  if(State == PullPaper){
    digitalWrite(P_MNPFSO,LOW);
    State = PaperInside;
    MillisecondCounter = 0;
  }
  
    //Comment this if using time emulation without a second mark
//  else if(State == PaperInside){
//    digitalWrite(P_PPINSO,LOW);
//    State = Transfer;
//    MillisecondCounter = 0;
//  }
  
  NPPINSPulse = true;

}

void loop() {
  // put your main code here, to run repeatedly:
 switch(State)
        {
            case PowerOnWait:
                // Does nothing.
                // Waits for the initialization of the printer to complete
                if(MillisecondCounter >= T_POWRON)
                {
                    State = Idle;
                    if(DEBUG) Serial.println("State = Idle");
                }
                break;
            case Idle:
                // Activate clutch and reset all sensor output just in case
                // something has gone mucus
                digitalWrite(P_PPINSO,HIGH);
                digitalWrite(P_MNPFSO,HIGH);
                digitalWrite(P_EXITSO,HIGH);
                digitalWrite(P_LED_RD,HIGH);
                digitalWrite(P_CLUTCH,HIGH);
                //digitalWrite(P_LED_GN,LOW);


                // Check if motor is running, new paper in sensor is blocked by carrier and printer is requesting paper
                if(HallSensorTimer < T_HALL_L && digitalRead(P_NPPINS) && !digitalRead(P_CLUTCHINS))
                {
                    // Unblock carrier
                    digitalWrite(P_CLUTCH,LOW);
                    //digitalWrite(P_LED_GN,HIGH);
                    //digitalWrite(P_LED_RD,LOW);

                    State = PullPaper;
                    //MillisecondCounter = 0;
                    if(DEBUG) Serial.println("State = PullPaper");
                }
                break;
              case PullPaper:

              //Comment this if using interrupts
//              if(digitalRead(P_NPPINS)){
//              digitalWrite(P_MNPFSO,LOW);
//              State = PaperInside;
//              MillisecondCounter = 0;
//              delay(200);
//              }
                
              break;

              case PaperInside:
                if(State!=lastState){
                  if(DEBUG) Serial.println("State = PaperInside");
                  lastState = State;  
                }

              //Comment this if using interrupts or time emulation
//                if(digitalRead(P_NPPINS)){
//                   digitalWrite(P_PPINSO,LOW);
//                   State = Transfer;
//                  MillisecondCounter = 0;
//                  }

                // Emulate PPINS here with timing if needed
                if(MillisecondCounter >= T_PPIN_L){
                  digitalWrite(P_PPINSO,LOW);
                  State = Transfer;
                  MillisecondCounter = 0; //Zero counter if couting times wrt PPINS fall
                 if(DEBUG) Serial.println("State = Transfer");
                }
              
              break;

              case Transfer:
                if(State!=lastState){
                  if(DEBUG) Serial.println("State = Transfer");
                  lastState = State;  
                }
                if(MillisecondCounter >= T_EXIT_L){
                  digitalWrite(P_EXITSO,LOW);
                  State = Exit;
                  if(DEBUG) Serial.println("State = Exit");
                }
              
              break;
  
            case Exit:
                if(MillisecondCounter >= T_MNPF_H)
                  digitalWrite(P_MNPFSO,HIGH);
                if(MillisecondCounter >= T_PPIN_H)
                  digitalWrite(P_PPINSO,HIGH);
                if(MillisecondCounter >= T_EXIT_H)
                {
                    // Deactivate exit sensor output
                    digitalWrite(P_EXITSO,HIGH);

                    MillisecondCounter = 0;
                    State = Idle;
                    if(DEBUG) Serial.println("State = Idle");
                }
                break;
        }
        
        if(DEBUG){
    //Serial.print("t: ");
    //Serial.print(MillisecondCounter);
    if(hallPulse){
    //Serial.print("\t hall");
    hallPulse=false;
    }
    bool x = digitalRead(P_OPANEL);
    if(x!=lastPanel){
      if(x) Serial.print("\t panel 1\n");
      else Serial.print("\t panel 0\n");
    }
    if(NPPINSPulse){
    Serial.println("NPIS Activated");
    NPPINSPulse = false;
    }

   lastPanel = x;
  if(panelPulse) panelPulse = false; 
  }
    
}
