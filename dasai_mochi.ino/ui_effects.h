#ifndef UI_EFFECTS_H
#define UI_EFFECTS_H

#include <TFT_eSPI.h>
#include <vector>

// Cấu trúc để lưu thông tin của mỗi hạt mưa số
struct RainParticle {
    float x, y;
    float speed;
    char character;
};

// --- Biến toàn cục cho module hiệu ứng ---
#define NUM_RAIN_PARTICLES 100 // Tăng số lượng hạt để hiệu ứng dày hơn
static std::vector<RainParticle> rainParticles;

/**
 * @brief Khởi tạo các hạt mưa cho hiệu ứng nền.
 * Được gọi một lần duy nhất trong hàm setup().
 */
void initMatrixRain(TFT_eSPI* tft) {
    rainParticles.clear(); // Xóa dữ liệu cũ nếu có
    for (int i = 0; i < NUM_RAIN_PARTICLES; i++) {
        RainParticle p;
        p.x = random(0, tft->width());
        p.y = random(0, tft->height());
        p.speed = random(2, 6);
        p.character = (char)random(48, 58); // Ký tự số từ '0' đến '9'
        rainParticles.push_back(p);
    }
}

/**
 * @brief Cập nhật và vẽ hiệu ứng mưa số lên một sprite được chỉ định.
 * @param sprite Con trỏ đến sprite để vẽ lên.
 */
void drawMatrixRainBackground(TFT_eSPI* tft, TFT_eSprite* sprite) {
    // *** LOGIC MỚI ĐỂ MÔ PHỎNG HIỆU ỨNG FADE ***
    // Vẽ một hình chữ nhật đen bán trong suốt lên toàn bộ sprite.
    // Vì TFT_eSPI không hỗ trợ alpha blending, chúng ta sẽ vẽ một hình chữ nhật đen
    // để làm mờ đi một chút khung hình trước đó, tạo ra vệt mờ.
    // Cách này hiệu quả hơn việc xóa hoàn toàn màn hình.
    // (Lưu ý: Kỹ thuật này có thể không hoạt động trên tất cả các màn hình,
    // nếu có vấn đề, hãy thay thế bằng sprite->fillSprite(TFT_BLACK);)
    // for (int i = 0; i < 5; i++) { // Lặp lại để làm mờ nhiều hơn
    //     sprite->drawRect(0, 0, sprite->width(), sprite->height(), TFT_BLACK);
    // }

    sprite->fillRect(0, 0, sprite->width(), sprite->height(), TFT_BLACK);

    for (auto& p : rainParticles) {
        p.y += p.speed;

        if (p.y > tft->height()) {
            p.y = 0;
            p.x = random(0, tft->width());
        }

        sprite->drawChar(p.x, p.y, p.character, TFT_DARKGREEN, TFT_BLACK, 1);
    }
}

#endif // UI_EFFECTS_H
