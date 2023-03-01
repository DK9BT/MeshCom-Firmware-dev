#include <Arduino.h>
#include <SPI.h>
#include <configuration.h>
#include <SX126x-RAK4630.h>
#include <debugconf.h>

#include <WisBlock-API.h>
/*
    RAK4631 PIN DEFINITIONS

    static const uint8_t WB_IO1 = 17;	   // SLOT_A SLOT_B
    static const uint8_t WB_IO2 = 34;	   // SLOT_A SLOT_B
    static const uint8_t WB_IO3 = 21;	   // SLOT_C
    static const uint8_t WB_IO4 = 4;	   // SLOT_C
    static const uint8_t WB_IO5 = 9;	   // SLOT_D
    static const uint8_t WB_IO6 = 10;	   // SLOT_D
    static const uint8_t WB_SW1 = 33;	   // IO_SLOT
    static const uint8_t WB_A0 = 5;		   // IO_SLOT
    static const uint8_t WB_A1 = 31;	   // IO_SLOT
    static const uint8_t WB_I2C1_SDA = 13; // SENSOR_SLOT IO_SLOT
    static const uint8_t WB_I2C1_SCL = 14; // SENSOR_SLOT IO_SLOT
    static const uint8_t WB_I2C2_SDA = 24; // IO_SLOT
    static const uint8_t WB_I2C2_SCL = 25; // IO_SLOT
    static const uint8_t WB_SPI_CS = 26;   // IO_SLOT
    static const uint8_t WB_SPI_CLK = 3;   // IO_SLOT
    static const uint8_t WB_SPI_MISO = 29; // IO_SLOT
    static const uint8_t WB_SPI_MOSI = 30; // IO_SLOT

    SPI
    static const uint8_t SS = 26;
    static const uint8_t MOSI = PIN_SPI_MOSI;
    static const uint8_t MISO = PIN_SPI_MISO;
    static const uint8_t SCK = PIN_SPI_SCK;

    // QSPI Pins
    #define PIN_QSPI_SCK 3	// 19
    #define PIN_QSPI_CS 26	// 17
    #define PIN_QSPI_IO0 30 // 20
    #define PIN_QSPI_IO1 29 // 21
    #define PIN_QSPI_IO2 28 // 22
    #define PIN_QSPI_IO3 2	// 23

    @note RAK5005-O GPIO mapping to RAK4631 GPIO ports
   RAK5005-O <->  nRF52840
   IO1       <->  P0.17 (Arduino GPIO number 17)
   IO2       <->  P1.02 (Arduino GPIO number 34)
   IO3       <->  P0.21 (Arduino GPIO number 21)
   IO4       <->  P0.04 (Arduino GPIO number 4)
   IO5       <->  P0.09 (Arduino GPIO number 9)
   IO6       <->  P0.10 (Arduino GPIO number 10)
   SW1       <->  P0.01 (Arduino GPIO number 1)
   A0        <->  P0.04/AIN2 (Arduino Analog A2
   A1        <->  P0.31/AIN7 (Arduino Analog A7
   SPI_CS    <->  P0.26 (Arduino GPIO number 26)

    SX126x-Arduino library for RAK4630:
    _hwConfig.CHIP_TYPE = SX1262;		   // Chip type, SX1261 or SX1262
    _hwConfig.PIN_LORA_RESET = 38;		   // LORA RESET
    _hwConfig.PIN_LORA_NSS = 42;		   // LORA SPI CS
    _hwConfig.PIN_LORA_SCLK = 43;		   // LORA SPI CLK
    _hwConfig.PIN_LORA_MISO = 45;		   // LORA SPI MISO
    _hwConfig.PIN_LORA_DIO_1 = 47;		   // LORA DIO_1
    _hwConfig.PIN_LORA_BUSY = 46;		   // LORA SPI BUSY
    _hwConfig.PIN_LORA_MOSI = 44;		   // LORA SPI MOSI
    _hwConfig.RADIO_TXEN = 39;			   // LORA ANTENNA TX ENABLE (e.g. eByte E22 module)
    _hwConfig.RADIO_RXEN = 37;			   // LORA ANTENNA RX ENABLE (e.g. eByte E22 module)
    _hwConfig.USE_DIO2_ANT_SWITCH = true;  // LORA DIO2 controls antenna
    _hwConfig.USE_DIO3_TCXO = true;		   // LORA DIO3 controls oscillator voltage (e.g. eByte E22 module)
    _hwConfig.USE_DIO3_ANT_SWITCH = false; // LORA DIO3 controls antenna (e.g. Insight SIP ISP4520 module)
    _hwConfig.USE_RXEN_ANT_PWR = true;	   // RXEN is used as power for antenna switch

    */

/*
Sync Word Setting in Meshtastic
    Meshtastic Syc Word is 0x2b

    Output of the LoRa Sync Word Register 0x0740 in Meshtastic:

    ??:??:?? 1 Set radio: final power level=22
    SYNC WORD SET!
    Sync Word 1st byte = 24
    Sync Word 2nd byte = b4
    ??:??:?? 1 SX126x init result 0
    ??:??:?? 1 Current limit set to 140.000000

    In our Library it gets set at sx126x.h / radio.cpp
    Define: sx126x.h line 109:
    #define LORA_MAC_PUBLIC_SYNCWORD 0x242b

    Radio.SetPublicNetwork(true); needs to be called, so syncword gets new set in radio.cpp line: 1183 in
    void RadioSetPublicNetwork(bool enable)
    Method
*/

// NVIC_SystemReset(); resets the device

// Lora callback Function declarations
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnRxTimeout(void);
void OnRxError(void);
void OnTxDone(void);
void OnTxTimeout(void);
void OnPreambleDetect(void);
void OnHeaderDetect(void);


// Ethernet Object
//NrfETH neth;

// LoRa Events and Buffers
static RadioEvents_t RadioEvents;
static uint8_t RcvBuffer[UDP_TX_BUF_SIZE];
static uint8_t RcvBuffer_before[UDP_TX_BUF_SIZE];
static uint8_t txBuffer[UDP_TX_BUF_SIZE]; // we need an extra buffer for udp tx, as we add other stuff (ID, RSSI, SNR, MODE)
uint8_t lora_tx_buffer[UDP_TX_BUF_SIZE];  // lora tx buffer
static uint8_t ringBufferUDPout[MAX_RING_UDP_OUT][UDP_TX_BUF_SIZE]; //Ringbuffer for UDP TX from LoRa RX, first byte is length
uint8_t udpWrite = 0;   // counter for ringbuffer
uint8_t udpRead = 0;    // counter for ringbuffer

s_meshcom_settings g_meshcom_settings;
bool g_meshcom_initialized;
bool init_flash_done=false;

// RINGBUFFER for incoming UDP lora packets for lora TX
unsigned char ringBuffer[MAX_RING][UDP_TX_BUF_SIZE];
int iWrite=0;
int iRead=0;

bool bDEBUG=false;

//variables and helper functions
int sendlng = 0;              // lora tx message length
uint8_t cmd_counter = 2;      // ticker dependant on main cycle delay time
void print_radioStatus(void); // prints the current Radio Status
uint8_t preamble_cnt = 0;     // stores how often a preamble detect is thrown
bool tx_is_active = false;    // avoids calling doTX() on each main iteration when we are already in TX mode
bool is_receiving = false;  // flag to store we are receiving a lora packet. triggered by header detect not preamble
uint8_t err_cnt_udp_tx = 0;    // counter on errors sending message via UDP

// timers
unsigned long hb_time = 0;            // heartbeat timer
unsigned long dhcp_timer = 0;         // dhcp refresh timer
unsigned long ntp_timer = 0;          // dhcp refresh timer
unsigned long chk_udp_conn_timer = 0; // we check periodically if we have received a HB from server
//unsigned long till_header_time = 0;    // stores till a header is detected after preamble detect
unsigned int msg_counter = 0;
char msg_text[300];

String strText="";

// Prototypes
bool is_new_packet(uint16_t size);                         // switch if we have a packet received we never saw before RcvBuffer[12] changes, rest is same
void addNodeData(uint16_t size, int16_t rssi, int8_t snr); // add additional data we need and send the udp packet
void blinkLED();
void sendHeartbeat();                                // heartbeat to server
void doTX();                                         // LoraTX function
void addUdpOutBuffer(uint8_t *buffer, uint16_t len); // function adds outgoing udp messages in the udp_out_ringbuffer
void printBuffer(uint8_t *buffer, int len);
void printBuffer_ascii(uint8_t *buffer, int len);
void sendMessage(char *buffer, int len);
void commandAction(char *buffer, int len);

// Client basic variables
    uint8_t dmac[6];
    unsigned int _GW_ID = 0x4B424332; // ID of our Node

    String _owner_call = "XX0XXX-0"; // our longanme, can be up to 20 chars ending with 0x00

    String _owner_short = "XXX40";     //our shortname

/**
 * @brief Initialize LoRa HW and LoRaWan MAC layer
 *
 * @return int8_t result
 *  0 => OK
 * -1 => SX126x HW init failure
 * -2 => LoRaWan MAC initialization failure
 * -3 => Subband selection failure
 */
int8_t init_meshcom(void)
{
#ifdef ESP32
	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, HIGH);
	delay(500);
#endif

	// Initialize LoRa chip.
	if (api_init_lora() != 0)
	{
		API_LOG("LORA", "Failed to initialize SX1262");
		return -1;
	}

	// Setup the EUIs and Keys
    /*KBC
	lmh_setDevEui(g_lorawan_settings.node_device_eui);
	lmh_setAppEui(g_lorawan_settings.node_app_eui);
	lmh_setAppKey(g_lorawan_settings.node_app_key);
	lmh_setNwkSKey(g_lorawan_settings.node_nws_key);
	lmh_setAppSKey(g_lorawan_settings.node_apps_key);
	lmh_setDevAddr(g_lorawan_settings.node_dev_addr);

	// Setup the LoRaWan init structure
	lora_param_init.adr_enable = g_lorawan_settings.adr_enabled;
	lora_param_init.tx_data_rate = g_lorawan_settings.data_rate;
	lora_param_init.enable_public_network = g_lorawan_settings.public_network;
	lora_param_init.nb_trials = g_lorawan_settings.join_trials;
	lora_param_init.tx_power = g_lorawan_settings.tx_power;
	lora_param_init.duty_cycle = g_lorawan_settings.duty_cycle_enabled;
   
	API_LOG("LORA", "Initialize LoRaWAN for region %s", region_names[g_lorawan_settings.lora_region]);
	// Initialize LoRaWan
	if (lmh_init(&lora_callbacks, lora_param_init, g_lorawan_settings.otaa_enabled, (eDeviceClass)g_lorawan_settings.lora_class, (LoRaMacRegion_t)g_lorawan_settings.lora_region) != 0)
	{
		API_LOG("LORA", "Failed to initialize LoRaWAN");
		return -2;
	}

	// For some regions we might need to define the sub band the gateway is listening to
	// This must be called AFTER lmh_init()
	if (!lmh_setSubBandChannels(g_lorawan_settings.subband_channels))
	{
		API_LOG("LORA", "lmh_setSubBandChannels failed. Wrong sub band requested?");
		return -3;
	}

	API_LOG("LORA", "Begin timer");
	// Initialize the app timer
	api_timer_init();

	API_LOG("LORA", "Start Join");
	// Start Join process
	lmh_join();

    */
	g_meshcom_initialized = true;
	return 0;
}

void getMacAddr(uint8_t *dmac)
{
    const uint8_t *src = (const uint8_t *)NRF_FICR->DEVICEADDR;
    dmac[5] = src[0];
    dmac[4] = src[1];
    dmac[3] = src[2];
    dmac[2] = src[3];
    dmac[1] = src[4];
    dmac[0] = src[5] | 0xc0; // MSB high two bits get set elsewhere in the bluetooth stack
}
void nrf52setup()
{

     // LEDs
    pinMode(PIN_LED1, OUTPUT);
    pinMode(PIN_LED2, OUTPUT);

    // clear the buffers
    for (int i = 0; i < uint8_t(sizeof(RcvBuffer)); i++)
    {
        RcvBuffer[i] = RcvBuffer_before[i] = 0x00;
    }

    //clear ringbuffer
    for(int i=0; i<MAX_RING_UDP_OUT; i++)
        memset(ringBufferUDPout[i],0,UDP_TX_BUF_SIZE);

    //  Initialize the LoRa Module
    lora_rak4630_init();

    //  Initialize the Serial Port for debug output
    time_t timeout = millis();
    Serial.begin(MONITOR_SPEED);
    while (!Serial)
    {
        if ((millis() - timeout) < 2000)
        {
            delay(100);
        }
        else
        {
            break;
        }
    }

    Serial.println("=====================================");
    Serial.println("CLIENT STARTED");
    Serial.println("=====================================");

    //  Set the LoRa Callback Functions
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;
    //RadioEvents.PreAmpDetect = OnPreambleDetect;
    //NICHT BENÖTIGTRadioEvents.HeaderDetect = OnHeaderDetect;
    

    //  Initialize the LoRa Transceiver
    Radio.Init(&RadioEvents);

    // Sets the Syncowrd new that we can set the MESHTASTIC SWORD
    DEBUG_MSG("RADIO", "Setting new LoRa Sync Word");
    Radio.SetPublicNetwork(true);

    //  Set the LoRa Frequency
    Radio.SetChannel(RF_FREQUENCY);

    //  Configure the LoRa Transceiver for receiving messages
    Radio.SetRxConfig(
        MODEM_LORA,
        LORA_BANDWIDTH,
        LORA_SPREADING_FACTOR,
        LORA_CODINGRATE,
        0, //  AFC bandwidth: Unused with LoRa
        LORA_PREAMBLE_LENGTH,
        LORA_SYMBOL_TIMEOUT,
        LORA_FIX_LENGTH_PAYLOAD_ON,
        0,    //  Fixed payload length: N/A
        true, //  CRC enabled
        0,    //  Frequency hopping disabled
        0,    //  Hop period: N/A
        LORA_IQ_INVERSION_ON,
        true //  Continuous receive mode
    );

    // Set Radio TX configuration
    Radio.SetTxConfig(
        MODEM_LORA,
        TX_OUTPUT_POWER,
        0, // fsk only
        LORA_BANDWIDTH,
        LORA_SPREADING_FACTOR,
        LORA_CODINGRATE,
        LORA_PREAMBLE_LENGTH,
        LORA_FIX_LENGTH_PAYLOAD_ON,
        true, // CRC ON
        0,    // fsk only frequ hop
        0,    // fsk only frequ hop period
        LORA_IQ_INVERSION_ON,
        TX_TIMEOUT_VALUE);

    //  Start receiving LoRa packets
    DEBUG_MSG("RADIO", "Starting RX MODE");
    Radio.Rx(RX_TIMEOUT_VALUE);

    getMacAddr(dmac);

    _GW_ID = dmac[0] | (dmac[1] << 8) | (dmac[2] << 16) | (dmac[3] << 24);

	// Initialize battery reading
	init_batt();

    delay(100);
}

void nrf52loop()
{
    if(!init_flash_done)
    {
        init_flash();

        char _owner_c[20];
        sprintf(_owner_c, "%s", g_meshcom_settings.node_call);
        _owner_call = _owner_c;

        sprintf(_owner_c, "%s", g_meshcom_settings.node_short);
        _owner_short = _owner_c;
    }

    // check if we have messages in ringbuffer to send
    if (iWrite != iRead)
    {
        if(cmd_counter <= 0)
        {
            if (tx_is_active == false && is_receiving == false)
                doTX();
        }
        else
            cmd_counter--;
    }

    //check if we got from the serial input
    if(Serial.available() > 0)
    {
        char rd = Serial.read();

        Serial.print(rd);

        strText += rd;

        //str.trim();                 // removes CR, NL, Whitespace at end

        if (strText.startsWith("reset"))
        {
            strText="";
            NVIC_SystemReset();     // resets the device
        }
        else
        if(strText.startsWith(":") || strText.startsWith("-"))
        {
            if(strText.endsWith("\n") || strText.endsWith("\r"))
            {
                strText.trim();
                strcpy(msg_text, strText.c_str());

                int inext=0;
                char msg_buffer[300];
                for(int itx=0; itx<(int)strText.length(); itx++)
                {
                    if(msg_text[itx] == 0x08 || msg_text[itx] == 0x7F)
                    {
                        inext--;
                        if(inext < 0)
                            inext=0;
                            
                        msg_buffer[inext+1]=0x00;
                    }
                    else
                    {
                        msg_buffer[inext]=msg_text[itx];
                        msg_buffer[inext+1]=0x00;
                        inext++;
                    }
                }

                if(strText.startsWith(":"))
                    sendMessage(msg_buffer, inext);

                if(strText.startsWith("-"))
                    commandAction(msg_buffer, inext);

                strText="";
            }
        }
        else
        {
            if(!strText.startsWith("\n") && !strText.startsWith("\r"))
            {
                printf("MSG:%02X", rd);
                printf("..not sent\n");
            }
            strText="";
        }
    }

    msg_counter++;
    
    //  We are on FreeRTOS, give other tasks a chance to run
    delay(100);
    yield();
}

/**@brief Function to be executed on Radio Rx Done event
 */


void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    //unsigned long diff_rx = millis() - till_header_time;
    //DEBUG_MSG_VAL("RADIO", diff_rx, "Time Preamble to RxDone");

    memcpy(RcvBuffer, payload, size);

    if (is_new_packet(size) || (SEE_ALL_PACKETS == 1))
    {
		// :|0x11223344|0x05|OE1KBC|>*:Hallo Mike, ich versuche eine APRS Meldung\0x00

        // print which message type we got
        uint8_t msg_type_b_lora = payload[0];

        switch (msg_type_b_lora)
        {

            case 0x3A: DEBUG_MSG("RADIO", "Received Textmessage"); break;
            case 0x03: DEBUG_MSG("RADIO", "Received PosInfo"); break;
            case 0x04: DEBUG_MSG("RADIO", "Received NodeInfo"); break;
            case 0x05: DEBUG_MSG("RADIO", "Routing APP"); break;
            case 0x06: DEBUG_MSG("RADIO", "Admin APP"); break;
            case 0x20: DEBUG_MSG("RADIO", "Reply APP"); break;
            case 0x42: DEBUG_MSG("RADIO", "Rangetest APP"); break;
            case 0x43: DEBUG_MSG("RADIO", "Environmental APP"); break;
            default:
                DEBUG_MSG("RADIO", "Received unknown");
                if(bDEBUG)
                    printBuffer(RcvBuffer, size);
                break;
        }

        // txtmessage
        if(msg_type_b_lora == 0x3A)
        {
            // print ascii of message
            printBuffer_ascii(RcvBuffer, size);

            // we add now Longname (up to 20), ID - 4, RSSI - 2, SNR - 1 and MODE BYTE - 1
            // MODE BYTE: LongSlow = 1, MediumSlow = 3
            // and send the UDP packet (done in the method)

            // we only send the packet via UDP if we have no collision with UDP rx
            // und wenn MSG nicht von einem anderen Gateway empfangen wurde welches es bereits vopm Server bekommen hat
            int msg_id = (RcvBuffer[4]<<24) | (RcvBuffer[3]<<16) | (RcvBuffer[2]<<8) | RcvBuffer[1];
            int msg_hop = RcvBuffer[5] & 0x07;
            int msg_hop_pos = 5;
            bool msg_server =false;
            if(RcvBuffer[5] & 0x80)
                msg_server=true;

            int lora_msg_len = size;
            if (lora_msg_len > UDP_TX_BUF_SIZE)
            lora_msg_len = UDP_TX_BUF_SIZE; // zur Sicherheit

            if(bDEBUG)
                printf("msg_id: %04X msg_len: %i payload[%i]=%i via=%d\n", msg_id, lora_msg_len, msg_hop_pos, msg_hop, msg_server);

            if(!msg_server) // Message kommt von User
            {
                // Wiederaussendung via LORA
                // Ringbuffer filling

                bool bMsg=false;

                for(int iop=0;iop<MAX_RING_UDP_OUT;iop++)
                {
                    int ring_msg_id = (ringBufferUDPout[iop][4]<<24) | (ringBufferUDPout[iop][3]<<16) | (ringBufferUDPout[iop][2]<<8) | ringBufferUDPout[iop][1];

                    if(ring_msg_id != 0 && bDEBUG)
                        printf("ring_msg_id:%08X msg_id:%08X\n", ring_msg_id, msg_id);

                    if(ring_msg_id == msg_id)
                    {
                        bMsg=true;

                        break;
                    }
                }

                if(!bMsg)
                {
                    if(msg_hop > 0 && msg_hop_pos > 0)
                        payload[msg_hop_pos]=msg_hop-1;
                


                    if ((msg_type_b_lora == 0x3A || msg_type_b_lora == 0x03 || msg_type_b_lora == 0x04))
                    {
                        addNodeData(size, rssi, snr);
                        //DEBUG_MSG("RADIO", "Packet sent to -->");
                    }
                    else
                    {
                        //DEBUG_MSG("RADIO", "Packet not sent to -->");
                    }
                } 
            }   
        }
        else
        {
            // print hex of message
            if(bDEBUG)
                printBuffer(RcvBuffer, size);
        }

        if(bDEBUG)
            Serial.println("");

        // store received message to compare later on
        memcpy(RcvBuffer_before, RcvBuffer, UDP_TX_BUF_SIZE);
        // set buffer to 0
        memset(RcvBuffer, 0, UDP_TX_BUF_SIZE);

        blinkLED();
    }
    else
        DEBUG_MSG("RADIO", "Packet discarded, already seen it!");

    cmd_counter = WAIT_AFTER_RX;
    is_receiving = false;

    Radio.Rx(RX_TIMEOUT_VALUE);

}

/**@brief Function to be executed on Radio Rx Timeout event
 */
void OnRxTimeout(void)
{
    // DEBUG_MSG("RADIO", "OnRxTimeout");
    Radio.Rx(RX_TIMEOUT_VALUE);
    is_receiving = false;
}

/**@brief Function to be executed on Radio Rx Error event
 */

void OnRxError(void)
{
    cmd_counter = WAIT_AFTER_RX;
    //DEBUG_MSG("RADIO", "RX ERROR");
    Radio.Rx(RX_TIMEOUT_VALUE);
    is_receiving = false;
}

/**@brief Function to be executed on Radio Tx Done event
 */
void OnTxDone(void)
{
    // DEBUG_MSG("RADIO", "OnTxDone");
    cmd_counter=WAIT_AFTER_TXDONE;
    Radio.Rx(RX_TIMEOUT_VALUE);
    tx_is_active = false;
    digitalWrite(PIN_LED2, LOW);
}

/**@brief Function to be executed on Radio Tx Timeout event
 */
void OnTxTimeout(void)
{
    // DEBUG_MSG("RADIO", "OnTxTimeout");
    tx_is_active = false;
    digitalWrite(PIN_LED2, LOW);
    Radio.Rx(RX_TIMEOUT_VALUE);
}

/**@brief fires when a preamble is detected 
 * currently not used!
 */
void OnPreambleDetect(void)
{
    //till_header_time = millis();
    preamble_cnt++;

    if(preamble_cnt >= 2){

        //DEBUG_MSG("RADIO", "Preamble detected");
        preamble_cnt = 0;
    } 
}

/**@brief our Lora TX sequence
 */

void doTX()
{
    tx_is_active = true;

    if (iWrite != iRead && iWrite < MAX_RING)
    {
        int irs=iRead;

        sendlng = ringBuffer[iRead][0];
        memcpy(lora_tx_buffer, ringBuffer[iRead] + 1, sendlng);

        // we can now tx the message
        if (TX_ENABLE == 1)
        {
            digitalWrite(PIN_LED2, HIGH);

            // print tx buffer
            //printBuffer(neth.lora_tx_buffer_eth, neth.lora_tx_msg_len);

            iRead++;
            if (iRead >= MAX_RING)
                iRead = 0;

            Radio.Send(lora_tx_buffer, sendlng);

            cmd_counter = WAIT_TX;

            if (iWrite == iRead)
            {
                DEBUG_MSG_VAL("RADIO", irs, "TX (LAST) :");
            }
            else
            {
                DEBUG_MSG_VAL("RADIO", irs, "TX :");
            }

            Serial.println("");
        }
        else
        {
            DEBUG_MSG("RADIO", "TX DISABLED");
        }
    }
}


/**@brief Function to check if we have a Lora packet already received
 */
bool is_new_packet(uint16_t size)
{
    for (int i = 0; i < size; i++)
    {
        if (i != 12)
        {
            if (RcvBuffer[i] != RcvBuffer_before[i])
            {
                return true;
            }
        }
    }
    return false;
}

/**@brief Function to write our additional data into the UDP tx buffer
 * we add now Longname (up to 20), ID - 4, RSSI - 2, SNR - 1 and MODE BYTE - 1
 * MODE BYTE: LongSlow = 1, MediumSlow = 3
 * 8 byte offset = ID+RSSI+SNR
 */
void addNodeData(uint16_t size, int16_t rssi, int8_t snr)
{

    uint8_t longname_len = _owner_call.length();
    char longname_c[longname_len + 1];

    // copying the contents of the
    // string to char array
    strcpy(longname_c, _owner_call.c_str());

    uint8_t offset = 8 + longname_len + 1; // offset for the payload written into tx udp buffer. We add 0x00 after Longanme

    if (longname_len <= LONGNAME_MAXLEN)
    {
        memcpy(&txBuffer, &longname_c, longname_len);
        txBuffer[longname_len] = 0x00; // we add a trailing 0x00 to mark the end of longname
    }
    else
    {
        DEBUG_MSG("ERROR", "LongName is too long!");
        longname_len = LONGNAME_MAXLEN;
    }

    uint8_t offset_params = longname_len + 1;
    memcpy(&txBuffer[offset_params], &_GW_ID, sizeof(_GW_ID));
    memcpy(&txBuffer[offset_params + 4], &rssi, sizeof(rssi));
    txBuffer[offset_params + 6] = snr;
    txBuffer[offset_params + 7] = 0x03; // manually set to 0x03 because we are on MediumSlow per default

    // now copy the rcvbuffer into txbuffer
    if ((size + 8 + offset) < UDP_TX_BUF_SIZE)
    {
        for (int i = 0; i < size; i++)
        {
            txBuffer[i + offset] = RcvBuffer[i];
        }
        // add it to the outgoing udp buffer
        // TODO change txBuffer with rinbuffer
        //DEBUG_MSG("UDP", "UDP out Buffer");
        //neth.printBuffer(txBuffer, (size + offset));
        addUdpOutBuffer(txBuffer, (size + offset));
    }
    else
    {
        DEBUG_MSG("ERROR", "Exceeding Buffer length!");
    }

}

/**@brief Function adding messages into outgoing UDP ringbuffer
 * 
 */
void addUdpOutBuffer(uint8_t *buffer, uint16_t len)
{
    if (len > UDP_TX_BUF_SIZE)
        len = UDP_TX_BUF_SIZE; // just for safety

    //first byte is always the message length
    ringBufferUDPout[udpWrite][0] = len;
    memcpy(ringBufferUDPout[udpWrite] + 1, buffer, len + 1);

    if(bDEBUG)
    {
        Serial.printf("Out Ringbuffer added element: %u\n", udpWrite);
        //DEBUG_MSG_VAL("UDP", udpWrite, "UDP Ringbuf added El.:");
        printBuffer(ringBufferUDPout[udpWrite], len + 1);
    }

    udpWrite++;
    if (udpWrite >= MAX_RING_UDP_OUT) // if the buffer is full we start at index 0 -> take care of overwriting!
        udpWrite = 0;
}

/**@brief Method to print our buffers
 */
void printBuffer(uint8_t *buffer, int len)
{
  for (int i = 0; i < len; i++)
  {
    Serial.printf("%02X ", buffer[i]);
  }
  Serial.println("");
}

/**@brief Method to print our buffers
 */
void printBuffer_ascii(uint8_t *buffer, int len)
{
  int i=0;

  Serial.printf("%03i ", len);

  Serial.printf("%c !", buffer[0]);
  for (i = 4; i > 0; i--)
  {
    Serial.printf("%02X", buffer[i]);
  }
  Serial.printf(" %02X ", buffer[5]);

  int ineg=2;
  if(buffer[len-7] == 0x00)
    ineg=6;

  for (i = 6; i < len-ineg; i++)
  {
    if(buffer[i] != 0x00)
      Serial.printf("%c", buffer[i]);
  }

  Serial.printf(" %02X", buffer[len-ineg]);
  Serial.printf("%02X", buffer[(len-ineg)+1]);

  Serial.println("");
}

/**@brief Function to check if the modem detected a preamble
 */
void blinkLED()
{
    digitalWrite(PIN_LED1, HIGH);
    delay(2);
    digitalWrite(PIN_LED1, LOW);
}

void blinkLED2()
{
    digitalWrite(PIN_LED2, HIGH);
    delay(2);
    digitalWrite(PIN_LED2, LOW);
}


/**@brief Function to get the current Radio Status
 * Radio status.[RF_IDLE, RF_RX_RUNNING, RF_TX_RUNNING]
 */

void print_radioStatus()
{
    if(Radio.GetStatus() == RF_IDLE) Serial.println("RF_IDLE");
    if(Radio.GetStatus() == RF_RX_RUNNING) Serial.println("RF_RX_RUNNING");
    if(Radio.GetStatus() == RF_TX_RUNNING) Serial.println("RF_TX_RUNNING");
    if(Radio.GetStatus() == RF_CAD) Serial.println("RF_CAD");

} 

void sendMessage(char *msg_text, int len)
{
    uint8_t msg_buffer[300];
    char msg_start[100];

    // :|0x11223344|0x05|OE1KBC|>*:Hallo Mike, ich versuche eine APRS Meldung\0x00

    msg_buffer[0]=':';
    msg_counter++;

    msg_buffer[1]=msg_counter & 0xff;
    msg_buffer[2]=(msg_counter >> 8) & 0xff;
    msg_buffer[3]=(msg_counter >> 16) & 0xff;
    msg_buffer[4]=(msg_counter >> 24) & 0xff;

    msg_buffer[5]=0x05; //max hop

    sprintf(msg_start, "%s>*", _owner_call.c_str());

    memcpy(msg_buffer+6, msg_start, _owner_call.length()+2);

    int inext=6+2+_owner_call.length();

    memcpy(msg_buffer+inext, msg_text, len);

    inext=inext+len;
    
    /*
    msg_buffer[inext]='/';
    inext++;

    sprintf(msg_start, "%03d%%", mv_to_percent(read_batt()));
    memcpy(msg_buffer+inext, msg_start, 4);

    inext+=4;
    */

    msg_buffer[inext]=0x00;
    inext++;

    int FCS_SUMME=0;
    for(int ifcs=0; ifcs<inext; ifcs++)
    {
        FCS_SUMME += msg_buffer[ifcs];
    }
    
    // FCS
    msg_buffer[inext] = (FCS_SUMME >> 8) & 0xFF;
    inext++;
    msg_buffer[inext] = FCS_SUMME & 0xFF;
    inext++;

    // _GW_ID   nur für 2.0 -> 4.0
    msg_buffer[inext] = (_GW_ID) & 0xFF;
    inext++;
    msg_buffer[inext] = (_GW_ID >> 8) & 0xFF;
    inext++;
    msg_buffer[inext] = (_GW_ID >> 16) & 0xFF;
    inext++;
    msg_buffer[inext] = (_GW_ID >> 24) & 0xFF;
    inext++;

    if(bDEBUG)
    {
        printBuffer(msg_buffer, inext);
        Serial.println("");
    }

    ringBuffer[iWrite][0]=inext;
    memcpy(ringBuffer[iWrite]+1, msg_buffer, inext);
    
    iWrite++;
    if(iWrite > MAX_RING)
        iWrite=0;

    DEBUG_MSG("RADIO", "Packet sent to -->");
}

void commandAction(char *msg_text, int len)
{
    // -info
    // -set-owner

    char _owner_c[20];

    // copying the contents of the
    // string to char array
    strcpy(_owner_c, _owner_call.c_str());

    bool bInfo=false;

    if(memcmp(msg_text, "-info", 5) == 0)
    {
        sprintf(_owner_c, "%s", g_meshcom_settings.node_call);
        _owner_call = _owner_c;

        sprintf(_owner_c, "%s", g_meshcom_settings.node_short);
        _owner_short = _owner_c;

        bInfo=true;
    }
    else
    if(memcmp(msg_text, "-set_owner ", 11) == 0)
    {
        sprintf(_owner_c, "%s", msg_text+11);
        _owner_call = _owner_c;
        _owner_call.toUpperCase();
        sprintf(_owner_c, "%s", _owner_call.c_str());

        memcpy(g_meshcom_settings.node_call, _owner_c, 10);

        sprintf(_owner_c, "XXX40");

        for(int its=11; its<21; its++)
        {
            if(msg_text[its] == '-')
            {
                memcpy(_owner_c, msg_text+its-3, 3);
                _owner_c[3]=0x34;
                _owner_c[4]=0x30;
                _owner_c[5]=0x00;
                break;
            }
        }

        _owner_short = _owner_c;
        _owner_short.toUpperCase();
        sprintf(_owner_c, "%s", _owner_short.c_str());

        memcpy(g_meshcom_settings.node_short, _owner_c, 6);

        save_settings();

        bInfo=true;
    }

    if(bInfo)
    {
        printf("\nMeshCom 4.0 Client\n...Call:  %s\n...Short: %s\n...ID %08X\n...MAC %02X %02X %02X %02X %02X %02X\n...BATT %f\n...PBATT %d%%\n", _owner_call.c_str(), _owner_short.c_str(), _GW_ID, dmac[0], dmac[1], dmac[2], dmac[3], dmac[4], dmac[5], read_batt(), mv_to_percent(read_batt()));
    }
    else
        printf("\nMeshCom 4.0 Client\n...wrong command %s\n", msg_text);
}