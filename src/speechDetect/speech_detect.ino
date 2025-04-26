/*
  Synesthesia Wear - Wearable Device Code
*/

// ===== Defines =====
#define SOUND_1_ON 1
#define SOUND_1_OFF 2
#define SOUND_2_ON 3 
#define SOUND_2_OFF 4
#define SOUND_3_ON 5
#define SOUND_3_OFF 6
#define SOUND_4_ON 7
#define SOUND_4_OFF 8
#define CONF_LEV_80 80
#define CONF_LEV_85 85
#define CONF_LEV_90 90
#define CONF_LEV_95 95
#define CONF_LEV_98 98
#define EIDSP_QUANTIZE_FILTERBANK 0
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 3

// ===== Includes =====
#include <PDM.h>
#include <capstone26_inferencing.h>
#include <ArduinoBLE.h>

// ===== Global Variables =====
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static signed short *sampleBuffer;
static bool record_ready = false;
static bool debug_nn = false;
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);

static const int vibrationPatterns[6] = {0, 1, 2, 3, 4, 5};
static const int LED_PIN = LED_BUILTIN;
static const int VIBRATION_PIN = 3;

float confidenceThreshold = 0.85;

bool abraham_on = false;
bool alarm_on = false;
bool jordan_on = false;
bool smith_on = false;

// BLE Service and Characteristics
BLEService ledService("180A");
BLEByteCharacteristic switchCharacteristic("2A57", BLERead | BLEWrite);

// ===== Setup =====
void setup() {
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

  // Initialize Bluetooth
  if (!BLE.begin()) {
    while (1);
  }

  BLE.setLocalName("Nano 33 BLE Sense");
  BLE.setAdvertisedService(ledService);
  ledService.addCharacteristic(switchCharacteristic);
  BLE.addService(ledService);
  switchCharacteristic.writeValue(0);
  BLE.advertise();

  // Indicate Bluetooth ready
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(LEDR, HIGH);
  digitalWrite(LEDG, HIGH);  
  digitalWrite(LEDB, HIGH);
  
  // Initialize classifier and microphone
  run_classifier_init();
  microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE);
}

// ===== Vibration Patterns =====
void vibration(int type) {
  switch(type) {
    case 0: // Single short vibration
      digitalWrite(VIBRATION_PIN, HIGH);
      delay(100);
      digitalWrite(VIBRATION_PIN, LOW);
      delay(1000);
      break;

    case 1: // Double short vibration
      for (int i = 0; i < 2; i++) {
        digitalWrite(VIBRATION_PIN, HIGH);
        delay(25);
        digitalWrite(VIBRATION_PIN, LOW);
        delay(25);
      }
      delay(1000);
      break;

    case 2: // Single long vibration with LED
      digitalWrite(VIBRATION_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      delay(1000);
      digitalWrite(VIBRATION_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      delay(1000);
      break;

    case 3: // Triple short vibration with LED
      for (int i = 0; i < 3; i++) {
        digitalWrite(VIBRATION_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(VIBRATION_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        delay(200);
      }
      delay(1000);
      break;

    case 4: // Double long vibration with LED
      for (int i = 0; i < 2; i++) {
        digitalWrite(VIBRATION_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(VIBRATION_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        delay(200);
      }
      delay(1000);
      break;

    case 5: // Quadruple short vibration with LED
      for (int i = 0; i < 4; i++) {
        digitalWrite(VIBRATION_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(VIBRATION_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        delay(200);
      }
      delay(1000);
      break;
  }
}

// ===== Main Loop =====
void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    // Indicate BLE connection
    digitalWrite(LEDR, HIGH);
    digitalWrite(LEDG, HIGH);
    digitalWrite(LEDB, LOW);
    digitalWrite(LED_PIN, LOW);

    while (central.connected()) {
      if (switchCharacteristic.written()) {
        switch (switchCharacteristic.value()) {
          case SOUND_1_ON: abraham_on = true; break;
          case SOUND_1_OFF: abraham_on = false; break;
          case SOUND_2_ON: smith_on = true; break;
          case SOUND_2_OFF: smith_on = false; break;
          case SOUND_3_ON: jordan_on = true; break;
          case SOUND_3_OFF: jordan_on = false; break;
          case SOUND_4_ON: alarm_on = true; break;
          case SOUND_4_OFF: alarm_on = false; break;
          case CONF_LEV_80: confidenceThreshold = 0.8; break;
          case CONF_LEV_85: confidenceThreshold = 0.85; break;
          case CONF_LEV_90: confidenceThreshold = 0.9; break;
          case CONF_LEV_95: confidenceThreshold = 0.95; break;
          case CONF_LEV_98: confidenceThreshold = 0.98; break;
        }
        return;
      }

      if (!microphone_inference_record()) return;

      signal_t signal;
      signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
      signal.get_data = &microphone_audio_signal_get_data;
      ei_impulse_result_t result = {0};

      run_classifier_continuous(&signal, &result, debug_nn);

      if ((result.classification[0].value > confidenceThreshold) && abraham_on) {
        vibration(vibrationPatterns[2]);
      }
      else if ((result.classification[2].value > confidenceThreshold) && alarm_on) {
        vibration(vibrationPatterns[3]);
      }
      else if ((result.classification[3].value > confidenceThreshold) && jordan_on) {
        vibration(vibrationPatterns[4]);
      }
      else if ((result.classification[1].value > confidenceThreshold) && smith_on) {
        vibration(vibrationPatterns[5]);
      }
    }
  } 
  else {
    // No connection
    digitalWrite(LEDR, HIGH);
    digitalWrite(LEDG, HIGH);
    digitalWrite(LEDB, HIGH);
    digitalWrite(LED_PIN, HIGH);
  }
}

// ===== Microphone Functions =====

// Callback when PDM buffer is full
static void pdm_data_ready_inference_callback() {
  int bytesAvailable = PDM.available();
  int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

  if (record_ready) {
    for (int i = 0; i < bytesRead >> 1; i++) {
      inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

      if (inference.buf_count >= inference.n_samples) {
        inference.buf_select ^= 1;
        inference.buf_count = 0;
        inference.buf_ready = 1;
      }
    }
  }
}

// Initialize inference buffers and microphone
static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
  if (inference.buffers[0] == NULL) return false;

  inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
  if (inference.buffers[1] == NULL) {
    free(inference.buffers[0]);
    return false;
  }

  sampleBuffer = (signed short *)malloc((n_samples >> 1) * sizeof(signed short));
  if (sampleBuffer == NULL) {
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    return false;
  }

  inference.buf_select = 0;
  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  PDM.onReceive(&pdm_data_ready_inference_callback);
  PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));
  PDM.setGain(127);

  record_ready = true;
  return true;
}

// Record inference
static bool microphone_inference_record() {
  if (inference.buf_ready == 1) return false;
  while (inference.buf_ready == 0) delay(1);
  inference.buf_ready = 0;
  return true;
}

// Convert audio to signal for classifier
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
  numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
  return 0;
}