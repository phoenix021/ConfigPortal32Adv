# ConfigPortal32Adv

### Credits

The **ConfigPortal32Adv** library is based on https://github.com/yhur/ConfigPortal32 by Yoonseok Hur.

The original library was incorporated into this project on August 15, 2025.

---

### Changes

**15.08.2025**

1. Added styles for input field labels and placeholders
2. Introduced InputField and InputGroup structs to divide parameters into WiFi and Other categories
3. Enabled pre-loading of existing settings into the configuration page
4. The sketch uses nearly 1 MB of flash; consider increasing the firmware partition size on the ESP32

---

### Description

Like the original, **ConfigPortal32Adv** is fully contained within a single header file: ConfigPortal32Adv.h.

It provides a captive portal for Wi-Fi provisioning and runtime configuration via a browser-based interface. Supported input types include:

* text
* checkbox
* password
* email
* number
* date
* time
* color

Configuration data is stored in a file within the **LittleFS** filesystem, located in the ESP32's flash memory. This configuration persists across device reboots.

A common use case is initial setup of the Wi-Fi SSID and password.

The portal displays the current configuration in a simple web form. Users can modify parameters and submit the form via the **Save** button. The library then writes the updated parameters to the config file in flash.

**Note**: The library does not validate parameter types, values, or constraints. Validation is left to the developer.

---

##### Example of usage

The accompanying .ino file demonstrates how to use the library. In its current form, the portal is always displayed — meaning the firmware never proceeds beyond the configuration stage. 



This behavior is intentional for demonstration purposes. In a real application, you should modify the logic to conditionally display the portal only when configuration is required.

---



### Further Work

Potential improvements include:

* Migrating to an async web server to free the main loop from frequent polling
* Adding support for parameter arrays
