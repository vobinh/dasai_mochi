#include "wifi_manager.h"
#include "SPIFFS.h"

// --- CONFIGURATION ---
static const char* ssid     = "Mochi-Watch";
static const char* password = "12345678";   // ít nhất 8 ký tự

static WebServer server(80);
static File fsUploadFile;
bool newBundleUploaded = false;

// Forward declarations
static void handleListFiles();
static void handleListAllFiles();
static void handleDelete();
static void handleDeleteAny();

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
:root{--bg:#0e1117;--surface:#181b22;--accent:#00c6a7;--danger:#f44336;--text:#e9e9e9;--muted:#888;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);margin:0;padding:1rem;}
.container{max-width:860px;margin:auto;}
h1{text-align:center;color:var(--accent);margin-bottom:.5rem;}
h2{color:var(--accent);margin-top:1.5rem;border-bottom:1px solid #333;padding-bottom:.25rem;}
.card{background:var(--surface);border-radius:12px;padding:1rem;margin-top:1rem;box-shadow:0 2px 10px rgba(0,0,0,.4);}
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
</style></head><body>
<div class='container'>
<h1>📁 Mochi Watch File Manager</h1>
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

    if (!foundImages) html += "<p style='text-align:center;color:#888;'>Chưa có hình nào.</p>";
    html += "</div></div>";

    html += R"rawliteral(
<div class="footer">
  <a class="link" href="/all">Xem tất cả file (Debug)</a><br>
  Mochi Watch © 2025
</div>
</div>
<script>
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

// =======================================================================================
// --- WIFI MANAGER PUBLIC API ---
// =======================================================================================
void wifi_manager_init() {
    if (!SPIFFS.begin(true)) Serial.println("SPIFFS mount failed!");

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/list", true);
        server.send(302, "text/plain", "");
    });
    server.on("/upload", HTTP_POST, []() {
        server.sendHeader("Location", "/list", true);
        server.send(302, "text/plain", "");
    }, handleFileUpload);
    server.on("/list", HTTP_GET, handleListFiles);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/all", HTTP_GET, handleListAllFiles);
    server.on("/delete_any", HTTP_GET, handleDeleteAny);

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
