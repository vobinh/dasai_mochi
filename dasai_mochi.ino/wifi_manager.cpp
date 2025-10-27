#include "wifi_manager.h"
#include "SPIFFS.h"
#include "ArduinoJson.h"
#include "globals.h" // Thêm dòng này để truy cập CONFIG_FILE và các định nghĩa khác

// --- CONFIGURATION ---
static const char* ssid     = "Mochi-Watch";
static const char* password = "12345678";   // ít nhất 8 ký tự

static WebServer server(80);
static File fsUploadFile;
static AppSettings* _settings; // Con trỏ để truy cập cài đặt chung
bool newBundleUploaded = false;

// Forward declarations
static void handleListFiles();
static void handleListAllFiles();
static void handleDelete();
static void handleDeleteAny();
static void handleSettings();
static void handleSaveSettings();

// --- UTILS ---
String formatBytes(size_t bytes) {
    if (bytes < 1024) return String(bytes) + " B";
    else if (bytes < (1024 * 1024)) return String(bytes / 1024.0, 2) + " KB";
    else return String(bytes / 1024.0 / 1024.0, 2) + " MB";
}

// =======================================================================================
// --- FILE UPLOAD (chuẩn theo code gốc, thêm logic video_custom.bin) ---
// =======================================================================================
static void handleFileUpload() {
    HTTPUpload& upload = server.upload();
    static String finalPath = "";

    if (upload.status == UPLOAD_FILE_START) {
        String originalFilename = upload.filename;
        int slash = max(originalFilename.lastIndexOf('/'), originalFilename.lastIndexOf('\\'));
        if (slash >= 0) originalFilename = originalFilename.substring(slash + 1);

        // Nếu là file .bin → đổi tên cố định
        if (originalFilename.endsWith(".bin")) {
            finalPath = "/video_custom.bin";
        } else {
            finalPath = "/ss_" + originalFilename;
        }

        if (SPIFFS.exists(finalPath)) {
            Serial.println("Overwriting existing file: " + finalPath);
            SPIFFS.remove(finalPath);
        }

        fsUploadFile = SPIFFS.open(finalPath, "w");
        if (!fsUploadFile) {
            Serial.println("!!! Failed to open file for write");
            return;
        }
        Serial.println("Upload Start: " + finalPath);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
            Serial.println("Upload End: " + String(upload.totalSize) + " bytes");
            if (upload.filename.endsWith(".bin")) {
                newBundleUploaded = true;
                Serial.println("Saved as video_custom.bin");
            }
        }
    }
}

// =======================================================================================
// --- FILE LIST PAGE (giao diện hiện đại, logic lọc ảnh giữ nguyên) ---
// =======================================================================================
static void handleListFiles() {
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    int percent = (totalBytes > 0) ? (usedBytes * 100) / totalBytes : 0;

    String html = R"rawliteral(
<!DOCTYPE html><html lang="vi"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Mochi Watch - File Manager</title>
<style>
:root{--bg:#0e1117;--surface:#161b22;--accent:#00c6a7;--danger:#f44336;--text:#e9e9e9;--muted:#888;--border: #30363d;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);margin:0;padding:1rem;}
.container{max-width:860px;margin:auto;}
h1{text-align:center;color:var(--accent);margin-bottom:.5rem;}
h2{color:var(--accent);margin-top:1.5rem;border-bottom:1px solid #333;padding-bottom:.25rem;}
.card{background:var(--surface);border: 1px solid var(--border); border-radius:12px;padding:1rem;margin-top:1rem;box-shadow:0 2px 10px rgba(0,0,0,.4);}
.tabs{display:flex;border-bottom:1px solid var(--border);margin-bottom:1rem;}
.tab{padding:10px 20px;cursor:pointer;border-bottom:2px solid transparent;}
.tab.active{color:var(--accent);border-bottom-color:var(--accent);}
.storage-bar{margin-top:.5rem;background:#222;border-radius:8px;height:18px;overflow:hidden;}
.storage-fill{height:100%;background:var(--accent);width:)rawliteral" + String(percent) + "%;}";

    html += R"rawliteral(.storage-info{text-align:center;color:var(--muted);font-size:.9rem;margin-top:.25rem;}
.upload-zone{border:2px dashed #444;border-radius:10px;text-align:center;padding:2rem;transition:border-color .3s;}
.upload-zone.dragover{border-color:var(--accent);}
.upload-btn{background:var(--accent);color:#000;border:none;padding:.8rem 1.5rem;border-radius:8px;font-weight:600;cursor:pointer;margin-top:1rem;}
.progress{margin-top:1rem;height:10px;background:#222;border-radius:5px;overflow:hidden;}
.progress-bar{height:100%;background:var(--accent);width:0%;transition:width .3s;}
.file-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:1rem;margin-top:1rem;}
.file-card{background:#222;border-radius:10px;overflow:hidden;text-align:center;transition:transform .2s;}
.file-card:hover{transform:scale(1.04);}
.file-card img{width:100%;height:120px;object-fit:cover;}
.file-card-info{padding:.5rem;}
.file-name{font-size:.9rem;font-weight:500;word-break:break-all;}
.file-size{font-size:.8rem;color:var(--muted);}
.delete-btn{background:var(--danger);border:none;padding:.4rem .8rem;border-radius:6px;color:#fff;font-size:.75rem;cursor:pointer;margin-top:.5rem;}
.video-section{text-align:center;background:#181a1f;border-radius:10px;padding:1rem;margin-top:2rem;}
.footer{text-align:center;color:#777;margin-top:2rem;font-size:.8rem;}
.link{color:var(--accent);text-decoration:none;font-size:.9rem;}
.link:hover{text-decoration:underline;}
.form-group{margin-bottom:1rem;}
.form-group label{display:block;margin-bottom:.5rem;color:var(--muted);}
.form-group input{width:calc(100% - 20px);background:#0d1117;border:1px solid var(--border);color:var(--text);padding:10px;border-radius:6px;}
.form-group input:focus{outline:none;border-color:var(--accent);}
.btn-save{background:var(--accent);color:#000;border:none;padding:.8rem 1.5rem;border-radius:8px;font-weight:600;cursor:pointer;margin-top:1rem;width:100%;}
</style></head><body>
<div class='container'>
<h1>📁 Mochi Watch Manager</h1>
<div class="tabs">
  <div class="tab active" onclick="showTab('files')">Quản lý File</div>
  <div class="tab" onclick="showTab('settings')">Cài đặt</div>
</div>
<div id="files-tab" class="tab-content">
  <div class='card'>
  <h2>💾 Storage</h2>
  <div class='storage-bar'><div class='storage-fill' style='width:)rawliteral" + String(percent) + "%;'></div></div>";
    html += "<div class='storage-info'>" + formatBytes(usedBytes) + " / " + formatBytes(totalBytes) + " used</div></div>";

    // --- Upload form ---
    html += R"rawliteral(
<div class="card">
  <h2>📤 Upload File</h2>
  <form id="upload-form" class="upload-zone" method="POST" action="/upload" enctype="multipart/form-data">
    <p>Chọn hoặc kéo thả file để tải lên</p>
    <input type="file" id="file-input" name="upload" hidden>
    <button type="button" class="upload-btn" onclick="document.getElementById('file-input').click()">Chọn File</button>
    <div class="progress"><div id="progress-bar" class="progress-bar"></div></div>
  </form>
</div>
)rawliteral";

    // --- Video file ---
    if (SPIFFS.exists("/video_custom.bin")) {
        File vf = SPIFFS.open("/video_custom.bin", "r");
        html += "<div class='video-section'><h2>🎬 Custom Video</h2>";
        html += "<div>video_custom.bin (" + formatBytes(vf.size()) + ")</div>";
        html += "<a href='/delete_any?file=/video_custom.bin' class='delete-btn'>Xóa video</a></div>";
        vf.close();
    }

    // --- Slideshow Images (giữ logic cũ) ---
    html += "<div class='card'><h2>🖼️ Slideshow Images</h2><div class='file-grid'>";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    bool foundImages = false;

    while (file) {
        String fileName = String(file.name());
        String checkName = fileName.startsWith("/") ? fileName.substring(1) : fileName;

        if (checkName.startsWith("ss_") && !file.isDirectory()) {
            foundImages = true;
            String fileNameOnly = checkName.substring(3);
            String fullPath = "/" + checkName;

            html += "<div class='file-card'>";
            html += "<img src='" + fullPath + "' loading='lazy'>";
            html += "<div class='file-card-info'>";
            html += "<div class='file-name'>" + fileNameOnly + "</div>";
            html += "<div class='file-size'>" + formatBytes(file.size()) + "</div>";
            html += "<a href='/delete?file=" + fullPath + "' class='delete-btn'>Xóa</a>";
            html += "</div></div>";
        }
        file = root.openNextFile();
    }
    // <div class="form-group">
    //     <label for="language">Ngôn ngữ</label>
    //     <select id="language" name="language">
    //       <option value="vi" )rawliteral" + String((_settings->language == "vi") ? "selected" : "") + R"rawliteral(>Tiếng Việt</option>
    //       <option value="en" )rawliteral" + String((_settings->language == "en") ? "selected" : "") + R"rawliteral(>English</option>
    //     </select>
    //   </div>

    if (!foundImages) html += "<p style='text-align:center;color:#888;'>Chưa có hình nào.</p>";
    html += "</div></div>";

    // --- Settings Tab ---
    html += R"rawliteral(
</div>
<div id="settings-tab" class="tab-content" style="display:none;">
  <div class="card">
    <h2>⚙️ Cài đặt Trạm thời tiết</h2>
    <form action="/save_settings" method="POST">
      <div class="form-group">
        <label for="ssid">Tên WiFi (SSID)</label>
        <input type="text" id="ssid" name="ssid" value=")rawliteral" + _settings->stationSsid + R"rawliteral(">
      </div>
      <div class="form-group">
        <label for="pass">Mật khẩu WiFi</label>
        <input type="password" id="pass" name="pass" value=")rawliteral" + _settings->stationPassword + R"rawliteral(">
      </div>
      <div class="form-group">
        <label for="apikey">OpenWeatherMap API Key</label>
        <input type="text" id="apikey" name="apikey" value=")rawliteral" + _settings->owmApiKey + R"rawliteral(">
      </div>
      <div class="form-group">
        <label for="cityid">OpenWeatherMap City ID</label>
        <input type="text" id="cityid" name="cityid" value=")rawliteral" + _settings->owmCityId + R"rawliteral(">
      </div>
      <div class="form-group">
        <label for="lat">Latitude</label>
        <input type="text" id="lat" name="lat" value=")rawliteral" + _settings->latitude + R"rawliteral(">
      </div>
      <div class="form-group">
        <label for="lon">Longitude</label>
        <input type="text" id="lon" name="lon" value=")rawliteral" +_settings->longitude + R"rawliteral(">
      </div>
      <div class="form-group">
        <label for="gmt">Múi giờ GMT (Giờ, ví dụ: 7, -5)</label>
        <input type="number" id="gmt" name="gmt" value=")rawliteral" + String(_settings->gmtOffsetHours) + R"rawliteral(" min="-12" max="14" step="1" required>
      </div>
      <button type="submit" class="btn-save">Lưu Cài Đặt</button>
    </form>
  </div>
</div>
)rawliteral";

    html += R"rawliteral(
<div class="footer">
  <a class="link" href="/all">Xem tất cả file (Debug)</a><br>
  Mochi Watch © 2025
</div>
</div>
<script>
function showTab(tabName) {
  document.querySelectorAll('.tab-content').forEach(t => t.style.display = 'none');
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(tabName + '-tab').style.display = 'block';
  event.currentTarget.classList.add('active');
}
document.addEventListener('DOMContentLoaded', () => {
  document.querySelector('.tab').click();
});
const form=document.getElementById('upload-form');
const input=document.getElementById('file-input');
const bar=document.getElementById('progress-bar');
form.addEventListener('dragover',e=>{e.preventDefault();form.classList.add('dragover');});
form.addEventListener('dragleave',()=>form.classList.remove('dragover'));
form.addEventListener('drop',e=>{
 e.preventDefault();form.classList.remove('dragover');
 input.files=e.dataTransfer.files;form.submit();
});
input.addEventListener('change', () => {
  if (input.files.length > 0) {
    document.getElementById('upload-form').submit();
  }
});
form.addEventListener('submit',e=>{
 e.preventDefault();
 const file=input.files[0];if(!file)return;
 const xhr=new XMLHttpRequest();
 xhr.open('POST','/upload',true);
 xhr.upload.onprogress=e=>{
   if(e.lengthComputable){bar.style.width=(e.loaded/e.total*100)+'%';}
 };
 xhr.onload=()=>{bar.style.width='0%';location.reload();};
 const fd=new FormData();fd.append('upload',file);xhr.send(fd);
});
</script></body></html>
)rawliteral";

    server.send(200, "text/html", html);
}

// =======================================================================================
// --- ALL FILES (debug page đẹp) ---
// =======================================================================================
static void handleListAllFiles() {
    size_t total = SPIFFS.totalBytes(), used = SPIFFS.usedBytes();
    int percent = (total > 0) ? (used * 100) / total : 0;

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>All Files</title>"
                  "<style>body{font-family:'Inter',system-ui;background:#0e1117;color:#eee;margin:0;padding:1rem;}"
                  "h1{text-align:center;color:#00c6a7;}table{width:100%;border-collapse:collapse;margin-top:1rem;}"
                  "th,td{border-bottom:1px solid #333;padding:8px;text-align:left;}tr:hover{background:#181b22;}"
                  ".btn{background:#f44336;color:white;border:none;padding:4px 10px;border-radius:5px;cursor:pointer;font-size:.8rem;}"
                  ".storage{margin-top:1rem;background:#222;border-radius:8px;height:16px;overflow:hidden;}"
                  ".fill{height:100%;background:#00c6a7;width:" + String(percent) + "%;}</style></head><body>";

    html += "<h1>All Files (Debug)</h1><div class='storage'><div class='fill'></div></div>";
    html += "<p style='text-align:center;margin:.5rem 0;'>Used: " + formatBytes(used) + " / " + formatBytes(total) + "</p>";
    html += "<table><tr><th>Path</th><th>Size</th><th>Action</th></tr>";

    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        String path = String(file.name());
        html += "<tr><td>" + path + "</td><td>" + formatBytes(file.size()) +
                "</td><td><a href='/delete_any?file=" + path + "'><button class='btn'>Xóa</button></a></td></tr>";
        file = root.openNextFile();
    }

    html += "</table><p style='text-align:center;margin-top:1rem;'><a href='/list' style='color:#00c6a7;'>⬅ Quay lại</a></p></body></html>";
    server.send(200, "text/html", html);
}

// =======================================================================================
// --- DELETE HANDLERS ---
// =======================================================================================
static void handleDelete() {
    if (!server.hasArg("file")) { server.send(400, "text/plain", "File missing"); return; }
    String path = server.arg("file");
    if (path.startsWith("/ss_") && path.indexOf("..") == -1) {
        if (SPIFFS.remove(path)) server.sendHeader("Location", "/list", true);
        else server.send(500, "text/plain", "Delete failed");
        server.send(302, "text/plain", "");
    } else server.send(400, "text/plain", "Invalid path");
}

static void handleDeleteAny() {
    if (!server.hasArg("file")) { server.send(400, "text/plain", "File missing"); return; }
    String path = server.arg("file");
    if (path.indexOf("..") == -1 && path.startsWith("/")) {
        SPIFFS.remove(path);
        server.sendHeader("Location", "/all", true);
        server.send(302, "text/plain", "");
    } else server.send(400, "text/plain", "Invalid path");
}

static void handleSaveSettings() {
    bool changed = false;
    if (server.hasArg("ssid") && server.arg("ssid") != _settings->stationSsid) {
        _settings->stationSsid = server.arg("ssid");
        changed = true;
    }
    if (server.hasArg("pass") && server.arg("pass") != _settings->stationPassword) {
        _settings->stationPassword = server.arg("pass");
        changed = true;
    }
    if (server.hasArg("apikey") && server.arg("apikey") != _settings->owmApiKey) {
        _settings->owmApiKey = server.arg("apikey");
        changed = true;
    }
    if (server.hasArg("cityid") && server.arg("cityid") != _settings->owmCityId) {
        _settings->owmCityId = server.arg("cityid");
        changed = true;
    }

    if (server.hasArg("lat") && server.arg("lat") != _settings->latitude) {
        _settings->latitude = server.arg("lat");
        changed = true;
    }
    if (server.hasArg("lon") && server.arg("lon") != _settings->longitude) {
        _settings->longitude = server.arg("lon");
        changed = true;
    }

    if (server.hasArg("gmt")) {
        int new_gmt = server.arg("gmt").toInt(); // Chuyển đổi sang số nguyên
        // Validate giá trị GMT (ví dụ: từ -12 đến +14)
        if (new_gmt >= -12 && new_gmt <= 14) {
             if (new_gmt != _settings->gmtOffsetHours) {
                _settings->gmtOffsetHours = new_gmt;
                changed = true;
                Serial.printf("GMT Offset updated to: %d\n", new_gmt); // Log
             }
        } else {
            Serial.printf("Warning: Invalid GMT Offset value received: %s\n", server.arg("gmt").c_str());
        }
    }

    if (changed) {
        wifi_manager_save_settings(); // Gọi hàm lưu file config
    }
    server.sendHeader("Location", "/list", true);
    server.send(302, "text/plain", "");
}

// =======================================================================================
// --- WIFI MANAGER PUBLIC API ---
// =======================================================================================
void wifi_manager_init(AppSettings* settings) {
    _settings = settings;
    if (!SPIFFS.begin(true)) Serial.println("SPIFFS mount failed!");

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/list", true);
        server.send(302, "text/plain", "");
    });
    server.on("/upload", HTTP_POST, []() {
        server.sendHeader("Location", "/list", true);
        server.send(302, "text/plain", "");
    }, handleFileUpload);
    server.on("/list", HTTP_GET, handleListFiles); // Trang chính giờ là /list
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/all", HTTP_GET, handleListAllFiles);
    server.on("/delete_any", HTTP_GET, handleDeleteAny);
    server.on("/save_settings", HTTP_POST, handleSaveSettings);

    // Serve static images
    server.onNotFound([]() {
        String path = server.uri();
        if (SPIFFS.exists(path)) {
            String contentType = "image/jpeg";
            File file = SPIFFS.open(path, "r");
            server.streamFile(file, contentType);
            file.close();
        } else server.send(404, "text/plain", "Not Found");
    });
}

bool wifi_manager_connect() {
    Serial.println("Configuring Access Point...");
    if (!WiFi.softAP(ssid, password)) return false;
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    Serial.println("AP started: " + WiFi.softAPIP().toString());
    server.begin();
    return true;
}

void wifi_manager_disconnect() {
    server.stop(); WiFi.softAPdisconnect(true);
    Serial.println("Access Point stopped.");
}

void wifi_manager_loop() { server.handleClient(); }
String wifi_manager_get_ip() { return WiFi.softAPIP().toString(); }

// Hàm lưu cài đặt, được gọi từ dasai_mochi.ino.ino
void wifi_manager_save_settings() {
    Serial.println("Saving settings from WiFi Manager...");
    File configFile = SPIFFS.open(CONFIG_FILE, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return;
    }

    StaticJsonDocument<2048> doc;
    // Ghi lại tất cả các cài đặt hiện có
    doc["frameDelay"] = _settings->frameDelay;
    doc["currentRotation"] = _settings->currentRotation;
    doc["language"] = _settings->currentLang;
    doc["notificationTimeout"] = _settings->notificationTimeout;
    doc["marqueeSpeed"] = _settings->marqueeSpeed;
    doc["currentAnalogFaceIndex"] = _settings->currentAnalogFaceIndex;
    doc["currentWeatherIndex"] = _settings->currentWeatherIndex;
    doc["soundEnabled"] = _settings->soundEnabled;
    doc["volume"] = _settings->volume;
    doc["musicAutoPlayNext"] = _settings->musicAutoPlayNext;
    doc["displayShape"] = (_settings->displayShape == SHAPE_SQUARE) ? "square" : "round";
    doc["bluetoothEnabled"] = _settings->bluetoothEnabled;
    doc["wifiEnabled"] = _settings->wifiEnabled;
    doc["stationSsid"] = _settings->stationSsid;
    doc["stationPassword"] = _settings->stationPassword;
    doc["owmApiKey"] = _settings->owmApiKey;
    doc["owmCityId"] = _settings->owmCityId;
    doc["latitude"] = _settings->latitude;
    doc["longitude"] = _settings->longitude;
    doc["gmtOffsetHours"] = _settings->gmtOffsetHours;

    if (serializeJson(doc, configFile) == 0) {
        Serial.println(F("Failed to write to file"));
    }
    configFile.close();
}
