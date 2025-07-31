.. _wifi_prov_readme:

Wi-Fi Provisioning BLE Transport
################################

.. contents::
   :local:
   :depth: 2

This library implements the Bluetooth® GATT transport layer for the Wi-Fi® provisioning service.
It provides the BLE-specific implementation of the transport interface defined by the core Wi-Fi provisioning library.
The BLE transport layer is designed to work with the core Wi-Fi provisioning library located at `subsys/net/lib/wifi_prov/`.

Overview
********

The BLE transport layer is responsible for:

* GATT service interface: Defines the GATT service and serves as the transport layer for the provisioning protocol.
* BLE-specific message handling: Implements the transport functions required by the core library.
* Connection management: Manages BLE connections and handles characteristic notifications/indications.

The transport layer implements the transport interface defined by the core Wi-Fi provisioning library:

* `wifi_prov_send_rsp()`: Sends Response messages via BLE indications
* `wifi_prov_send_result()`: Sends Result messages via BLE notifications

Service declaration
*******************

The Wi-Fi Provisioning Service is instantiated as a primary service.
Set the service UUID value as defined in the following table.

========================== ========================================
Service name               UUID
Wi-Fi Provisioning Service ``14387800-130c-49e7-b877-2881c89cb258``
========================== ========================================

Service characteristics
=======================

The UUID value of characteristics are defined in the following table.

========================== ========================================
Characteristic name        UUID
Information                ``14387801-130c-49e7-b877-2881c89cb258``
Operation Control Point    ``14387802-130c-49e7-b877-2881c89cb258``
Data Out                   ``14387803-130c-49e7-b877-2881c89cb258``
========================== ========================================

The characteristic requirements of the Wi-Fi Provisioning Service are shown in the following table.

+-----------------+-------------+-------------+-------------+-------------+
| Characteristic  | Requirement | Mandatory   | Optional    | Security    |
| name            |             | properties  | properties  | permissions |
+=================+=============+=============+=============+=============+
| Information     | Mandatory   | Read        |             | No security |
|                 |             |             |             | required    |
+-----------------+-------------+-------------+-------------+-------------+
| Operation       | Mandatory   | Indicate,   |             | Encryption  |
| Control         |             | Write       |             | required    |
| Point           |             |             |             |             |
+-----------------+-------------+-------------+-------------+-------------+
| Operation       | Mandatory   | Read, Write |             | Encryption  |
| Control         |             |             |             | required    |
| Point           |             |             |             |             |
| - Client        |             |             |             |             |
| Characteristic  |             |             |             |             |
| Configuration   |             |             |             |             |
| descriptor      |             |             |             |             |
+-----------------+-------------+-------------+-------------+-------------+
| Data Out        | Mandatory   | Notify      |             | Encryption  |
|                 |             |             |             | required    |
+-----------------+-------------+-------------+-------------+-------------+
| Data Out        | Mandatory   | Read, Write |             | Encryption  |
| - Client        |             |             |             | required    |
| Characteristic  |             |             |             |             |
| Configuration   |             |             |             |             |
| descriptor      |             |             |             |             |
+-----------------+-------------+-------------+-------------+-------------+

The purpose of each characteristic is as follows:

* ``Information``: For client to get ``Info`` message from server.
* ``Operation Control Point``: For client to send ``Request`` message to server, and server to send ``Response`` message to client.
* ``Data Out``: For server to send ``Result`` message to the client.

Transport Interface Implementation
**********************************

The BLE transport layer implements the transport interface defined by the core Wi-Fi provisioning library:

* `wifi_prov_send_rsp()`: Sends Response messages via BLE indications on the Operation Control Point characteristic
* `wifi_prov_send_result()`: Sends Result messages via BLE notifications on the Data Out characteristic

The transport layer also handles:

* Receiving Request messages from the Operation Control Point characteristic
* Providing Info messages via the Information characteristic
* Managing BLE connection state and characteristic subscriptions

Dependencies
***********

The BLE transport layer depends on:

* Core Wi-Fi provisioning library (`subsys/net/lib/wifi_prov/`)
* Bluetooth stack (`CONFIG_BT`)
* nanopb for protobuf message handling

API documentation
*****************

| Header file: :file:`include/net/wifi_prov/wifi_prov.h`
| Source files: :file:`subsys/bluetooth/services/wifi_prov`

.. doxygengroup:: bt_wifi_prov
