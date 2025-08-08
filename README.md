# clink-ko

`clink-ko` is a Linux kernel module designed for embedded systems running OpenWrt. It facilitates communication between the kernel space and userspace applications using Netlink sockets. The primary purpose of this module is to capture specific kernel events (like Wi-Fi status changes or WAN link state) and relay them to a userspace daemon for handling.

## Features

- **Netlink Communication**: Provides a robust communication channel between the kernel and userspace via `NETLINK_USERSOCK`.
- **Event-Driven Hooks**: Hooks into various kernel subsystems to monitor hardware and network status.
- **Wi-Fi Status Monitoring**:
    - Reports when the first client connects or the last client disconnects from the AP.
    - Monitors the RSSI (Received Signal Strength Indication) of an AP-Client connection and reports significant changes.
    - Can be used to control Wi-Fi status LEDs.
- **Ethernet WAN Status**: Monitors the link status (up/down) of the physical WAN port.
- **Power Saving**: Provides a hook to wake up a device when a client connects.
- **Highly Configurable**: Features can be enabled or disabled at compile-time through the OpenWrt `menuconfig` interface.
- **Procfs Interface**: Uses a simple `/proc/clink` interface for userspace applications to register their Process ID (PID).

## How it Works

1.  **Initialization**: The module initializes a Netlink socket and registers function pointers (hooks) with other kernel drivers (e.g., the Wi-Fi driver).
2.  **PID Registration**: A userspace application writes its own PID to `/proc/clink`. The kernel module reads this PID to know where to send the Netlink messages.
3.  **Event Capturing**: When a hooked event occurs (e.g., a Wi-Fi client connects), the corresponding driver calls the function provided by `clink-ko`.
4.  **Event Forwarding**: The `clink-ko` module packages the event information into an `Event_t` struct and sends it as a Netlink message to the registered userspace PID.
5.  **Userspace Handling**: The userspace application listens on its Netlink socket, receives the event, and performs actions based on the event ID and payload (e.g., toggling a GPIO for an LED).

## Dependencies

-   `kmod-mt7628-ko`: This module is designed to work with the MediaTek MT7628 Wi-Fi driver, although it can be adapted for others.

## Building the Module

This package is intended to be built as part of a full OpenWrt firmware image.

1.  **Copy Package**: Place the `clink-ko` directory into the `<openwrt-root>/package/` directory of your OpenWrt build environment.
2.  **Run Menuconfig**: Launch the OpenWrt configuration menu:
    ```bash
    make menuconfig
    ```
3.  **Select Package**: Navigate to `CAMEO Proprietary Software --->` and select `kmod-clink-ko` to be built as a module (`M`).
4.  **Configure**: Enter the `Configuration --->` submenu for the package to enable or disable features.
5.  **Build Firmware**: Exit `menuconfig`, save the configuration, and run the build:
    ```bash
    make
    ```
The compiled `clink.ko` file will be included in the final firmware image. The module is set to autoload on boot.

## Configuration

The following options are available in `make menuconfig` under the package's configuration menu:

-   `UPDATE_WIFI_LED`: Enable hooks for Wi-Fi events to control LEDs.
-   `UPDATE_RSSI_LED`: (Sub-option of `UPDATE_WIFI_LED`) Enable RSSI-based LED control for AP-Client mode.
-   `HAVE_PHY_ETHERNET`: Enable if the target device has a physical Ethernet WAN port.
-   `POWER_SAVING`: Enable the `rx_wake_up` event hook.
-   `UPDATE_WAN_STATUS`: Enable WAN status updates (specifically for "WDC30" devices).

## Userspace Integration

A custom userspace application is required to receive and handle the events from this module.

1.  **Open a Netlink Socket**: The application must create a socket of type `NETLINK_USERSOCK`.
2.  **Register PID**: The application must write its process ID (PID) to `/proc/clink`.
    ```c
    // Example:
    FILE *fp = fopen("/proc/clink", "w");
    if (fp) {
        fprintf(fp, "%d", getpid());
        fclose(fp);
    }
    ```
3.  **Listen for Events**: The application should continuously read from the Netlink socket. Messages will arrive as an `Event_t` struct, defined in `src/clink_ev.h`.

    ```c
    // from src/clink_ev.h
    struct Event {
        unsigned int id;
        unsigned int length;
        char payload[128];
    };
    ```
4.  **Parse Events**: The application should use a `switch` statement on the `event.id` to handle different types of events.

    -   `UPDATE_WIFI_LED`: Payload is `"<level>,<channel>,<size>"`.
    -   `LINK_WAN_ST`: Payload is `"up"` or `"down"`.
    -   `RX_WAKE_UP`: No payload.
    -   `UPDATE_WAN_STATUS`: Payload is `"up"` or `"down"`.

## License

This project is licensed under the **GPL**.
