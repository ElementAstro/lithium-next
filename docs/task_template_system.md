# Task Sequence Template System

## Overview

The enhanced task sequence system now supports serialization and deserialization from JSON files, with the added capability to create and use templates. This document explains how to use the template feature.

## Templates

Templates are reusable sequence definitions that can be customized with parameters when creating a new sequence. This allows users to define common sequence patterns once and reuse them with different settings.

### Template Format

A sequence template is a JSON file with the following structure:

```json
{
  "version": "1.0",
  "type": "template",
  "targets": [
    {
      "name": "${target_name|M42}",
      "tasks": [
        {
          "type": "exposure",
          "exposure_time": "${exposure_time|30}",
          "count": "${count|5}",
          "filter_wheel": "LRGB",
          "filter": "L"
        }
      ]
    }
  ]
}
```

The template format uses placeholders in the format `${parameter_name|default_value}` that can be replaced when creating a sequence from the template.

### Creating a Template

You can export an existing sequence as a template using the `exportAsTemplate` method:

```cpp
ExposureSequence sequence;
// Add targets and tasks to the sequence
sequence.exportAsTemplate("my_template.json");
```

### Using a Template

To create a new sequence from a template, use the `createFromTemplate` method with parameters:

```cpp
ExposureSequence sequence;
json params;
params["target_name"] = "M51";
params["exposure_time"] = 60.0;
params["count"] = 10;
sequence.createFromTemplate("my_template.json", params);
```

If a parameter is not provided, the default value from the template will be used.

## Serialization and Deserialization

The system supports serialization and deserialization of sequences to and from JSON files.

### Saving a Sequence

```cpp
sequence.saveSequence("my_sequence.json");
```

### Loading a Sequence

```cpp
sequence.loadSequence("my_sequence.json");
```

### Validation

Before loading a sequence, you can validate it against the schema:

```cpp
if (sequence.validateSequenceFile("my_sequence.json")) {
    sequence.loadSequence("my_sequence.json");
} else {
    std::cerr << "Invalid sequence file!" << std::endl;
}
```

## Integration with Task Generator

The sequence system integrates with the task generator to support macro processing. This allows for dynamic generation of tasks based on macros defined in the sequence.

### Example

```cpp
// Set up a task generator with macros
auto generator = std::make_shared<TaskGenerator>();
generator->addMacro("TARGET", "M42");
generator->addMacro("EXPOSURE", 30.0);

// Set the generator on the sequence
sequence.setTaskGenerator(generator);

// Process targets with macros
sequence.processAllTargetsWithMacros();
```

## Thread Safety

The sequence system is thread-safe, using shared mutexes for read operations and exclusive locks for write operations. This allows for concurrent reading of sequence data while ensuring exclusive access during modifications.

## Error Handling

The system includes comprehensive error handling with detailed error messages. Validation failures provide information about what went wrong, and serialization/deserialization operations throw exceptions with descriptive messages.

## Example Usage

See `example/sequence_template_example.cpp` for a complete example of using the template system.

```bash
# Build the example
cd build
make sequence_template_example

# Run the example
./example/sequence_template_example
```
