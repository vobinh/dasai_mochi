# Weather icon + label map (Chronos 0..7)

Files:
- `weather_map.h` → labels + helper functions (getWeatherIconIndex, getWeatherLabel)
- `weather_icons.h` → 8 tiny 1‑bit icons (16×16) as placeholders

Usage (TFT_eSPI 240×240):
```cpp
#include <TFT_eSPI.h>
#include "weather_map.h"
#include "weather_icons.h"

void drawWeather(TFT_eSPI &tft, int iconIndex, int x, int y) {
  uint8_t idx = getWeatherIconIndex(iconIndex);

  // label
  char label[20];
  getWeatherLabel(idx, label, sizeof(label));

  // icon (1‑bit) -> white color
  const uint8_t* bmp = (const uint8_t*)pgm_read_ptr(&WEATHER_ICON_PTRS[idx]);
  tft.drawBitmap(x, y, bmp, WX_W, WX_H, TFT_WHITE);

  tft.setCursor(x, y + WX_H + 4);
  tft.print(label);
}
```

To replace with real icons from wt32-standby:
- Swap arrays in `weather_icons.h` with your converted bitmaps/RGB565,
- Keep the same index order (0..7) so no code changes elsewhere.
