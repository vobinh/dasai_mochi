#include "wifi_manager.h"
#include "SPIFFS.h"

// --- CONFIGURATION ---
static const char* ssid     = "Mochi-Watch";
static const char* password = "12345678";   // ít nhất 8 ký tự

static WebServer server(80);
static File fsUploadFile;
bool newBundleUploaded = false;

// Forward declaration for the new debug handler
static void handleListAllFiles();

// --- HELPER FUNCTIONS ---
// Hàm tiện ích để định dạng bytes thành KB hoặc MB cho dễ đọc
String formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < (1024 * 1024)) {
        return String(bytes / 1024.0, 2) + " KB";
    } else {
        return String(bytes / 1024.0 / 1024.0, 2) + " MB";
    }
}

// Hàm tạo mã HTML cho phần hiển thị dung lượng
String getStorageHTML() {
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    int percentage = 0;
    if (totalBytes > 0) {
        percentage = (usedBytes * 100) / totalBytes;
    }
    String html = "<div class='card storage-info'>";
    html += "<h3>Storage</h3>";
    html += "<div class='progress-bar'><div class='progress-bar-fill' style='width: " + String(percentage) + "%;'>" + String(percentage) + "%</div></div>";
    html += "<p style='text-align:center; margin-top:0.5rem;'>" + formatBytes(usedBytes) + " / " + formatBytes(totalBytes) + " used</p>";
    html += "</div>";
    return html;
}


// =======================================================================================
// --- INTERNAL HANDLERS ---
// =======================================================================================
static void handleFileUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        
        String originalFilename = upload.filename;
        int slash = max(originalFilename.lastIndexOf('/'), originalFilename.lastIndexOf('\\'));
        if (slash >= 0) {
            originalFilename = originalFilename.substring(slash + 1);
        }

        String finalPath = "/ss_" + originalFilename;
        
        Serial.println("Attempting to open file for writing at: " + finalPath);
        fsUploadFile = SPIFFS.open(finalPath, "w");

        if (fsUploadFile) {
            Serial.println("SUCCESS: File opened. Path: " + String(fsUploadFile.name()));
        } else {
            Serial.println("!!! FAILED to open file at path: " + finalPath);
            return;
        }

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
            }
        }
    }
}

static void handleListFiles() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mochi Watch - File Manager</title>
    <style>
        :root { --bg-color: #121212; --surface-color: #1e1e1e; --primary-color: #03dac6; --primary-variant: #3700b3; --secondary-color: #bb86fc; --text-color: #e1e1e1; --error-color: #cf6679; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: var(--bg-color); color: var(--text-color); margin: 0; padding: 1rem; }
        .container { max-width: 800px; margin: auto; }
        h1 { color: var(--primary-color); text-align: center; }
        h2 { border-bottom: 1px solid #333; padding-bottom: 0.5rem; margin-top: 2rem;}
        .card { background-color: var(--surface-color); border-radius: 8px; padding: 1.5rem; margin-bottom: 1.5rem; box-shadow: 0 4px 8px rgba(0,0,0,0.2); }
        .storage-info h3 { margin-top: 0; color: var(--secondary-color); }
        .progress-bar { width: 100%; background-color: #333; border-radius: 5px; overflow: hidden; height: 20px; }
        .progress-bar-fill { height: 100%; background-color: var(--secondary-color); text-align: center; line-height: 20px; color: #121212; font-weight: bold; transition: width 0.5s ease-in-out; }
        .upload-form .form-group { margin-bottom: 1rem; }
        .file-input-wrapper { position: relative; overflow: hidden; display: inline-block; cursor: pointer; background-color: var(--primary-variant); color: white; padding: 0.8rem 1.2rem; border-radius: 5px; }
        .upload-form input[type="file"] { position: absolute; left: 0; top: 0; opacity: 0; cursor: pointer; height: 100%; width: 100%; }
        .upload-form #file-name { margin-left: 1rem; font-style: italic; color: #aaa; }
        .btn { background-color: var(--primary-color); color: var(--bg-color); padding: 0.8rem 1.5rem; border: none; border-radius: 5px; cursor: pointer; font-size: 1rem; font-weight: bold; transition: background-color 0.3s; text-decoration: none; display: inline-block;}
        .btn:hover { background-color: #018786; }
        .btn-danger { background-color: var(--error-color); color: white; padding: 0.5rem 1rem; font-size: 0.9rem;}
        .btn-danger:hover { background-color: #b00020; }
        .file-list { list-style: none; padding: 0; display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 1rem; }
        .file-card { background-color: #2a2a2a; border-radius: 8px; overflow: hidden; text-align: center; padding-bottom: 1rem; display: flex; flex-direction: column; justify-content: space-between;}
        .file-card img { width: 100%; height: 120px; object-fit: cover; background-color: #121212; }
        .file-card-info { padding: 0.5rem; }
        .file-card-name { font-weight: bold; word-break: break-all; margin-bottom: 0.25rem; }
        .file-card-size { font-size: 0.8rem; color: #aaa; margin-bottom: 0.75rem; }
        .footer { text-align: center; margin-top: 2rem; color: #888; }
    </style>
</head>
<body><div class="container">
    <h1>Mochi Watch</h1>
)rawliteral";
    
    html += getStorageHTML();

    html += R"rawliteral(
    <div class="card">
        <h2>Upload Image</h2>
        <form class="upload-form" method='POST' action='/upload' enctype='multipart/form-data'>
            <div class="form-group">
                <label class="file-input-wrapper">
                    Choose File
                    <input type='file' name='upload' id="file-upload-input" required>
                </label>
                <span id="file-name">No file chosen</span>
            </div>
            <input type='submit' value='Upload' class="btn">
        </form>
    </div>
    <div class="card">
        <h2>Slideshow Images</h2>
        <ul class="file-list">
)rawliteral";

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

            html += "<li class='file-card'><div>";
            html += "<img src='" + fullPath + "' loading='lazy'>";
            html += "<div class='file-card-info'>";
            html += "<div class='file-card-name'>" + fileNameOnly + "</div>";
            html += "<div class='file-card-size'>" + formatBytes(file.size()) + "</div>";
            html += "</div></div>";
            html += "<a href='/delete?file=" + fullPath + "' class='btn btn-danger'>Delete</a></li>";
        }
        file = root.openNextFile();
    }
    if (!foundImages) {
        html += "<p>No slideshow images found. Upload one to get started!</p>";
    }

    html += R"rawliteral(
        </ul>
    </div>
    <div class="footer">
        <a href='/all'>Debug: View All Files</a>
    </div>
</div>
<script>
    document.getElementById('file-upload-input').addEventListener('change', function() {
        var fileName = this.files[0] ? this.files[0].name : 'No file chosen';
        document.getElementById('file-name').textContent = fileName;
    });
</script>
</body></html>
)rawliteral";
    server.send(200, "text/html", html);
}

static void handleDelete() {
    if (server.hasArg("file")) {
        String path = server.arg("file"); // path nhận được là "/ss_somefile.jpg"
        
        if (path.startsWith("/ss_") && path.indexOf("..") == -1) {
            Serial.println("Attempting to delete slideshow file: [" + path + "]");
            if (SPIFFS.remove(path)) {
                Serial.println("Deleted: " + path);
                server.sendHeader("Location", "/list", true);
                server.send(302, "text/plain", "");
            } else {
                Serial.println("!!! FAILED to delete: [" + path + "]");
                server.send(500, "text/plain", "Delete failed.");
            }
        } else {
            server.send(400, "text/plain", "Invalid path for this operation.");
        }
    } else {
        server.send(400, "text/plain", "File argument missing.");
    }
}

// --- NEW DEBUG HANDLERS ---
static void handleListAllFiles() {
    String html = "<html><head><title>All Files (Debug)</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif; background-color:#222; color:#fff;} a{color:#0af;} .delete-btn{color:#f44;} .storage-info{background-color:#330; border: 1px solid #ffc107; padding: 10px; margin-bottom: 15px; border-radius: 5px;}</style>";
    html += "</head><body>";
    
    html += getStorageHTML();

    html += "<h1>All Files (Debug)</h1><ul>";

    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    bool foundFiles = false;
    while (file) {
        foundFiles = true;
        String fullPath = String(file.name());
        if (!fullPath.startsWith("/")) {
            fullPath = "/" + fullPath;
        }
        html += "<li>" + fullPath + " (" + formatBytes(file.size()) + ") ";
        html += "<a href='/delete_any?file=" + fullPath + "' class='delete-btn'>[Delete]</a></li>";
        file = root.openNextFile();
    }
     if (!foundFiles) {
        html += "<li>No files found on filesystem.</li>";
    }
    html += "</ul><br><a href='/list'>Back to Image Manager</a></body></html>";
    server.send(200, "text/html", html);
}

static void handleDeleteAny() {
    if (server.hasArg("file")) {
        String path = server.arg("file"); // path nhận được là "/somefile.jpg"

        if (path.indexOf("..") == -1 && path.length() > 0 && path.startsWith("/")) {
            Serial.println("Attempting to delete ANY file: [" + path + "]");
            if (SPIFFS.remove(path)) {
                Serial.println("DEBUG: Deleted file: " + path);
                server.sendHeader("Location", "/all", true);
                server.send(302, "text/plain", "");
            } else {
                Serial.println("!!! DEBUG: FAILED to delete file: [" + path + "]");
                server.send(500, "text/plain", "Delete failed.");
            }
        } else {
            server.send(400, "text/plain", "Invalid path.");
        }
    } else {
        server.send(400, "text/plain", "File argument missing.");
    }
}


// =======================================================================================
// --- PUBLIC FUNCTIONS ---
// =======================================================================================
void wifi_manager_init() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed!");
    }

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/list", true);
        server.send(302, "text/plain", "");
    });
    server.on("/upload", HTTP_POST, []() {
        server.sendHeader("Location", "/list", true);
        server.send(302, "text/plain", "");
    }, handleFileUpload);
    
    // Main image routes
    server.on("/list", HTTP_GET, handleListFiles);
    server.on("/delete", HTTP_GET, handleDelete);

    // New debug routes
    server.on("/all", HTTP_GET, handleListAllFiles);
    server.on("/delete_any", HTTP_GET, handleDeleteAny);

    // Serve static files (jpg preview)
    server.onNotFound([]() {
        String path = server.uri();
        if (SPIFFS.exists(path)) {
            String contentType = "image/jpeg";
            File file = SPIFFS.open(path, "r");
            server.streamFile(file, contentType);
            file.close();
        } else {
            server.send(404, "text/plain", "Not Found");
        }
    });
}

bool wifi_manager_connect() {
    Serial.println("Configuring Access Point...");

    if (!WiFi.softAP(ssid, password)) {
        Serial.println("AP start FAILED");
        return false;
    }

    WiFi.softAPConfig(IPAddress(192,168,4,1),
                      IPAddress(192,168,4,1),
                      IPAddress(255,255,255,0));

    Serial.print("AP SSID: "); Serial.println(ssid);
    Serial.print("Password: "); Serial.println(password);
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

    server.begin();
    Serial.println("Web Server started.");
    return true;
}

void wifi_manager_disconnect() {
    server.stop();
    WiFi.softAPdisconnect(true);
    Serial.println("Access Point stopped.");
}

void wifi_manager_loop() {
    server.handleClient();
}

String wifi_manager_get_ip() {
    return WiFi.softAPIP().toString();
}

