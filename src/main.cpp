#include "cam.h"

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting ESP32-CAM...");
  
  if (!initCamera()) {
    Serial.println("Camera init failed!");
    while (1);
  }
  Serial.println("Camera initialized successfully!");
  
  // Start WiFi and Web Server
  startCameraWebServer();
  
  Serial.println("\nCommands available via Serial:");
  Serial.println("'c' - Capture single image");
  Serial.println("'q' + number - Set quality (0-63, lower = better)");
  Serial.println("'s' + number - Set frame size (0-12)");
  Serial.println("'f' - Toggle flash LED");
  Serial.println("'m' - Toggle motion detection");
  Serial.println();
}

bool motionDetectionEnabled = true;

void loop() {
  // Check for serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "c") {
      // Capture single image
      Serial.println("Capturing image...");
      setFlashLED(true);
      delay(100);
      camera_fb_t* fb = captureImage();
      setFlashLED(false);
      
      if (fb) {
        printImageToSerial(fb);
        esp_camera_fb_return(fb);
      }
    }
    else if (command.startsWith("q")) {
      // Set quality
      int quality = command.substring(1).toInt();
      if (quality >= 0 && quality <= 63) {
        setCameraQuality(quality);
      } else {
        Serial.println("Quality must be 0-63");
      }
    }
    else if (command.startsWith("s")) {
      // Set frame size
      int size = command.substring(1).toInt();
      if (size >= 0 && size <= 12) {
        setCameraFrameSize((framesize_t)size);
      } else {
        Serial.println("Frame size must be 0-12");
      }
    }
    else if (command == "f") {
      // Toggle flash
      static bool flash_state = false;
      flash_state = !flash_state;
      setFlashLED(flash_state);
      Serial.printf("Flash LED: %s\n", flash_state ? "ON" : "OFF");
    }
    else if (command == "m") {
      // Toggle motion detection
      motionDetectionEnabled = !motionDetectionEnabled;
      Serial.printf("Motion detection: %s\n", motionDetectionEnabled ? "ENABLED" : "DISABLED");
    }
  }
  
  // Continue motion detection if enabled
  if (motionDetectionEnabled) {
    detectMotion(); 
  }
  
  delay(500);     
}