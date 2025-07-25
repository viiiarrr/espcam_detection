#include "cam.h"

// Variabel global untuk menyimpan frame buffer sebelumnya (untuk deteksi motion)
camera_fb_t* prev_fb = nullptr;

// Web server instance
httpd_handle_t camera_httpd = NULL;

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

bool initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
    return false;
  }
}

// Handler untuk stream video
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, "\r\n--frame\r\n", 10);
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

// Handler untuk capture single photo
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  
  // Flash LED untuk foto
  setFlashLED(true);
  delay(100);
  
  fb = esp_camera_fb_get();
  
  setFlashLED(false);
  
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// Handler untuk halaman utama
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Web Server</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial; text-align: center; margin: 0; }
        .container { padding: 20px; }
        img { max-width: 100%; height: auto; }
        button { padding: 10px 20px; margin: 10px; font-size: 16px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32-CAM Web Server</h1>
        <h2>Live Stream</h2>
        <img id="stream" src="/stream">
        <br>
        <button onclick="capture()">Capture Photo</button>
        <button onclick="toggleFlash()">Toggle Flash</button>
        <br>
        <h2>Last Captured Photo</h2>
        <img id="photo" style="display: none;">
    </div>
    
    <script>
        function capture() {
            var photo = document.getElementById('photo');
            photo.src = '/capture?' + new Date().getTime();
            photo.style.display = 'block';
        }
        
        function toggleFlash() {
            fetch('/flash');
        }
    </script>
</body>
</html>
)rawliteral";

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

// Handler untuk toggle flash
static esp_err_t flash_handler(httpd_req_t *req) {
  static bool flash_state = false;
  flash_state = !flash_state;
  setFlashLED(flash_state);
  
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, flash_state ? "Flash ON" : "Flash OFF", -1);
}

bool initWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t flash_uri = {
    .uri       = "/flash",
    .method    = HTTP_GET,
    .handler   = flash_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &flash_uri);
    Serial.println("Web server started!");
    return true;
  }
  
  Serial.println("Failed to start web server!");
  return false;
}

void startCameraWebServer() {
  if (initWiFi()) {
    if (initWebServer()) {
      Serial.println("=== ESP32-CAM Web Server Ready ===");
      Serial.print("Open browser and go to: http://");
      Serial.println(WiFi.localIP());
      Serial.println("Available endpoints:");
      Serial.println("  / - Main page with live stream");
      Serial.println("  /stream - Live video stream");
      Serial.println("  /capture - Capture single photo");
      Serial.println("  /flash - Toggle flash LED");
    }
  }
}

// ...existing camera functions...