#include "cam.h"

// Variabel global untuk menyimpan frame buffer sebelumnya (untuk deteksi motion)
camera_fb_t* prev_fb = nullptr;

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Inisialisasi dengan resolusi sedang untuk performa yang baik
  config.frame_size = FRAMESIZE_SVGA; // 800x600
  config.jpeg_quality = 10; // 0-63, nilai lebih rendah = kualitas lebih baik
  config.fb_count = 2; // Jumlah frame buffer
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  // Inisialisasi flash LED sebagai output
  pinMode(FLASH_LED_PIN, OUTPUT);
  setFlashLED(false);

  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  // Pengaturan sensor tambahan
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    // Flip vertical jika diperlukan
    s->set_vflip(s, 0);
    // Flip horizontal jika diperlukan  
    s->set_hmirror(s, 0);
    // Pengaturan kontras, brightness, dll bisa ditambahkan di sini
  }

  Serial.println("Camera initialized successfully!");
  return true;
}

camera_fb_t* captureImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return nullptr;
  }
  
  Serial.printf("Image captured: %dx%d, size: %d bytes\n", 
                fb->width, fb->height, fb->len);
  return fb;
}

void detectMotion() {
  // Ambil gambar saat ini
  camera_fb_t* current_fb = captureImage();
  if (!current_fb) {
    return;
  }

  // Jika ada frame sebelumnya, bandingkan untuk deteksi motion
  if (prev_fb) {
    // Algoritma sederhana: bandingkan ukuran file JPEG
    // Untuk algoritma yang lebih sophisticated, perlu konversi ke grayscale
    int size_diff = abs((int)current_fb->len - (int)prev_fb->len);
    float motion_threshold = 0.05; // 5% perubahan ukuran
    
    if (size_diff > (prev_fb->len * motion_threshold)) {
      Serial.println("Motion detected!");
      
      // Nyalakan flash sebagai indikator
      setFlashLED(true);
      delay(100);
      setFlashLED(false);
      
      // Tampilkan informasi gambar
      printImageToSerial(current_fb);
    }
    
    // Bebaskan frame buffer sebelumnya
    esp_camera_fb_return(prev_fb);
  }
  
  // Simpan frame saat ini sebagai referensi untuk perbandingan berikutnya
  prev_fb = current_fb;
}

void setCameraQuality(int quality) {
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_quality(s, quality);
    Serial.printf("Camera quality set to: %d\n", quality);
  }
}

void setCameraFrameSize(framesize_t size) {
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, size);
    Serial.printf("Camera frame size changed\n");
  }
}

void setFlashLED(bool state) {
  digitalWrite(FLASH_LED_PIN, state ? HIGH : LOW);
}

void printImageToSerial(camera_fb_t* fb) {
  if (!fb) return;
  
  Serial.println("=== IMAGE DATA ===");
  Serial.printf("Width: %d\n", fb->width);
  Serial.printf("Height: %d\n", fb->height);
  Serial.printf("Size: %d bytes\n", fb->len);
  Serial.printf("Format: %s\n", (fb->format == PIXFORMAT_JPEG) ? "JPEG" : "Other");
  
  // Tampilkan beberapa byte pertama dalam format hex (untuk debugging)
  Serial.print("First 32 bytes (hex): ");
  for (int i = 0; i < 32 && i < fb->len; i++) {
    Serial.printf("%02X ", fb->buf[i]);
  }
  Serial.println();
  
  // Untuk transmisi data lengkap, Anda bisa menggunakan base64 encoding
  // atau mengirim raw data melalui Serial
  Serial.println("==================");
}