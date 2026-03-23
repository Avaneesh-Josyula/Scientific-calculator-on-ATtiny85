## 🌟 Project Overview
Standard scientific calculators rely on 32-bit or 64-bit architectures with dedicated hardware floating-point units (FPUs). This project forces advanced scientific evaluation onto an 8-bit, 8-pin microcontroller. 
* **Custom Math Engine:** Bypasses the bloated C `<math.h>` library using a 1000-scale 32-bit fixed-point engine.
* **Order of Operations:** Implements a dynamic Shunting Yard string parsing algorithm.
* **Dual-Mode Matrix:** Reads 25 mechanical keys using only 3 GPIO pins via heavily multiplexed shift registers (74HC595 & 74HC165).

---

## 💻 Software Setup & Installation (PlatformIO)

To compile and upload this code via the terminal, you need **PlatformIO Core (CLI)** and the Micronucleus USB drivers.

### 1. Install Prerequisites
1. Download and install [Python 3](https://www.python.org/downloads/).
2. Install PlatformIO using pip:
   ```bash
   pip install -U platformio
   ```
3. **Install Digispark USB Drivers:**
   * **Windows:** Download the [Digistump Drivers](https://github.com/digistump/DigistumpArduino/releases/download/1.6.7/Digistump.Drivers.zip), extract, and run `Install Drivers.exe`.
   * **Linux:** Configure `udev` rules to grant USB access:
     ```bash
     curl -fsSL [https://raw.githubusercontent.com/digistump/DigistumpArduino/master/tools/49-micronucleus.rules](https://raw.githubusercontent.com/digistump/DigistumpArduino/master/tools/49-micronucleus.rules) | sudo tee /etc/udev/rules.d/49-micronucleus.rules
     sudo udevadm control --reload-rules
     ```

### 2. Project Initialization
Create a folder, navigate into it, and initialize a Digispark project:
```bash
mkdir ATtiny85_Calculator
cd ATtiny85_Calculator
pio project init --board digispark-tiny
```

### 3. Configure `platformio.ini`
Open `platformio.ini` and paste the following. *(Do not use the Arduino framework; we are compiling bare-metal AVR C).*
```ini
[env:digispark-tiny]
platform = atmelavr
board = digispark-tiny
build_flags = 
    -Os           ; Optimize heavily for size
    -std=gnu11    ; Standard C11
```
Place the `main.c` code file inside the `src/` directory.

---

## 🚀 Building and Uploading

### 1. Compile the Code
Verify compilation and memory footprint:
```bash
pio run
```
*Flash usage should be ~97%.*

### 2. Flash the Digispark (The "Plug-In Dance")
**Do NOT plug the Digispark into your computer yet.**

1. Run the upload command:
   ```bash
   pio run -t upload
   ```
2. The compiler will build the `.hex` file. Wait until the terminal prompts:
   > `Please plug in device (will time out in 60 seconds)...`
3. **NOW** plug the Digispark into the USB port.
4. The Micronucleus bootloader will catch the signal and flash the firmware.

---

## 🛠️ Troubleshooting

* **Upload Timeout / Device Not Found**
  * *Cause:* Driver missing or device plugged in too early.
  * *Fix:* Unplug the Digispark, run `pio run -t upload`, and only insert the USB when prompted.

