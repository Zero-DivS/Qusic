#include <math.h>

// Core parameters
const int ADC0_PIN = A0;
const int ADC1_PIN = A1;
const int BUFFER_SIZE = 1024;

// Photodiode calibration parameters
float adc0Offset = 0.0;
float adc0Scale = 1.0;
float adc1Offset = 0.0;
float adc1Scale = 1.0;
uint32_t lastCalibrationTime = 0;
const uint32_t RECALIBRATION_INTERVAL = 15 * 60 * 1000; // 15 minutes

// Entropy pool
uint32_t entropyPool[4] = {0xA5A5A5A5, 0x5A5A5A5A, 0x3C3C3C3C, 0xC3C3C3C3};
uint8_t poolIndex = 0;

// Streamlined calibration
void calibratePhotodiodes() {
  uint32_t adc0Sum = 0, adc1Sum = 0;
  uint32_t adc0SqSum = 0, adc1SqSum = 0;
  
  for (int i = 0; i < 300; i++) {
    uint16_t adc0Sample = analogRead(ADC0_PIN);
    uint16_t adc1Sample = analogRead(ADC1_PIN);
    
    adc0Sum += adc0Sample;
    adc1Sum += adc1Sample;
    adc0SqSum += adc0Sample * adc0Sample;
    adc1SqSum += adc1Sample * adc1Sample;
    
    delayMicroseconds(1);
  }
  
  // Calculate statistics
  float adc0Avg = (float)adc0Sum / 300;
  float adc1Avg = (float)adc1Sum / 300;
  float adc0StdDev = sqrt((float)adc0SqSum/300 - adc0Avg*adc0Avg);
  float adc1StdDev = sqrt((float)adc1SqSum/300 - adc1Avg*adc1Avg);
  
  adc0Offset = adc0Avg - 512.0;
  adc0Scale = max(1024.0 / (6.0 * adc0StdDev), 0.5);
  
  adc1Offset = adc1Avg - 512.0;
  adc1Scale = max(1024.0 / (6.0 * adc1StdDev), 0.5);
  
  adc0Scale = constrain(adc0Scale, 0.2, 5.0);
  adc1Scale = constrain(adc1Scale, 0.2, 5.0);
  
  lastCalibrationTime = millis();
  
  // Mix calibration data into entropy pool
  entropyPool[0] ^= adc0Sum;
  entropyPool[1] ^= adc1Sum;
  entropyPool[2] ^= (uint32_t)(adc0StdDev * 1000);
  entropyPool[3] ^= (uint32_t)(adc1StdDev * 1000);
}

// Improved entropy collection
void collectEntropy() {
  static uint32_t lastTime = 0;
  uint32_t now = micros();
  uint32_t delta = now - lastTime;
  lastTime = now;
  
  // Read analog inputs with variable timing
  delayMicroseconds((now & 0x3) + 1);
  uint16_t sample1 = analogRead(ADC0_PIN);
  
  // Variable delay based on the sample itself
  delayMicroseconds((sample1 & 0x3) + 1);
  uint16_t sample2 = analogRead(ADC1_PIN);
  
  // Update entropy pool with complex mixing
  entropyPool[poolIndex] ^= delta;
  entropyPool[poolIndex] = (entropyPool[poolIndex] << 7) | (entropyPool[poolIndex] >> 25);
  entropyPool[poolIndex] ^= sample1;
  entropyPool[poolIndex] = (entropyPool[poolIndex] << 5) | (entropyPool[poolIndex] >> 27);
  entropyPool[poolIndex] ^= sample2;
  entropyPool[poolIndex] = (entropyPool[poolIndex] << 11) | (entropyPool[poolIndex] >> 21);
  entropyPool[poolIndex] ^= entropyPool[(poolIndex + 2) % 4]; // Cross-mix with another pool element
  
  // Increment pool index
  poolIndex = (poolIndex + 1) % 4;
}

// Extract a high-quality random byte from the entropy pool
uint8_t getRandomByte() {
  // Collect new entropy
  collectEntropy();
  
  // Mix pool further
  uint32_t mix = entropyPool[0] ^ entropyPool[1] ^ entropyPool[2] ^ entropyPool[3];
  mix ^= (mix << 13);
  mix ^= (mix >> 17);
  mix ^= (mix << 5);
  
  // Return 8 bits from the mix
  return (mix >> ((entropyPool[poolIndex] & 0x1F) % 24)) & 0xFF;
}

// Generate a random value from 1-9 or A-I with rejection sampling to avoid bias
char generateRandomCharacter() {
  // Use rejection sampling to avoid modulo bias
  // We now want 0-17 (18 possible values), so reject values >= 252 (252 % 18 = 0)
  uint8_t rand;
  do {
    rand = getRandomByte();
  } while (rand >= 252);
  
  // Now we can safely use modulo without bias, get a number from 0-17
  uint8_t value = rand % 18;
  
  // If value is 0-8, return '1'-'9'
  // If value is 9-17, return 'A'-'I'
  if (value < 9) {
    return '1' + value;  // Convert to characters '1' through '9'
  } else {
    return 'A' + (value - 9);  // Convert to characters 'A' through 'I'
  }
}

void setup() {
  Serial.begin(230400);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Configure ADC for maximum speed
  #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    ADCSRA = (ADCSRA & 0xF8) | 0x04; // Set ADC prescaler to 16 (faster sampling)
  #endif
  
  // Configure pins
  pinMode(ADC0_PIN, INPUT);
  pinMode(ADC1_PIN, INPUT);
  
  // Initial calibration and entropy collection
  calibratePhotodiodes();
  
  // Pre-fill entropy pool
  for (int i = 0; i < 32; i++) {
    collectEntropy();
    delay(1);
  }
}

void loop() {
  // Generate and output a random character (1-9 or A-I)
  char randomChar = generateRandomCharacter();
  Serial.print(randomChar);
  
  // Add a small delay to control output rate
  delay(100);
  
  // Periodic calibration
  uint32_t now = millis();
  if (now - lastCalibrationTime > RECALIBRATION_INTERVAL) {
    calibratePhotodiodes();
  }
}

// Function to test randomness distribution
// Uncomment and call this from setup() to verify the distribution
/*
void testRandomDistribution() {
  long counts[18] = {0}; // We'll count occurrences of 1-9 and A-I
  const long samples = 1800;
  
  Serial.println("Testing random distribution:");
  
  for(long i = 0; i < samples; i++) {
    char c = generateRandomCharacter();
    int index;
    
    // Map the character to an array index
    if (c >= '1' && c <= '9') {
      index = c - '1'; // 0-8 for '1'-'9'
    } else {
      index = 9 + (c - 'A'); // 9-17 for 'A'-'I'
    }
    
    counts[index]++;
    
    // Print progress every 100 samples
    if(i % 100 == 99) {
      Serial.print(".");
    }
  }
  
  Serial.println("\nResults:");
  
  // Print results for numbers 1-9
  for(int i = 0; i < 9; i++) {
    char c = '1' + i;
    Serial.print(c);
    Serial.print(": ");
    Serial.print(counts[i]);
    Serial.print(" (");
    Serial.print((float)counts[i]/samples*100);
    Serial.println("%)");
  }
  
  // Print results for letters A-I
  for(int i = 0; i < 9; i++) {
    char c = 'A' + i;
    Serial.print(c);
    Serial.print(": ");
    Serial.print(counts[i+9]);
    Serial.print(" (");
    Serial.print((float)counts[i+9]/samples*100);
    Serial.println("%)");
  }
}
*/