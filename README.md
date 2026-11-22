# Wifi-Deauther-with-esp8266
Here is the step-by-step guide to downloading the Arduino IDE and setting up the ESP8266 Deauther from scratch.

### ⚠️ Important Legal & Ethical Warning
**This tool is for educational purposes only.** You must only use it on networks and devices you own or have explicit permission to test. Using it on others' networks is illegal and unethical.

---

### 🛠️ Part 1: Install the Arduino IDE
1.  Go to the official Arduino website: [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software).
2.  Download the installer for your operating system (Windows, macOS, or Linux).
3.  Run the installer and follow the on-screen instructions to complete the installation.

---

### 📚 Part 2: Add ESP8266 Deauther Support
1.  Open the Arduino IDE.
2.  Go to **File > Preferences** (or **Arduino IDE > Preferences** on macOS).
3.  Find the field labeled **"Additional Boards Manager URLs"** and paste the following link into it:
    `https://raw.githubusercontent.com/SpacehuhnTech/arduino/main/package_spacehuhn_index.json`
4.  Click **OK** to save.
5.  Go to **Tools > Board > Boards Manager**.
6.  In the search bar, type **`deauther`**.
7.  Find **"Deauther ESP8266 Boards"** by Spacehuhn Technologies and click **Install**.

---

### 💻 Part 3: Flash the Firmware
1.  Connect your ESP8266 (NodeMCU) to your computer via a **USB data cable**.
2.  In the Arduino IDE, go to **Tools > Board** and select your specific board model (e.g., **NodeMCU 1.0**) under the "Deauther ESP8266 Boards" section.
3.  Go to **Tools > Port** and select the COM port your board is connected to.
4.  *(Optional but recommended)* Go to **Tools > Erase Flash** and select **"All Flash Contents"** to clear any old data.
5.  Open the Deauther source code by going to **File > Examples > Deauther** (or open the `.ino` file you downloaded).
6.  Click the **Upload** button (the right-pointing arrow icon).

---

### 🚀 Part 4: How to Use It
1.  After the upload is complete, **unplug and replug** your ESP8266.
2.  On your phone or laptop, search for a new Wi-Fi network named **`pwned`** and connect to it (the default password is usually `deauther`).
3.  Open a web browser and go to **`192.168.4.1`** to access the control panel.
4.  From there, you can scan for networks, select a target, and start the deauthentication attack.
