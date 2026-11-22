# ATS-20 / ATS-20+ Custom Firmware (V17 Mod)

Firmware personalizado para receptores **ATS-20 / ATS-20+** (Arduino Nano + SI4732/SI4735 + OLED SSD1306), basado en el diseño clásico de **PU2CLR / RA4NAL (Brown Bear)** con tipografía grande y legible, optimizado con RDS mejorado en español, nuevos atajos de botones y mejoras en SSB.

*[English version below]*

---

## ✨ Novedades y Mejoras (Español)

* **Interfaz grande y clara:** Mantiene el diseño de caracteres grandes y alto contraste para pantallas OLED de 0,96" (`SSD1306` / `SH1106`).
* **RDS completo en FM con soporte de caracteres españoles/europeos:**
  * Muestra el nombre de la emisora (PS) y el mensaje de texto (RadioText) con desplazamiento suave.
  * Traducción automática de caracteres especiales (`Á, É, Í, Ó, Ú, Ü, Ñ, Ç`).
* **Indicador de batería calibrado:** Lectura en porcentaje real (`0% - 100%`) calibrada para baterías Li-Ion (`3.1V` – `4.0V`) a través del pin analógico **`A1`**.
* **Nuevos controles y atajos de botones:**
  * **`MODE` (pulsación corta):** Activa/desactiva el silencio por hardware (**Mute** / `MT`).
  * **`MODE` (pulsación larga > 0,6 s):** Cambia la modulación entre **AM / LSB / USB**.
  * **`BW` en modo FM:** Conmuta entre **Estéreo (`ST`)** y **Mono (`MN`)**.
  * **`VOL+` y `VOL-` simultáneos:** Guarda manualmente la banda, frecuencia y ajustes actuales en la memoria EEPROM (muestra `SAV` en pantalla).
* **Mejoras en Banda Lateral (SSB) y Onda Corta:**
  * Compensación automática de volumen (`+5`) al entrar en LSB/USB.
  * Sintonía continua mediante BFO sin cortes ni chasquidos cada 16 kHz.
  * Bandas de radioaficionado completas (160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m) + Banda Ciudadana (**CB 27 MHz**) y onda corta continua.
  * Medidor de señal (S-Meter) calibrado en 8 niveles.

### 🚀 Instalación rápida (sin compilar)
1. Descarga el archivo **`.ino.hex`** desde la sección **Releases** de este repositorio.
2. Conecta tu ATS-20+ por el puerto Mini-USB de datos.
3. Abre **AVRDUDESS**, selecciona `ATmega328P`, el puerto `COM` correspondiente, carga el `.ino.hex` y pulsa **Program!**.

### 🛠️ Compilación desde Arduino IDE (opcional)
Si deseas modificar y compilar el código fuente:
1. Asegúrate de que todos los archivos (`.ino`, `Buttons.h`, `Buttons.cpp`, `Rotary.h`, `Rotary.cpp`) estén dentro de una carpeta con el mismo nombre exacto que el archivo `.ino`.
2. Instala desde el Gestor de Librerías de Arduino IDE:
   * **`PU2CLR SI4735`** (de Ricardo Lima Caratti)
   * **`SSD1306Ascii`** (de Bill Greiman)

---

## ✨ Features & Improvements (English)

Custom firmware for **ATS-20 / ATS-20+** receivers (Arduino Nano + SI4732/SI4735 + SSD1306 OLED), based on the **PU2CLR / RA4NAL (Brown Bear)** architecture featuring large, easy-to-read fonts, enhanced RDS, improved SSB tuning, and new button shortcuts.

* **Large, high-contrast UI:** Keeps large digits and clear menus for 0.96" OLED displays (`SSD1306`).
* **Enhanced FM RDS:** Displays both Station Name (PS) and smooth-scrolling RadioText with automatic decoding for European/Spanish accents (`Á, É, Í, Ó, Ú, Ü, Ñ, Ç`).
* **Calibrated Battery Meter:** Real percentage readout (`0% - 100%`) calibrated for Li-Ion cells (`3.1V` to `4.0V`) on analog pin **`A1`**.
* **New Button Shortcuts:**
  * **`MODE` (short press):** Toggles hardware **Mute** (`MT`).
  * **`MODE` (long press > 0.6s):** Cycles modulation between **AM / LSB / USB**.
  * **`BW` (in FM mode):** Toggles **Stereo (`ST`)** / **Mono (`MN`)**.
  * **`VOL+` + `VOL-` together:** Manually saves current settings to EEPROM (`SAV` indicator).
* **SSB & Shortwave Enhancements:**
  * Automatic `+5` volume boost in SSB modes.
  * Seamless continuous BFO tuning every 16 kHz.
  * Full Ham bands (160m to 10m), **CB (27 MHz)**, and continuous SW coverage.
  * Calibrated 8-bar S-Meter.

### 🚀 Quick Flash (Pre-compiled `.hex`)
1. Download the **`.ino.hex`** file from the **Releases** tab.
2. Connect your ATS-20+ via the Mini-USB data port.
3. Use **AVRDUDESS** (`ATmega328P`, `Arduino` programmer, your `COM` port) to flash the `.ino.hex` file.

### 🛠️ Building from Source
1. Keep all 5 source files inside a folder matching the `.ino` filename.
2. Install the following libraries via Arduino IDE Library Manager:
   * **`PU2CLR SI4735`** by Ricardo Lima Caratti
   * **`SSD1306Ascii`** by Bill Greiman

---

## Credits
Based on the original **SI4735** library by **PU2CLR (Ricardo Lima Caratti)** and the **ATS-20** firmware by **RA4NAL / Brown Bear**.
