
# WiFi Hotspot Manager

A comprehensive utility for managing WiFi hotspots on Linux systems using NetworkManager. This package provides both a command-line interface and a programmable API, allowing you to easily create, manage, and monitor WiFi hotspots.

## Features

- Create and manage WiFi hotspots with various authentication options
- Monitor connected clients in real-time
- Save and load hotspot configurations
- Command-line interface for easy usage
- Programmable API for integration into other Python applications
- C++ integration through pybind11
- Extensive logging using loguru
- Comprehensive error handling and user feedback

## Requirements

- Linux system with NetworkManager installed
- Python 3.10 or later
- NetworkManager's CLI tools (`nmcli`)
- Root privileges (for some operations)

## Installation

### From PyPI

```bash
pip install wifi-hotspot-manager
```

### From Source

```bash
git clone https://github.com/username/wifi-hotspot-manager.git
cd wifi-hotspot-manager
pip install .
```

### Development Installation

```bash
git clone https://github.com/username/wifi-hotspot-manager.git
cd wifi-hotspot-manager
pip install -e ".[dev]"
```

## Quick Start

### Command Line Usage

Start a WiFi hotspot with default settings:

```bash
wifi-hotspot start --name "MyHotspot" --password "securepassword"
```

Check the status of your hotspot:

```bash
wifi-hotspot status
```

View connected clients:

```bash
wifi-hotspot clients
```

Monitor clients in real-time:

```bash
wifi-hotspot clients --monitor --interval 2
```

Stop the hotspot:

```bash
wifi-hotspot stop
```

### Python API Usage

```python
from wifi_hotspot_manager import HotspotManager, AuthenticationType, BandType

# Create a hotspot manager
manager = HotspotManager()

# Start a hotspot with custom settings
manager.start(
    name="PythonHotspot",
    password="secretpassword",
    authentication=AuthenticationType.WPA2_PSK,
    channel=6,
    band=BandType.G_ONLY
)

# Check hotspot status
status = manager.get_status()
print(f"Hotspot running: {status['running']}")
print(f"IP Address: {status['ip_address']}")

# Get connected clients
clients = manager.get_connected_clients()
for client in clients:
    print(f"Client: {client['mac_address']} ({client['ip_address']})")

# Stop the hotspot
manager.stop()
```

## Command Line Reference

The `wifi-hotspot` command provides the following subcommands:

- `start`: Start a WiFi hotspot
  - `--name`: Hotspot name/SSID
  - `--password`: Hotspot password
  - `--authentication`: Authentication type (wpa-psk, wpa2-psk, wpa3-sae, none)
  - `--encryption`: Encryption type (aes, tkip, ccmp)
  - `--channel`: WiFi channel
  - `--interface`: Network interface to use
  - `--band`: WiFi band (bg = 2.4GHz, a = 5GHz, any = dual band)
  - `--hidden`: Make the hotspot hidden (not broadcast)
  - `--max-clients`: Maximum number of clients

- `stop`: Stop the currently running hotspot

- `status`: Show the current status of the hotspot

- `list`: List all active network connections

- `config`: Update the hotspot configuration
  - (Takes the same options as `start`)

- `restart`: Restart the hotspot with new settings
  - (Takes the same options as `start`)

- `interfaces`: List available network interfaces

- `clients`: List connected clients
  - `--monitor`: Continuously monitor clients
  - `--interval`: Monitoring interval in seconds

- `channels`: List available WiFi channels
  - `--interface`: Network interface to check

Global options:
- `--verbose`, `-v`: Enable verbose logging

## Configuration

The WiFi Hotspot Manager stores its configuration in `~/.config/hotspot-manager/config.json`. You can modify this file directly or use the `config` command to update settings.

Default configuration:
- Name: MyHotspot
- Authentication: WPA-PSK
- Encryption: AES
- Channel: 11
- Interface: wlan0
- Band: 2.4GHz
- Hidden: False
- Max Clients: 10

## Advanced Usage

### Monitoring Clients Programmatically

```python
import asyncio
from wifi_hotspot_manager import HotspotManager

async def client_callback(clients):
    print(f"Number of clients: {len(clients)}")
    for client in clients:
        print(f"Client: {client['mac_address']}")

async def main():
    manager = HotspotManager()
    await manager.monitor_clients(interval=3, callback=client_callback)

# Run the monitor for 30 seconds
async def run_with_timeout():
    try:
        task = asyncio.create_task(main())
        await asyncio.sleep(30)
        task.cancel()
        await task
    except asyncio.CancelledError:
        print("Monitoring stopped")

asyncio.run(run_with_timeout())
```

### C++ Integration via pybind11

To use this library with C++, you'll need to install the pybind11 dependencies:

```bash
pip install "wifi-hotspot-manager[pybind]"
```

Then in your C++ binding code:

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

PYBIND11_MODULE(my_cpp_module, m) {
    py::object wifi_hotspot = py::module::import("wifi_hotspot_manager");
    py::object create_module = wifi_hotspot.attr("create_pybind11_module")();

    py::object HotspotManager = create_module["HotspotManager"];
    py::object AuthenticationType = create_module["AuthenticationType"];

    // Expose to C++
    m.attr("HotspotManager") = HotspotManager;
    m.attr("AuthenticationType") = AuthenticationType;
    // ... other classes
}
```

## Troubleshooting

### Common Issues

1. "NetworkManager is not available on this system"
   - Make sure NetworkManager is installed and running:
     ```bash
     sudo apt install network-manager
     sudo systemctl status NetworkManager
     ```

2. "Command failed: nmcli dev wifi hotspot"
   - Ensure you have the right permissions:
     ```bash
     sudo wifi-hotspot start --name MyHotspot --password securepassword
     ```

3. "Failed to stop hotspot for restart"
   - The hotspot might not be running or another process is using it
   - Try manually stopping any existing hotspots:
     ```bash
     sudo nmcli connection down Hotspot
     ```

For more detailed debug information, use the `--verbose` flag:

```bash
wifi-hotspot status --verbose
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin feature-name`
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- NetworkManager team for providing the underlying functionality
- The loguru project for excellent logging capabilities
