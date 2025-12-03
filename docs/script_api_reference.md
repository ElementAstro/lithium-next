# Script Service API Reference

## Overview

The ScriptService provides a unified facade for all script-related operations in Lithium.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Transport Layer                         │
├──────────────────────┬──────────────────────────────────────┤
│   HTTP Controllers   │        WebSocket Commands            │
│   /api/python/*      │        script.*                      │
│   /isolated/*        │                                      │
│   /script/*          │                                      │
│   /tools/*           │                                      │
│   /venv/*            │                                      │
└──────────────────────┴──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                      ScriptService                           │
│                  (Unified Facade Layer)                      │
└──────────────────────────────────────────────────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        ▼                      ▼                      ▼
┌───────────────┐    ┌─────────────────┐    ┌───────────────┐
│ PythonWrapper │    │ InterpreterPool │    │ IsolatedRunner│
│ (In-Process)  │    │ (Concurrent)    │    │ (Sandboxed)   │
└───────────────┘    └─────────────────┘    └───────────────┘
        │                                         │
        └─────────────────────┬───────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│   VenvManager  │  ToolRegistry  │  ScriptAnalyzer  │  ...   │
└─────────────────────────────────────────────────────────────┘
```

## API Endpoints

### Python Execution (`/api/python/*`)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/python/execute` | POST | Execute Python code |
| `/api/python/executeFile` | POST | Execute Python file |
| `/api/python/executeFunction` | POST | Execute Python function |
| `/api/python/executeAsync` | POST | Execute Python code asynchronously |
| `/api/python/numpy` | POST | Execute NumPy operations |
| `/api/python/validate` | POST | Validate script security |
| `/api/python/analyze` | POST | Analyze script for issues |
| `/api/python/statistics` | GET | Get execution statistics |
| `/api/python/statistics/reset` | POST | Reset statistics |

### Isolated Runner Management (`/isolated/*`)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/isolated/cancel` | POST | Cancel running execution |
| `/isolated/kill` | POST | Kill subprocess |
| `/isolated/status` | GET | Get runner status |
| `/isolated/memoryUsage` | GET | Get memory usage |
| `/isolated/processId` | GET | Get process ID |
| `/isolated/validateConfig` | POST | Validate configuration |
| `/isolated/pythonVersion` | GET | Get Python version |
| `/isolated/setConfig` | POST | Set runner configuration |

### Shell Scripts (`/script/*`)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/script/register` | POST | Register shell script |
| `/script/delete` | POST | Delete shell script |
| `/script/update` | POST | Update shell script |
| `/script/run` | POST | Run shell script |
| `/script/runAsync` | POST | Run shell script async |
| `/script/list` | GET | List registered scripts |

### Tool Registry (`/tools/*`)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/tools/list` | GET | List registered tools |
| `/tools/info` | POST | Get tool information |
| `/tools/functions` | POST | Get tool functions |
| `/tools/invoke` | POST | Invoke tool function |
| `/tools/discover` | POST | Discover new tools |
| `/tools/reload` | POST | Reload tool |
| `/tools/unregister` | POST | Unregister tool |

### Virtual Environment (`/venv/*`)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/venv/create` | POST | Create virtual environment |
| `/venv/delete` | POST | Delete virtual environment |
| `/venv/list` | GET | List virtual environments |
| `/venv/activate` | POST | Activate virtual environment |
| `/venv/deactivate` | POST | Deactivate virtual environment |
| `/venv/install` | POST | Install package |
| `/venv/uninstall` | POST | Uninstall package |
| `/venv/packages` | POST | List installed packages |

## WebSocket Commands

All commands are prefixed with `script.`:

### Python Execution
- `script.execute` - Execute Python code
- `script.executeFile` - Execute Python file
- `script.executeFunction` - Execute Python function
- `script.cancel` - Cancel execution
- `script.status` - Get execution status

### Shell Scripts
- `script.shell.execute` - Execute shell script
- `script.shell.list` - List shell scripts

### Tool Registry
- `script.tool.list` - List tools
- `script.tool.info` - Get tool info
- `script.tool.invoke` - Invoke tool function
- `script.tool.discover` - Discover new tools

### Virtual Environment
- `script.venv.list` - List environments
- `script.venv.packages` - List packages
- `script.venv.install` - Install package
- `script.venv.uninstall` - Uninstall package
- `script.venv.create` - Create environment
- `script.venv.activate` - Activate environment
- `script.venv.deactivate` - Deactivate environment

## Execution Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `InProcess` | Direct execution via PythonWrapper | Fast, trusted scripts |
| `Pooled` | Concurrent execution via InterpreterPool | Medium complexity |
| `Isolated` | Sandboxed subprocess execution | Untrusted scripts |
| `Auto` | Automatic selection based on analysis | Default |

## Request Examples

### Execute Python Code
```json
POST /api/python/execute
{
  "code": "result = sum([1, 2, 3, 4, 5])",
  "args": {},
  "mode": "auto",
  "timeout": 30000,
  "validate": true
}
```

### Execute NumPy Operation
```json
POST /api/python/numpy
{
  "operation": "stack",
  "arrays": [[1, 2, 3], [4, 5, 6]],
  "params": {"axis": 0}
}
```

### Invoke Tool Function
```json
POST /tools/invoke
{
  "tool": "math_utils",
  "function": "calculate",
  "args": {"x": 10, "y": 20}
}
```
