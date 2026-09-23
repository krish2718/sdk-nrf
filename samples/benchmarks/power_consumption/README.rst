.. _power_consumption_sample:

Power consumption (System ON idle)
###################################

.. contents::
   :local:
   :depth: 2

This sample demonstrates System ON idle power consumption on the nRF7120
DK. It repeatedly sleeps for a configurable duration and wakes up, printing
how many times it has woken up so far. It also demonstrates the different
RAM retention levels available through the :ref:`lib_ram_pwrdn` library.

Requirements
************

* An nRF7120 DK (``nrf7120dk/nrf7120/cpuapp``).
* A `Power Profiler Kit II (PPK2)`_, for measuring current.

Overview
********

At boot, the sample prints a reminder to switch the board's power off and
back on before taking a power measurement (see `Measuring power
consumption`_ below for why), then applies the configured RAM retention
level, then loops forever: sleep for
:kconfig:option:`CONFIG_POWER_CONSUMPTION_IDLE_SECONDS` (10 s by default),
wake up, print a wake counter, and sleep again.

RAM retention levels
=====================

By default, all RAM stays retained during System ON idle. The sample's own
``Kconfig`` exposes a choice of retention levels, each backed by the
``ram_pwrdn`` library:

.. list-table::
   :header-rows: 1

   * - Kconfig option
     - Behavior
   * - ``CONFIG_POWER_CONSUMPTION_RAM_RETAIN_FULL`` (default)
     - All RAM stays retained. No RAM is powered down.
   * - ``CONFIG_POWER_CONSUMPTION_RAM_RETAIN_64K``
     - Only the first 64 KiB of RAM stays retained; the rest is powered down.
   * - ``CONFIG_POWER_CONSUMPTION_RAM_RETAIN_128K``
     - Only the first 128 KiB of RAM stays retained; the rest is powered down.
   * - ``CONFIG_POWER_CONSUMPTION_RAM_RETAIN_256K``
     - Only the first 256 KiB of RAM stays retained; the rest is powered down.
   * - ``CONFIG_POWER_CONSUMPTION_RAM_RETAIN_512K``
     - Only the first 512 KiB of RAM stays retained; the rest is powered down.
   * - ``CONFIG_POWER_CONSUMPTION_RAM_RETAIN_UNUSED_ONLY``
     - Powers down whatever this image itself does not use, using the
       library's own automatic detection (``power_down_unused_ram()``),
       instead of a fixed KiB boundary.

Building and running
*********************

.. |sample path| replace:: :file:`nrf/samples/benchmarks/power_consumption`

.. include:: /includes/build_and_run.txt

The default configuration (all RAM retained) is built with:

.. code-block:: console

   west build -p always -b nrf7120dk/nrf7120/cpuapp nrf/samples/benchmarks/power_consumption

To build one of the other RAM retention levels, pass the matching Kconfig
option on the command line, for example:

.. code-block:: console

   west build -p always -b nrf7120dk/nrf7120/cpuapp nrf/samples/benchmarks/power_consumption -- -DCONFIG_POWER_CONSUMPTION_RAM_RETAIN_64K=y

The same six configurations are also named in :file:`sample.yaml` (for
example ``sample.benchmarks.power_consumption.ram_retain_64k``), for use
with ``west build -T <name>`` or Twister.

Then flash normally:

.. code-block:: console

   west flash

Measuring power consumption
****************************

.. important::
   Flashing itself (the debugger/SWD connection) draws extra current and
   can leave the target in a state that does not reflect real standalone
   power consumption. Always power-cycle the target after flashing,
   before taking a reading -- step 8 below.

**P601** is a 1x3 header (pin 1 ``P5V0``, pin 2 ``VBAT_5V0``, pin 3
``GND``), normally bridged by the **JP601** shunt jumper so ``P5V0``
feeds straight through to ``VBAT_5V0``. To measure current with a PPK2
in source meter mode:

#. Remove the **JP601** shunt jumper from **P601**.
#. Connect the PPK2's **Vout** to **P601** pin 2 (``VBAT_5V0``).
#. Connect the PPK2's **GND** to **P601** pin 3 (``GND``).
#. Connect the USB cable to the DK to flash the image (the DK's
   interface MCU/debugger runs on its own USB-derived supply; with the
   **JP601** shunt removed, the nRF7120 itself is powered only from
   **P601** pin 2, i.e. the PPK2, throughout).
#. In the PPK2 app, select **Source Meter** mode and enable power output
   with **3.6 V** as the supply voltage.
#. Run ``west flash`` to flash the image.
#. Remove the USB cable.
#. Toggle the PPK2's **Enable power output** control off and back on --
   this power-cycles the target now that the debugger is disconnected,
   giving a clean measurement.

Sample output
**************

.. code-block:: console

   Power consumption demo ready.
   Switch the board's power off and back on now to get correct power consumption readings.

   Woken up 1 time(s)
   Woken up 2 time(s)
   Woken up 3 time(s)
