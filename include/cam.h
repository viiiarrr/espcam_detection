#ifndef CAM_H
#define CAM_H

#include "esp_camera.h"
#include "Arduino.h"

// Pin definitions untuk ESP32-CAM (AI-Thinker model)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flash LED pin
#define FLASH_LED_PIN      4

// Fungsi untuk inisialisasi kamera
bool initCamera();

// Fungsi untuk menangkap gambar
camera_fb_t* captureImage();

// Fungsi untuk deteksi motion (untuk kompatibilitas dengan main.cpp yang ada)
void detectMotion();

// Fungsi untuk mengatur kualitas gambar
void setCameraQuality(int quality);

// Fungsi untuk mengatur ukuran frame
void setCameraFrameSize(framesize_t size);

// Fungsi untuk kontrol flash LED
void setFlashLED(bool state);

// Fungsi untuk menyimpan gambar ke Serial (base64 atau hex dump)
void printImageToSerial(camera_fb_t* fb);

#endif