# Lithium Server - Astronomical Equipment Control API

## Overview

This is the server component of the Lithium astronomical equipment control system, providing a comprehensive REST API and WebSocket interface for controlling observatory equipment.

## Architecture

The server follows a standard web server architecture with the following components:

### Core Components

1. **Main Server** (`main_server.hpp`, `main.cpp`)
   - Entry point and application lifecycle management
   - Integrates all controllers and middleware
   - Manages WebSocket and HTTP services

2. **Event Loop** (`eventloop.hpp`, `eventloop.cpp`)
   - High-performance asynchronous task scheduler
   - Priority-based task queue
   - Platform-specific I/O multiplexing (epoll on Linux, select on Windows)

3. **WebSocket Server** (`websocket.hpp`, `websocket.cpp`)
   - Real-time bidirectional communication
   - Event broadcasting and subscription management
   - Command handling and authentication

### Middleware

Located in `middleware/`:

- **Authentication** (`auth.hpp`)
  - API Key validation (X-API-Key header)
  - CORS support
  - Request logging

### Utilities

Located in `utils/`:

- **Response Builder** (`response.hpp`)
  - Standardized JSON response formatting
  - Error response generation
  - HTTP status code management

### Controllers

Located in `controller/`:

All controllers implement the base `Controller` interface and register their routes with the Crow application.

#### Device Controllers

- **CameraController** (`camera.hpp`)
  - List cameras, get status, connect/disconnect
  - Configure settings (cooling, gain, offset, binning, ROI)
  - Start/abort exposures
  - Get capabilities, gains, offsets
  - Warm-up sequence

- **MountController** (`mount.hpp`)
  - List mounts, get status, connect/disconnect
  - Slew to coordinates
  - Set tracking, guide rates, tracking rate
  - Park/home/unpark
  - Pulse guide, sync position
  - Meridian flip operations

- **FocuserController** (`focuser.hpp`)
  - List focusers, get status, connect/disconnect
  - Move to position (absolute/relative)
  - Temperature compensation
  - Autofocus routines
  - Halt movement

- **FilterWheelController** (`filterwheel.hpp`)
  - List filter wheels, get status, connect/disconnect
  - Set filter position by index or name
  - Configure filter names and focus offsets
  - Calibration routines

- **DomeController** (`dome.hpp`)
  - List domes, get status, connect/disconnect
  - Control shutter (open/close)
  - Slave to telescope

#### System Controllers

- **SystemController** (`system.hpp`)
  - System status and resource monitoring
  - Configuration management
  - Process management
  - Service restart
  - System shutdown/restart
  - Logging access
  - Device discovery
  - Diagnostics and health checks
  - Error reports
  - Backup/restore operations

- **FilesystemController** (`filesystem.hpp`)
  - List directory contents
  - Read/write files
  - Delete, move, copy operations
  - Create directories
  - File search

- **SkyController** (`sky.hpp`)
  - Resolve celestial object names
  - Search sky objects (basic and advanced)
  - Plate solving

## API Structure

### Base Path

All REST API endpoints use the base path: `/api/v1`

### Authentication

All endpoints require authentication using the `X-API-Key` header:

```http
X-API-Key: your-api-key-here
```

### Standard Response Format

#### Success Response

```json
{
  "status": "success",
  "data": { ... }
}
```

#### Error Response

```json
{
  "status": "error",
  "error": {
    "code": "error_code",
    "message": "Human-readable error message",
    "details": { ... }
  }
}
```

### HTTP Status Codes

- `200 OK`: Success
- `202 Accepted`: Async operation initiated
- `400 Bad Request`: Invalid request
- `401 Unauthorized`: Missing/invalid API key
- `403 Forbidden`: Operation not allowed
- `404 Not Found`: Resource not found
- `409 Conflict`: Resource conflict (e.g., device busy)
- `422 Unprocessable Entity`: Semantic errors
- `429 Too Many Requests`: Rate limit exceeded
- `500 Internal Server Error`: Server error
- `503 Service Unavailable`: Device not connected

## WebSocket Protocol

### Connection

Connect to: `ws://server:port/api/v1/ws?apiKey=your-api-key`

### Message Format

#### Client to Server (Commands)

```json
{
  "type": "command",
  "command": "command_name",
  "requestId": "unique_request_id",
  "params": { ... }
}
```

#### Server to Client (Events/Responses)

```json
{
  "type": "event_type",
  "timestamp": "2023-11-20T12:00:00Z",
  "data": { ... },
  "correlationId": "request_id"
}
```

### Event Types

- Connection events: `connection.established`
- Device events: `device.connected`, `device.disconnected`, `device.status_update`
- Exposure events: `exposure.started`, `exposure.progress`, `exposure.finished`, `exposure.aborted`
- Mount events: `mount.slew_started`, `mount.slew_finished`, `mount.meridian_flip_required`
- Focuser events: `focuser.move_finished`, `focuser.autofocus_progress`, `focuser.autofocus_finished`
- Guider events: `guider.status_update`, `guider.started`, `guider.stopped`, `guider.dither_finished`
- Sequence events: `sequence.started`, `sequence.progress`, `sequence.finished`, `sequence.aborted`
- System events: `notification`, `server.shutdown`, `system.status_update`

## Building and Running

### Prerequisites

- C++20 compatible compiler
- CMake 3.15+
- Crow web framework
- nlohmann/json
- spdlog/loguru
- Boost (optional, for ASIO)

### Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Run

```bash
./lithium-server --port 8080 --threads 4
```

### Command Line Options

- `--port <number>`: Server port (default: 8080)
- `--threads <number>`: Worker threads (default: 4)
- `--help, -h`: Show help message

## Configuration

API keys can be configured in the main application or loaded from a secure configuration file. The default key is:

```
default-api-key-please-change-in-production
```

**⚠️ IMPORTANT: Change this key in production deployments!**

## API Documentation

Full API specification is available in:
- `docs/astronomy_api_spec.md` - Complete REST API and WebSocket specification
- OpenAPI/AsyncAPI specs in `docs/` directory

## Security Considerations

1. **API Keys**: Store API keys securely and never commit them to version control
2. **SSL/TLS**: Enable SSL in production for encrypted communication
3. **CORS**: Configure CORS policies appropriately for your deployment
4. **Rate Limiting**: Implement rate limiting to prevent abuse
5. **Input Validation**: All user input is validated before processing

## Development

### Adding a New Controller

1. Create a new header file in `controller/` directory
2. Inherit from the `Controller` base class
3. Implement `registerRoutes()` method
4. Register controller in `main_server.hpp`

Example:

```cpp
class MyController : public Controller {
public:
    void registerRoutes(crow::SimpleApp& app) override {
        CROW_ROUTE(app, "/api/v1/my-endpoint")
            .methods("GET"_method)
            ([this](const crow::request& req) {
                return this->handleRequest(req);
            });
    }

private:
    crow::response handleRequest(const crow::request& req) {
        using namespace utils;
        nlohmann::json data = { {"message", "Hello"} };
        return ResponseBuilder::success(data);
    }
};
```

### Error Handling

Use `ResponseBuilder` utility class for consistent error responses:

```cpp
// Device not found
return ResponseBuilder::deviceNotFound(deviceId, "camera");

// Missing field
return ResponseBuilder::missingField("duration");

// Invalid field value
return ResponseBuilder::invalidFieldValue("temperature", "Must be <= 15.0");

// Device busy
return ResponseBuilder::deviceBusy(deviceId, "exposing", {{"remainingTime", 120}});
```

## Testing

Test the API using curl:

```bash
# Get system status
curl -H "X-API-Key: default-api-key-please-change-in-production" \
  http://localhost:8080/api/v1/system/status

# List cameras
curl -H "X-API-Key: default-api-key-please-change-in-production" \
  http://localhost:8080/api/v1/cameras

# Start exposure
curl -X POST \
  -H "X-API-Key: default-api-key-please-change-in-production" \
  -H "Content-Type: application/json" \
  -d '{"duration": 10, "frameType": "Light"}' \
  http://localhost:8080/api/v1/cameras/cam-001/exposure
```

## License

See LICENSE file in project root.

## Authors

Lithium Development Team

## Support

For issues and questions, please refer to the project repository.
