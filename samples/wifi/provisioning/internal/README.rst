.. _wifi_provisioning_internal_sample:

WiFi Provisioning Internal Sample
================================

This sample demonstrates the internal WiFi provisioning functionality using the
decoupled WiFi provisioning library. It provides shell commands to test all
supported WiFi provisioning operations.

Overview
--------

The sample uses the core WiFi provisioning library (`wifi_prov_core`) with a
transport stub implementation that decodes and logs protobuf messages. This
allows testing of the provisioning protocol without requiring a Bluetooth
transport layer.

Features
--------

- **Transport Decoupling**: Uses the core WiFi provisioning library without
  Bluetooth dependencies
- **Protobuf Decoding**: Automatically decodes and logs all requests and responses
- **Comprehensive Testing**: Shell commands for all supported operations
- **Raw Data Support**: Ability to send custom binary data for testing
- **Configurable Generation**: WiFi configuration generated from Kconfig options

Architecture
-----------

The sample demonstrates the decoupled architecture:

- **Core Library**: `wifi_prov_core` handles the protobuf protocol and business logic
- **Transport Stub**: `wifi_prov_transport_stub.c` provides mock transport functions
- **Shell Interface**: `prov.c` implements shell commands for testing
- **Configuration**: Generated from Kconfig options at build time

Kconfig Options
==============

The following Kconfig options are available for configuring the WiFi provisioning:

.. code-block:: text

    # WiFi Provisioning Configuration
    CONFIG_WIFI_PROV_CONFIG=y                    # Enable WiFi provisioning
    CONFIG_WIFI_PROV_SSID="SampleWiFi"          # WiFi SSID
    CONFIG_WIFI_PROV_BSSID="00:11:22:33:44:55" # WiFi BSSID
    CONFIG_WIFI_PROV_PASSPHRASE="samplepassword" # WiFi passphrase
    CONFIG_WIFI_PROV_AUTH_MODE=4                # Authentication mode (0-23)
    CONFIG_WIFI_PROV_CHANNEL=0                  # WiFi channel (0 for auto)
    CONFIG_WIFI_PROV_BAND=0                     # WiFi band (0=Any, 1=2.4GHz, 2=5GHz)
    CONFIG_WIFI_PROV_CERT_DIR=""                # Certificate directory (optional)
    CONFIG_WIFI_PROV_PRIVATE_KEY_PASSWD=""      # Primary private key password
    CONFIG_WIFI_PROV_PRIVATE_KEY_PASSWD2=""     # Secondary private key password
    CONFIG_WIFI_PROV_IDENTITY="user@example.com" # EAP identity
    CONFIG_WIFI_PROV_PASSWORD="user_password"   # EAP password
    CONFIG_WIFI_PROV_VOLATILE_MEMORY=n          # Use volatile memory (y/n)
    CONFIG_WIFI_PROV_SCAN_BAND=0                # Scan band (0=Any, 1=2.4GHz, 2=5GHz)
    CONFIG_WIFI_PROV_SCAN_PASSIVE=n             # Passive scan mode (y/n)
    CONFIG_WIFI_PROV_SCAN_PERIOD_MS=0           # Scan period in milliseconds
    CONFIG_WIFI_PROV_SCAN_GROUP_CHANNELS=0      # Scan group channels
    CONFIG_WIFI_PROV_MAX_DATA_SIZE=4096         # Max protobuf data size (bytes)
    CONFIG_WIFI_PROV_MAX_BASE64_SIZE=8192       # Max base64 input size (bytes)
    CONFIG_BT_WIFI_PROV_LOG_LEVEL=3             # Log level (0-4)

Authentication Modes
~~~~~~~~~~~~~~~~~~~

- 0: Open
- 1: WEP
- 2: WPA-PSK
- 3: WPA2-PSK
- 4: WPA3-SAE
- 5: WPA3-OWE
- 6: WPA2-Enterprise
- 7: WPA3-Enterprise
- 8: EAP-TLS
- 9: EAP-TTLS
- 10: EAP-PEAP
- 11: EAP-PWD
- 12: EAP-SIM
- 13: EAP-AKA
- 14: EAP-AKA'
- 15: EAP-FAST
- 16: EAP-TEAP
- 17: EAP-PAX
- 18: EAP-PSK
- 19: EAP-SAKE
- 20: EAP-IKEv2
- 21: EAP-GPSK
- 22: EAP-POTP
- 23: EAP-VENDOR

WiFi Bands
~~~~~~~~~~

- 1: 2.4 GHz
- 2: 5 GHz
- 3: 6 GHz

Shell Commands
=============

The sample provides several shell commands for testing WiFi provisioning functionality:

**wifi_prov**
    Send pre-generated WiFi configuration data to the provisioning service.

**wifi_prov raw <base64_data>**
    Send raw protobuf-encoded data (Base64 format) to the provisioning service.
    Example: ``wifi_prov raw CARaLgoaCgpTYW1wbGVXaUZpEgYAESIzRFUYAiAAKAMSDnNhbXBsZXBhc3N3b3JkIAA=``

**wifi_prov dump_scan <base64_data>**
    Decode and display scan results in human-readable format from Base64 encoded protobuf data.
    Example: ``wifi_prov dump_scan <base64_encoded_scan_results>``

**wifi_prov get_status**
    Send a GET_STATUS request to the provisioning service.

**wifi_prov start_scan**
    Send a START_SCAN request to the provisioning service.

**wifi_prov stop_scan**
    Send a STOP_SCAN request to the provisioning service.

**wifi_prov set_config**
    Send a SET_CONFIG request with WiFi configuration from Kconfig options.

**wifi_prov forget_config**
    Send a FORGET_CONFIG request to the provisioning service.

**wifi_prov info**
    Display information about the pre-generated WiFi configuration data.

Usage Examples
-------------

**Basic Testing**
~~~~~~~~~~~~~~~~

1. Build and flash the sample:
   .. code-block:: bash

       west build -b nrf7002dk/nrf5340/cpuapp samples/wifi/provisioning/internal
       west flash

2. Connect to the device console and test basic commands:
   .. code-block:: text

       uart:~$ wifi_prov info
       WiFi Configuration Information:
         Size: 156 bytes
         Data: 0x20000000
         First 16 bytes:
           0x08 0x04 0x12 0x0a 0x4d 0x79 0x57 0x69
           0x46 0x69 0x4e 0x65 0x74 0x77 0x6f 0x72

       uart:~$ wifi_prov get_status
       Getting WiFi provisioning status...
       === WiFi Provisioning Request ===
       Type: GET_STATUS
       ===============================
       WiFi status request sent successfully

**Advanced Testing**
~~~~~~~~~~~~~~~~~~~

Test custom protobuf messages:

.. code-block:: text

    uart:~$ wifi_prov raw 0801
    Sending raw binary data to provisioning service...
    Binary data size: 2 bytes
    === WiFi Provisioning Request ===
    Type: GET_STATUS
    ===============================
    Raw data sent successfully

**Protocol Testing**
~~~~~~~~~~~~~~~~~~

The sample automatically decodes and logs all protobuf messages:

.. code-block:: text

    uart:~$ wifi_prov start_scan
    Starting WiFi scan...
    === WiFi Provisioning Request ===
    Type: START_SCAN
    ===============================
    WiFi scan started successfully

    # Response will be logged automatically:
    === WiFi Provisioning Response ===
    Scan Result:
      Status: 0
      Networks found: 3
      Network 1:
        SSID: MyWiFiNetwork
        BSSID: 11:22:33:44:55:66
        RSSI: -45
        Channel: 6
        Auth mode: 3

Build and Run
------------

1. **Configure the sample** (optional):
   .. code-block:: bash

       # Edit prj.conf or use west build with -D options
       west build -b nrf7002dk/nrf5340/cpuapp samples/wifi/provisioning/internal \
           -- -DCONFIG_WIFI_PROV_SSID="MyNetwork" \
               -DCONFIG_WIFI_PROV_PASSPHRASE="mypassword"

2. **Build the sample**:
   .. code-block:: bash

       west build -b nrf7002dk/nrf5340/cpuapp samples/wifi/provisioning/internal

3. **Flash the device**:
   .. code-block:: bash

       west flash

4. **Connect to console and test**:
   .. code-block:: bash

       # Connect to device console
       screen /dev/ttyACM0 115200

       # Test commands
       uart:~$ wifi_prov info
       uart:~$ wifi_prov get_status
       uart:~$ wifi_prov raw 0801

Dependencies
-----------

- **Zephyr RTOS**: Core RTOS functionality
- **WiFi Provisioning Core Library**: Protocol handling and business logic
- **Protobuf/nanopb**: Message serialization
- **Shell Subsystem**: Command-line interface
- **Logging**: Debug output and message decoding

Directory Structure
------------------

.. code-block:: text

    samples/wifi/provisioning/internal/
    ├── CMakeLists.txt              # Build configuration
    ├── Kconfig                     # Kconfig options
    ├── prj.conf                    # Project configuration
    ├── README.rst                  # This documentation
    └── src/
        ├── main.c                  # Application entry point
        ├── prov.c                  # Shell command implementations
        └── wifi_prov_transport_stub.c  # Transport stub with decoding

Generated Files
--------------

During build, the following files are generated:

- **wifi_config.h**: Header with WiFi configuration data declarations
- **wifi_config.c**: C file with WiFi configuration data definitions
- **Protobuf Python files**: Generated from .proto files for configuration generation

The sample demonstrates proper integration of the WiFi provisioning library
with comprehensive testing capabilities and human-readable protocol decoding.
