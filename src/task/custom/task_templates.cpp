/**
 * @file task_templates.cpp
 * @brief Task templates implementation
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "task_templates.hpp"
#include "atom/log/loguru.hpp"
#include <unordered_map>
#include <mutex>
#include <regex>

namespace lithium::sequencer::templates {

// Template registry
static std::unordered_map<std::string, json> registeredTemplates;
static std::mutex templateMutex;

// TemplateManager implementation

json TemplateManager::getImagingSequenceTemplate() {
    return json{
        {"templateName", "imaging_sequence"},
        {"description", "Standard imaging sequence with calibration frames"},
        {"parameters", {
            {"target_name", "{{target_name}}"},
            {"exposure_time", "{{exposure_time|default:120}}"},
            {"gain", "{{gain|default:100}}"},
            {"binning", "{{binning|default:1}}"},
            {"filter", "{{filter|default:Luminance}}"},
            {"frame_count", "{{frame_count|default:10}}"},
            {"include_darks", "{{include_darks|default:true}}"},
            {"include_flats", "{{include_flats|default:true}}"},
            {"include_bias", "{{include_bias|default:true}}"}
        }},
        {"sequence", {
            {"id", "imaging_{{target_name}}"},
            {"strategy", 1}, // Parallel
            {"maxConcurrency", 4},
            {"targets", json::array({
                {
                    {"name", "{{target_name}}"},
                    {"tasks", json::array({
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "camera"},
                                {"timeout", 5000}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "mount"},
                                {"timeout", 5000}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "plate_solve"},
                                {"exposure_time", 5.0},
                                {"gain", "{{gain}}"},
                                {"timeout", 60.0}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "auto_focus"},
                                {"filter", "{{filter}}"},
                                {"samples", 7},
                                {"step_size", 100}
                            }}
                        },
                        {
                            {"type", "config_task"},
                            {"parameters", {
                                {"action", "set_imaging_params"},
                                {"exposure_time", "{{exposure_time}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"},
                                {"filter", "{{filter}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_sequence"},
                                {"frame_type", "light"},
                                {"count", "{{frame_count}}"},
                                {"exposure_time", "{{exposure_time}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"},
                                {"filter", "{{filter}}"}
                            }}
                        }
                    })}
                },
                {
                    {"name", "calibration_darks"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_sequence"},
                                {"frame_type", "dark"},
                                {"count", 10},
                                {"exposure_time", "{{exposure_time}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"{{target_name}}"})}
                },
                {
                    {"name", "calibration_flats"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_sequence"},
                                {"frame_type", "flat"},
                                {"count", 10},
                                {"exposure_time", 5.0},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"},
                                {"filter", "{{filter}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"calibration_darks"})}
                },
                {
                    {"name", "calibration_bias"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_sequence"},
                                {"frame_type", "bias"},
                                {"count", 20},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"calibration_flats"})}
                }
            })}
        }}
    };
}

json TemplateManager::getCalibrationSequenceTemplate() {
    return json{
        {"templateName", "calibration_sequence"},
        {"description", "Comprehensive calibration frame sequence"},
        {"parameters", {
            {"gain", "{{gain|default:100}}"},
            {"binning", "{{binning|default:1}}"},
            {"filters", "{{filters|default:[\"Luminance\", \"Red\", \"Green\", \"Blue\"]}}"},
            {"dark_exposures", "{{dark_exposures|default:[60, 120, 300]}}"},
            {"flat_exposure", "{{flat_exposure|default:5.0}}"},
            {"dark_count", "{{dark_count|default:10}}"},
            {"flat_count", "{{flat_count|default:10}}"},
            {"bias_count", "{{bias_count|default:30}}"}
        }},
        {"sequence", {
            {"id", "calibration_master"},
            {"strategy", 0}, // Sequential for calibration
            {"targets", json::array({
                {
                    {"name", "bias_frames"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_sequence"},
                                {"frame_type", "bias"},
                                {"count", "{{bias_count}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"}
                            }}
                        }
                    })}
                },
                {
                    {"name", "dark_frames"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_darks_multi_exposure"},
                                {"exposures", "{{dark_exposures}}"},
                                {"count_per_exposure", "{{dark_count}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"bias_frames"})}
                },
                {
                    {"name", "flat_frames"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_flats_multi_filter"},
                                {"filters", "{{filters}}"},
                                {"exposure_time", "{{flat_exposure}}"},
                                {"count_per_filter", "{{flat_count}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"dark_frames"})}
                }
            })}
        }}
    };
}

json TemplateManager::getFocusSequenceTemplate() {
    return json{
        {"templateName", "focus_sequence"},
        {"description", "Automated focus sequence with multiple filters"},
        {"parameters", {
            {"filters", "{{filters|default:[\"Luminance\", \"Red\", \"Green\", \"Blue\"]}}"},
            {"samples", "{{samples|default:9}}"},
            {"step_size", "{{step_size|default:50}}"},
            {"exposure_time", "{{exposure_time|default:3.0}}"},
            {"gain", "{{gain|default:200}}"},
            {"tolerance", "{{tolerance|default:0.1}}"}
        }},
        {"sequence", {
            {"id", "auto_focus_multi"},
            {"strategy", 0}, // Sequential
            {"targets", json::array({
                {
                    {"name", "focus_luminance"},
                    {"tasks", json::array({
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "set_filter"},
                                {"filter", "Luminance"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "auto_focus"},
                                {"samples", "{{samples}}"},
                                {"step_size", "{{step_size}}"},
                                {"exposure_time", "{{exposure_time}}"},
                                {"gain", "{{gain}}"},
                                {"tolerance", "{{tolerance}}"}
                            }}
                        }
                    })}
                },
                {
                    {"name", "focus_color_filters"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "focus_multi_filter"},
                                {"filters", "{{filters}}"},
                                {"samples", 5},
                                {"step_size", 25},
                                {"exposure_time", "{{exposure_time}}"},
                                {"gain", "{{gain}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"focus_luminance"})}
                }
            })}
        }}
    };
}

json TemplateManager::getPlateSolvingTemplate() {
    return json{
        {"templateName", "plate_solving"},
        {"description", "Plate solving and sync sequence"},
        {"parameters", {
            {"exposure_time", "{{exposure_time|default:5.0}}"},
            {"gain", "{{gain|default:100}}"},
            {"timeout", "{{timeout|default:60.0}}"},
            {"precision", "{{precision|default:high}}"},
            {"sync_mount", "{{sync_mount|default:true}}"}
        }},
        {"sequence", {
            {"id", "plate_solve"},
            {"strategy", 0},
            {"targets", json::array({
                {
                    {"name", "solve_and_sync"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "capture_for_solve"},
                                {"exposure_time", "{{exposure_time}}"},
                                {"gain", "{{gain}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "plate_solve"},
                                {"precision", "{{precision}}"},
                                {"timeout", "{{timeout}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "sync_mount"},
                                {"enabled", "{{sync_mount}}"}
                            }}
                        }
                    })}
                }
            })}
        }}
    };
}

json TemplateManager::getDeviceSetupTemplate() {
    return json{
        {"templateName", "device_setup"},
        {"description", "Complete device initialization and setup"},
        {"parameters", {
            {"camera_name", "{{camera_name}}"},
            {"mount_name", "{{mount_name}}"},
            {"filterwheel_name", "{{filterwheel_name|default:\"\"}}"},
            {"focuser_name", "{{focuser_name|default:\"\"}}"},
            {"guider_name", "{{guider_name|default:\"\"}}"},
            {"timeout", "{{timeout|default:5000}}"}
        }},
        {"sequence", {
            {"id", "device_setup"},
            {"strategy", 1}, // Parallel where possible
            {"targets", json::array({
                {
                    {"name", "connect_devices"},
                    {"tasks", json::array({
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "camera"},
                                {"device_name", "{{camera_name}}"},
                                {"timeout", "{{timeout}}"}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "mount"},
                                {"device_name", "{{mount_name}}"},
                                {"timeout", "{{timeout}}"}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "filterwheel"},
                                {"device_name", "{{filterwheel_name}}"},
                                {"timeout", "{{timeout}}"},
                                {"optional", true}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "focuser"},
                                {"device_name", "{{focuser_name}}"},
                                {"timeout", "{{timeout}}"},
                                {"optional", true}
                            }}
                        }
                    })}
                },
                {
                    {"name", "initialize_devices"},
                    {"tasks", json::array({
                        {
                            {"type", "config_task"},
                            {"parameters", {
                                {"action", "load_device_profiles"},
                                {"camera_profile", "default"},
                                {"mount_profile", "default"}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "initialize"},
                                {"device_type", "all"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"connect_devices"})}
                }
            })}
        }}
    };
}

json TemplateManager::getSafetyCheckTemplate() {
    return json{
        {"templateName", "safety_check"},
        {"description", "Comprehensive safety and status check"},
        {"parameters", {
            {"check_weather", "{{check_weather|default:true}}"},
            {"check_power", "{{check_power|default:true}}"},
            {"check_disk_space", "{{check_disk_space|default:true}}"},
            {"min_disk_space", "{{min_disk_space|default:1024}}"},
            {"check_cooling", "{{check_cooling|default:true}}"},
            {"target_temp", "{{target_temp|default:-10}}"}
        }},
        {"sequence", {
            {"id", "safety_check"},
            {"strategy", 1}, // Parallel checks
            {"targets", json::array({
                {
                    {"name", "environmental_checks"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "weather_check"},
                                {"enabled", "{{check_weather}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "power_check"},
                                {"enabled", "{{check_power}}"}
                            }}
                        }
                    })}
                },
                {
                    {"name", "system_checks"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "disk_space_check"},
                                {"enabled", "{{check_disk_space}}"},
                                {"min_space_mb", "{{min_disk_space}}"}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "check_cooling"},
                                {"enabled", "{{check_cooling}}"},
                                {"target_temperature", "{{target_temp}}"}
                            }}
                        }
                    })}
                }
            })}
        }}
    };
}

json TemplateManager::getScriptExecutionTemplate() {
    return json{
        {"templateName", "script_execution"},
        {"description", "Custom script execution with error handling"},
        {"parameters", {
            {"script_path", "{{script_path}}"},
            {"script_type", "{{script_type|default:python}}"},
            {"arguments", "{{arguments|default:[]}}"},
            {"timeout", "{{timeout|default:300}}"},
            {"retry_count", "{{retry_count|default:3}}"},
            {"working_directory", "{{working_directory|default:\"\"}}"}
        }},
        {"sequence", {
            {"id", "script_execution"},
            {"strategy", 0}, // Sequential
            {"targets", json::array({
                {
                    {"name", "execute_script"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "{{script_type}}"},
                                {"script_path", "{{script_path}}"},
                                {"arguments", "{{arguments}}"},
                                {"timeout", "{{timeout}}"},
                                {"working_directory", "{{working_directory}}"},
                                {"retry_policy", {
                                    {"max_retries", "{{retry_count}}"},
                                    {"retry_delay", 5}
                                }}
                            }}
                        }
                    })}
                }
            })}
        }}
    };
}

json TemplateManager::getFilterChangeTemplate() {
    return json{
        {"templateName", "filter_change"},
        {"description", "Safe filter change with verification"},
        {"parameters", {
            {"target_filter", "{{target_filter}}"},
            {"verify_change", "{{verify_change|default:true}}"},
            {"settle_time", "{{settle_time|default:2}}"},
            {"max_attempts", "{{max_attempts|default:3}}"}
        }},
        {"sequence", {
            {"id", "filter_change"},
            {"strategy", 0},
            {"targets", json::array({
                {
                    {"name", "change_filter"},
                    {"tasks", json::array({
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "set_filter"},
                                {"filter", "{{target_filter}}"},
                                {"settle_time", "{{settle_time}}"},
                                {"max_attempts", "{{max_attempts}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "verify_filter"},
                                {"expected_filter", "{{target_filter}}"},
                                {"enabled", "{{verify_change}}"}
                            }}
                        }
                    })}
                }
            })}
        }}
    };
}

json TemplateManager::getGuidingSetupTemplate() {
    return json{
        {"templateName", "guiding_setup"},
        {"description", "Automated guiding setup and calibration"},
        {"parameters", {
            {"guide_exposure", "{{guide_exposure|default:2.0}}"},
            {"guide_gain", "{{guide_gain|default:150}}"},
            {"calibration_steps", "{{calibration_steps|default:12}}"},
            {"settle_timeout", "{{settle_timeout|default:30}}"},
            {"settle_tolerance", "{{settle_tolerance|default:1.5}}"}
        }},
        {"sequence", {
            {"id", "guiding_setup"},
            {"strategy", 0},
            {"targets", json::array({
                {
                    {"name", "setup_guiding"},
                    {"tasks", json::array({
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "connect"},
                                {"device_type", "guider"}
                            }}
                        },
                        {
                            {"type", "config_task"},
                            {"parameters", {
                                {"action", "set_guide_params"},
                                {"exposure_time", "{{guide_exposure}}"},
                                {"gain", "{{guide_gain}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "auto_select_guide_star"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "calibrate_guiding"},
                                {"steps", "{{calibration_steps}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "start_guiding"},
                                {"settle_timeout", "{{settle_timeout}}"},
                                {"settle_tolerance", "{{settle_tolerance}}"}
                            }}
                        }
                    })}
                }
            })}
        }}
    };
}

json TemplateManager::getCompleteObservationTemplate() {
    return json{
        {"templateName", "complete_observation"},
        {"description", "Full end-to-end observation sequence"},
        {"parameters", {
            {"target_name", "{{target_name}}"},
            {"ra", "{{ra}}"},
            {"dec", "{{dec}}"},
            {"exposure_time", "{{exposure_time|default:120}}"},
            {"frame_count", "{{frame_count|default:20}}"},
            {"filters", "{{filters|default:[\"Luminance\", \"Red\", \"Green\", \"Blue\"]}}"},
            {"gain", "{{gain|default:100}}"},
            {"binning", "{{binning|default:1}}"},
            {"enable_guiding", "{{enable_guiding|default:true}}"},
            {"enable_dithering", "{{enable_dithering|default:true}}"},
            {"dither_frequency", "{{dither_frequency|default:5}}"}
        }},
        {"sequence", {
            {"id", "complete_observation_{{target_name}}"},
            {"strategy", 2}, // Adaptive
            {"maxConcurrency", 2},
            {"targets", json::array({
                {
                    {"name", "setup_phase"},
                    {"tasks", json::array({
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "slew_to_target"},
                                {"ra", "{{ra}}"},
                                {"dec", "{{dec}}"},
                                {"target_name", "{{target_name}}"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "plate_solve_sync"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "auto_focus"},
                                {"filter", "Luminance"}
                            }}
                        }
                    })}
                },
                {
                    {"name", "guiding_setup"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "setup_guiding"},
                                {"enabled", "{{enable_guiding}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"setup_phase"})}
                },
                {
                    {"name", "imaging_session"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "imaging_sequence_multi_filter"},
                                {"filters", "{{filters}}"},
                                {"exposure_time", "{{exposure_time}}"},
                                {"frame_count", "{{frame_count}}"},
                                {"gain", "{{gain}}"},
                                {"binning", "{{binning}}"},
                                {"enable_dithering", "{{enable_dithering}}"},
                                {"dither_frequency", "{{dither_frequency}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"guiding_setup"})}
                },
                {
                    {"name", "cleanup_phase"},
                    {"tasks", json::array({
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "stop_guiding"},
                                {"enabled", "{{enable_guiding}}"}
                            }}
                        },
                        {
                            {"type", "device_task"},
                            {"parameters", {
                                {"action", "park_mount"}
                            }}
                        },
                        {
                            {"type", "script_task"},
                            {"parameters", {
                                {"script_type", "save_session_data"},
                                {"target_name", "{{target_name}}"}
                            }}
                        }
                    })},
                    {"dependencies", json::array({"imaging_session"})}
                }
            })}
        }}
    };
}

json TemplateManager::createTemplateFromSequence(const json& sequence, const std::string& templateName) {
    json templateDef;
    templateDef["templateName"] = templateName;
    templateDef["description"] = "Template created from existing sequence";
    templateDef["parameters"] = json::object();
    templateDef["sequence"] = sequence;
    
    // Extract parameterizable values
    std::vector<std::string> parameterizedFields = {
        "exposure_time", "gain", "binning", "filter", "count", 
        "target_name", "timeout", "device_name"
    };
    
    for (const auto& field : parameterizedFields) {
        templateDef["parameters"][field] = "{{" + field + "}}";
    }
    
    return templateDef;
}

json TemplateManager::applyTemplate(const json& templateDef, const json& parameters) {
    if (!templateDef.contains("sequence")) {
        throw std::invalid_argument("Invalid template: missing sequence definition");
    }
    
    json result = templateDef["sequence"];
    std::string resultStr = result.dump();
    
    // Simple template parameter substitution
    for (const auto& [key, value] : parameters.items()) {
        std::string placeholder = "{{" + key + "}}";
        std::string replacement = value.is_string() ? value.get<std::string>() : value.dump();
        
        size_t pos = 0;
        while ((pos = resultStr.find(placeholder, pos)) != std::string::npos) {
            resultStr.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
    }
    
    // Handle default values ({{key|default:value}})
    std::regex defaultPattern(R"(\{\{([^|]+)\|default:([^}]+)\}\})");
    std::smatch match;
    
    while (std::regex_search(resultStr, match, defaultPattern)) {
        std::string key = match[1].str();
        std::string defaultValue = match[2].str();
        
        if (!parameters.contains(key)) {
            resultStr.replace(match.position(), match.length(), defaultValue);
        }
    }
    
    try {
        return json::parse(resultStr);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to parse template result: {}", e.what());
        return templateDef["sequence"];
    }
}

bool TemplateManager::validateTemplate(const json& templateDef) {
    if (!templateDef.contains("templateName") || !templateDef["templateName"].is_string()) {
        return false;
    }
    
    if (!templateDef.contains("sequence") || !templateDef["sequence"].is_object()) {
        return false;
    }
    
    auto sequence = templateDef["sequence"];
    if (!sequence.contains("targets") || !sequence["targets"].is_array()) {
        return false;
    }
    
    // Validate each target
    for (const auto& target : sequence["targets"]) {
        if (!target.contains("name") || !target["name"].is_string()) {
            return false;
        }
        
        if (target.contains("tasks") && target["tasks"].is_array()) {
            for (const auto& task : target["tasks"]) {
                if (!task.contains("type") || !task["type"].is_string()) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

std::vector<std::string> TemplateManager::getAvailableTemplates() {
    std::lock_guard<std::mutex> lock(templateMutex);
    
    std::vector<std::string> templates = {
        "imaging_sequence",
        "calibration_sequence", 
        "focus_sequence",
        "plate_solving",
        "device_setup",
        "safety_check",
        "script_execution",
        "filter_change",
        "guiding_setup",
        "complete_observation"
    };
    
    // Add registered custom templates
    for (const auto& [name, _] : registeredTemplates) {
        templates.push_back(name);
    }
    
    return templates;
}

void TemplateManager::registerTemplate(const std::string& name, const json& templateDef) {
    if (!validateTemplate(templateDef)) {
        throw std::invalid_argument("Invalid template definition");
    }
    
    std::lock_guard<std::mutex> lock(templateMutex);
    registeredTemplates[name] = templateDef;
    
    LOG_F(INFO, "Registered custom template: {}", name);
}

void TemplateManager::unregisterTemplate(const std::string& name) {
    std::lock_guard<std::mutex> lock(templateMutex);
    
    auto it = registeredTemplates.find(name);
    if (it != registeredTemplates.end()) {
        registeredTemplates.erase(it);
        LOG_F(INFO, "Unregistered template: {}", name);
    }
}

// CommonTasks implementation

namespace CommonTasks {

json standardExposure(double exposureTime, int gain, int binning, const std::string& filter) {
    return json{
        {"exposure_time", exposureTime},
        {"gain", gain},
        {"binning", binning},
        {"filter", filter},
        {"frame_type", "light"}
    };
}

json darkFrame(double exposureTime, int gain, int binning, int count) {
    return json{
        {"exposure_time", exposureTime},
        {"gain", gain},
        {"binning", binning},
        {"frame_type", "dark"},
        {"count", count}
    };
}

json flatFrame(double exposureTime, int gain, int binning, const std::string& filter, int count) {
    return json{
        {"exposure_time", exposureTime},
        {"gain", gain},
        {"binning", binning},
        {"filter", filter},
        {"frame_type", "flat"},
        {"count", count}
    };
}

json biasFrame(int gain, int binning, int count) {
    return json{
        {"gain", gain},
        {"binning", binning},
        {"frame_type", "bias"},
        {"count", count}
    };
}

json autoFocus(const std::string& filter, int samples, double stepSize) {
    return json{
        {"filter", filter},
        {"samples", samples},
        {"step_size", stepSize},
        {"action", "auto_focus"}
    };
}

json plateSolve(double exposureTime, int gain, double timeout) {
    return json{
        {"exposure_time", exposureTime},
        {"gain", gain},
        {"timeout", timeout},
        {"action", "plate_solve"}
    };
}

json deviceConnect(const std::string& deviceName, const std::string& deviceType, int timeout) {
    return json{
        {"device_name", deviceName},
        {"device_type", deviceType},
        {"timeout", timeout},
        {"action", "connect"}
    };
}

json filterChange(const std::string& filterName, int settleTime) {
    return json{
        {"filter", filterName},
        {"settle_time", settleTime},
        {"action", "set_filter"}
    };
}

json startGuiding(double exposureTime, int gain, double tolerance) {
    return json{
        {"exposure_time", exposureTime},
        {"gain", gain},
        {"tolerance", tolerance},
        {"action", "start_guiding"}
    };
}

json safetyCheck(bool checkWeather, bool checkHorizon, bool checkSun) {
    return json{
        {"check_weather", checkWeather},
        {"check_horizon", checkHorizon},
        {"check_sun", checkSun},
        {"action", "safety_check"}
    };
}

} // namespace CommonTasks

// SequencePatterns implementation

namespace SequencePatterns {

json createLRGBSequence(const json& target, const json& exposureConfig) {
    json sequence;
    sequence["id"] = "lrgb_" + target["name"].get<std::string>();
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Sequential);
    sequence["targets"] = json::array();
    
    std::vector<std::string> filters = {"Luminance", "Red", "Green", "Blue"};
    
    for (const auto& filter : filters) {
        json filterTarget;
        filterTarget["name"] = target["name"].get<std::string>() + "_" + filter;
        filterTarget["tasks"] = json::array({
            {
                {"type", "device_task"},
                {"parameters", CommonTasks::filterChange(filter)}
            },
            {
                {"type", "script_task"},
                {"parameters", CommonTasks::standardExposure(
                    exposureConfig[filter]["exposure_time"],
                    exposureConfig[filter]["gain"],
                    exposureConfig.value("binning", 1),
                    filter
                )}
            }
        });
        
        sequence["targets"].push_back(filterTarget);
    }
    
    return sequence;
}

json createNarrowbandSequence(const json& target, const json& exposureConfig) {
    json sequence;
    sequence["id"] = "narrowband_" + target["name"].get<std::string>();
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Sequential);
    sequence["targets"] = json::array();
    
    std::vector<std::string> filters = {"Ha", "OIII", "SII"};
    
    for (const auto& filter : filters) {
        if (exposureConfig.contains(filter)) {
            json filterTarget;
            filterTarget["name"] = target["name"].get<std::string>() + "_" + filter;
            filterTarget["tasks"] = json::array({
                {
                    {"type", "device_task"},
                    {"parameters", CommonTasks::filterChange(filter)}
                },
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::standardExposure(
                        exposureConfig[filter]["exposure_time"],
                        exposureConfig[filter]["gain"],
                        exposureConfig.value("binning", 1),
                        filter
                    )}
                }
            });
            
            sequence["targets"].push_back(filterTarget);
        }
    }
    
    return sequence;
}

json createFullCalibrationSequence(const json& cameraConfig) {
    json sequence;
    sequence["id"] = "full_calibration";
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Sequential);
    sequence["targets"] = json::array();
    
    // Bias frames
    json biasTarget;
    biasTarget["name"] = "bias_frames";
    biasTarget["tasks"] = json::array({
        {
            {"type", "script_task"},
            {"parameters", CommonTasks::biasFrame(
                cameraConfig["gain"],
                cameraConfig.value("binning", 1),
                cameraConfig.value("bias_count", 30)
            )}
        }
    });
    sequence["targets"].push_back(biasTarget);
    
    // Dark frames for multiple exposures
    if (cameraConfig.contains("dark_exposures")) {
        for (const auto& exposure : cameraConfig["dark_exposures"]) {
            json darkTarget;
            darkTarget["name"] = "dark_" + std::to_string(static_cast<int>(exposure));
            darkTarget["tasks"] = json::array({
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::darkFrame(
                        exposure,
                        cameraConfig["gain"],
                        cameraConfig.value("binning", 1),
                        cameraConfig.value("dark_count", 10)
                    )}
                }
            });
            darkTarget["dependencies"] = json::array({"bias_frames"});
            sequence["targets"].push_back(darkTarget);
        }
    }
    
    // Flat frames for each filter
    if (cameraConfig.contains("filters")) {
        for (const auto& filter : cameraConfig["filters"]) {
            json flatTarget;
            flatTarget["name"] = "flat_" + filter.get<std::string>();
            flatTarget["tasks"] = json::array({
                {
                    {"type", "device_task"},
                    {"parameters", CommonTasks::filterChange(filter)}
                },
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::flatFrame(
                        cameraConfig.value("flat_exposure", 5.0),
                        cameraConfig["gain"],
                        cameraConfig.value("binning", 1),
                        filter,
                        cameraConfig.value("flat_count", 10)
                    )}
                }
            });
            sequence["targets"].push_back(flatTarget);
        }
    }
    
    return sequence;
}

json createMeridianFlipSequence(const json& target) {
    json sequence;
    sequence["id"] = "meridian_flip_" + target["name"].get<std::string>();
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Sequential);
    sequence["targets"] = json::array({
        {
            {"name", "pre_flip_pause"},
            {"tasks", json::array({
                {
                    {"type", "script_task"},
                    {"parameters", {
                        {"action", "pause_guiding"}
                    }}
                },
                {
                    {"type", "script_task"},
                    {"parameters", {
                        {"action", "stop_exposure"}
                    }}
                }
            })}
        },
        {
            {"name", "meridian_flip"},
            {"tasks", json::array({
                {
                    {"type", "device_task"},
                    {"parameters", {
                        {"action", "meridian_flip"}
                    }}
                }
            })},
            {"dependencies", json::array({"pre_flip_pause"})}
        },
        {
            {"name", "post_flip_setup"},
            {"tasks", json::array({
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::plateSolve()}
                },
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::autoFocus()}
                },
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::startGuiding()}
                }
            })},
            {"dependencies", json::array({"meridian_flip"})}
        }
    });
    
    return sequence;
}

json createDitheringSequence(const json& baseSequence, int ditherSteps) {
    json sequence = baseSequence;
    
    // Add dithering steps between exposures
    if (sequence.contains("targets")) {
        for (auto& target : sequence["targets"]) {
            if (target.contains("tasks")) {
                json newTasks = json::array();
                
                for (size_t i = 0; i < target["tasks"].size(); ++i) {
                    newTasks.push_back(target["tasks"][i]);
                    
                    // Add dither after every ditherSteps exposures
                    if ((i + 1) % ditherSteps == 0 && i < target["tasks"].size() - 1) {
                        newTasks.push_back({
                            {"type", "script_task"},
                            {"parameters", {
                                {"action", "dither"},
                                {"pixels", 5},
                                {"settle_time", 10}
                            }}
                        });
                    }
                }
                
                target["tasks"] = newTasks;
            }
        }
    }
    
    return sequence;
}

json createMosaicSequence(const std::vector<json>& targets, const json& exposureConfig) {
    json sequence;
    sequence["id"] = "mosaic_sequence";
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Adaptive);
    sequence["targets"] = json::array();
    
    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        
        json mosaicTarget;
        mosaicTarget["name"] = "mosaic_panel_" + std::to_string(i + 1);
        mosaicTarget["tasks"] = json::array({
            {
                {"type", "device_task"},
                {"parameters", {
                    {"action", "slew_to_target"},
                    {"ra", target["ra"]},
                    {"dec", target["dec"]}
                }}
            },
            {
                {"type", "script_task"},
                {"parameters", CommonTasks::plateSolve()}
            },
            {
                {"type", "script_task"},
                {"parameters", CommonTasks::autoFocus()}
            },
            {
                {"type", "script_task"},
                {"parameters", CommonTasks::standardExposure(
                    exposureConfig["exposure_time"],
                    exposureConfig["gain"],
                    exposureConfig.value("binning", 1),
                    exposureConfig.value("filter", "Luminance")
                )}
            }
        });
        
        sequence["targets"].push_back(mosaicTarget);
    }
    
    return sequence;
}

json createFocusMaintenanceSequence(int intervalMinutes) {
    json sequence;
    sequence["id"] = "focus_maintenance";
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Sequential);
    sequence["targets"] = json::array({
        {
            {"name", "periodic_focus_check"},
            {"tasks", json::array({
                {
                    {"type", "script_task"},
                    {"parameters", {
                        {"action", "temperature_focus_check"},
                        {"interval_minutes", intervalMinutes},
                        {"temperature_threshold", 2.0}
                    }}
                },
                {
                    {"type", "script_task"},
                    {"parameters", CommonTasks::autoFocus("Luminance", 5, 50)}
                }
            })}
        }
    });
    
    return sequence;
}

json createWeatherMonitoringSequence(int checkIntervalMinutes) {
    json sequence;
    sequence["id"] = "weather_monitoring";
    sequence["strategy"] = static_cast<int>(ExecutionStrategy::Sequential);
    sequence["targets"] = json::array({
        {
            {"name", "weather_safety_check"},
            {"tasks", json::array({
                {
                    {"type", "script_task"},
                    {"parameters", {
                        {"action", "weather_monitoring"},
                        {"interval_minutes", checkIntervalMinutes},
                        {"safety_thresholds", {
                            {"wind_speed_max", 20},
                            {"humidity_max", 80},
                            {"cloud_cover_max", 50},
                            {"rain_detection", true}
                        }}
                    }}
                }
            })}
        }
    });
    
    return sequence;
}

} // namespace SequencePatterns

// TaskValidation implementation

namespace TaskValidation {

bool validateExposureParams(const json& params) {
    std::vector<std::string> required = {"exposure_time", "gain"};
    
    for (const auto& req : required) {
        if (!params.contains(req)) {
            return false;
        }
    }
    
    if (params["exposure_time"].get<double>() <= 0) {
        return false;
    }
    
    int gain = params["gain"].get<int>();
    if (gain < 0 || gain > 2000) {
        return false;
    }
    
    return true;
}

bool validateDeviceParams(const json& params) {
    if (!params.contains("action") || !params["action"].is_string()) {
        return false;
    }
    
    std::string action = params["action"];
    if (action == "connect" && !params.contains("device_type")) {
        return false;
    }
    
    return true;
}

bool validateFilterParams(const json& params) {
    if (!params.contains("filter") || !params["filter"].is_string()) {
        return false;
    }
    
    return true;
}

bool validateFocusParams(const json& params) {
    if (params.contains("samples")) {
        int samples = params["samples"].get<int>();
        if (samples < 3 || samples > 20) {
            return false;
        }
    }
    
    if (params.contains("step_size")) {
        double stepSize = params["step_size"].get<double>();
        if (stepSize <= 0 || stepSize > 1000) {
            return false;
        }
    }
    
    return true;
}

bool validateGuidingParams(const json& params) {
    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"].get<double>();
        if (exposure <= 0 || exposure > 30) {
            return false;
        }
    }
    
    if (params.contains("tolerance")) {
        double tolerance = params["tolerance"].get<double>();
        if (tolerance <= 0 || tolerance > 10) {
            return false;
        }
    }
    
    return true;
}

bool validateScriptParams(const json& params) {
    if (!params.contains("script_type") || !params["script_type"].is_string()) {
        return false;
    }
    
    if (params.contains("timeout")) {
        double timeout = params["timeout"].get<double>();
        if (timeout <= 0) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> getRequiredParameters(const std::string& taskType) {
    static std::unordered_map<std::string, std::vector<std::string>> requirements = {
        {"script_task", {"script_type"}},
        {"device_task", {"action"}},
        {"config_task", {"action"}},
        {"exposure_task", {"exposure_time", "gain"}},
        {"filter_task", {"filter"}},
        {"focus_task", {}},
        {"guiding_task", {}}
    };
    
    auto it = requirements.find(taskType);
    return (it != requirements.end()) ? it->second : std::vector<std::string>{};
}

json getParameterSchema(const std::string& taskType) {
    static std::unordered_map<std::string, json> schemas = {
        {"script_task", {
            {"script_type", {{"type", "string"}, {"required", true}}},
            {"timeout", {{"type", "number"}, {"minimum", 0}}},
            {"arguments", {{"type", "array"}}}
        }},
        {"device_task", {
            {"action", {{"type", "string"}, {"required", true}}},
            {"device_type", {{"type", "string"}}},
            {"timeout", {{"type", "number"}, {"minimum", 0}}}
        }},
        {"config_task", {
            {"action", {{"type", "string"}, {"required", true}}},
            {"parameters", {{"type", "object"}}}
        }}
    };
    
    auto it = schemas.find(taskType);
    return (it != schemas.end()) ? it->second : json::object();
}

} // namespace TaskValidation

// TaskUtils implementation

namespace TaskUtils {

std::chrono::seconds calculateSequenceTime(const json& sequence) {
    std::chrono::seconds totalTime{0};
    
    if (sequence.contains("targets")) {
        for (const auto& target : sequence["targets"]) {
            if (target.contains("tasks")) {
                for (const auto& task : target["tasks"]) {
                    if (task.contains("parameters")) {
                        auto params = task["parameters"];
                        
                        // Estimate time based on task type
                        if (params.contains("exposure_time")) {
                            double exposure = params["exposure_time"].get<double>();
                            int count = params.value("count", 1);
                            totalTime += std::chrono::seconds(static_cast<int>(exposure * count));
                        } else {
                            // Default task overhead
                            totalTime += std::chrono::seconds(10);
                        }
                    }
                }
            }
        }
    }
    
    return totalTime;
}

size_t estimateDiskSpace(const json& sequence) {
    size_t totalSpace = 0;
    
    // Assume 16-bit images, typical camera resolution
    const size_t pixelsPerImage = 4096 * 4096; // 16MP camera
    const size_t bytesPerPixel = 2; // 16-bit
    const size_t imageSize = pixelsPerImage * bytesPerPixel;
    
    if (sequence.contains("targets")) {
        for (const auto& target : sequence["targets"]) {
            if (target.contains("tasks")) {
                for (const auto& task : target["tasks"]) {
                    if (task.contains("parameters")) {
                        auto params = task["parameters"];
                        
                        if (params.contains("frame_type") && 
                            (params["frame_type"] == "light" || 
                             params["frame_type"] == "dark" || 
                             params["frame_type"] == "flat" || 
                             params["frame_type"] == "bias")) {
                            
                            int count = params.value("count", 1);
                            int binning = params.value("binning", 1);
                            size_t binnedImageSize = imageSize / (binning * binning);
                            
                            totalSpace += binnedImageSize * count;
                        }
                    }
                }
            }
        }
    }
    
    // Add 20% overhead for metadata, processing files, etc.
    return totalSpace * 1.2;
}

bool checkResourceAvailability(const json& sequence) {
    // Check estimated disk space
    size_t requiredSpace = estimateDiskSpace(sequence);
    
    // This would typically check actual available disk space
    // For now, we'll assume we need at least 1GB free
    const size_t minimumFreeSpace = 1024 * 1024 * 1024; // 1GB
    
    if (requiredSpace > minimumFreeSpace) {
        LOG_F(WARNING, "Sequence requires {} MB, but only {} MB assumed available", 
              requiredSpace / (1024 * 1024), minimumFreeSpace / (1024 * 1024));
    }
    
    // Check estimated time
    auto totalTime = calculateSequenceTime(sequence);
    const auto maxTime = std::chrono::hours(12); // 12-hour limit
    
    if (totalTime > maxTime) {
        LOG_F(WARNING, "Sequence estimated time {} hours exceeds maximum {}", 
              std::chrono::duration_cast<std::chrono::hours>(totalTime).count(),
              std::chrono::duration_cast<std::chrono::hours>(maxTime).count());
        return false;
    }
    
    return true;
}

json optimizeSequenceOrder(const json& sequence) {
    json optimized = sequence;
    
    // Simple optimization: group tasks by type to minimize setup overhead
    if (optimized.contains("targets")) {
        for (auto& target : optimized["targets"]) {
            if (target.contains("tasks")) {
                auto& tasks = target["tasks"];
                
                // Sort tasks: device tasks first, then script tasks, then config tasks
                std::sort(tasks.begin(), tasks.end(), [](const json& a, const json& b) {
                    static const std::map<std::string, int> priority = {
                        {"device_task", 1},
                        {"script_task", 2},
                        {"config_task", 3}
                    };
                    
                    int aPrio = priority.count(a["type"]) ? priority.at(a["type"]) : 99;
                    int bPrio = priority.count(b["type"]) ? priority.at(b["type"]) : 99;
                    
                    return aPrio < bPrio;
                });
            }
        }
    }
    
    return optimized;
}

std::vector<json> splitSequence(const json& sequence, size_t maxChunkSize) {
    std::vector<json> chunks;
    
    if (!sequence.contains("targets")) {
        return {sequence};
    }
    
    auto targets = sequence["targets"];
    
    for (size_t i = 0; i < targets.size(); i += maxChunkSize) {
        json chunk = sequence;
        chunk["targets"] = json::array();
        
        size_t end = std::min(i + maxChunkSize, targets.size());
        for (size_t j = i; j < end; ++j) {
            chunk["targets"].push_back(targets[j]);
        }
        
        chunk["id"] = sequence.value("id", "sequence") + "_chunk_" + std::to_string(chunks.size() + 1);
        chunks.push_back(chunk);
    }
    
    return chunks;
}

json mergeSequences(const std::vector<json>& sequences) {
    if (sequences.empty()) {
        return json::object();
    }
    
    json merged = sequences[0];
    merged["id"] = "merged_sequence";
    
    if (merged.contains("targets")) {
        merged["targets"] = json::array();
        
        for (const auto& seq : sequences) {
            if (seq.contains("targets")) {
                for (const auto& target : seq["targets"]) {
                    merged["targets"].push_back(target);
                }
            }
        }
    }
    
    return merged;
}

json generateSequenceReport(const json& sequence) {
    json report;
    
    report["summary"] = {
        {"sequence_id", sequence.value("id", "unknown")},
        {"target_count", sequence.contains("targets") ? sequence["targets"].size() : 0},
        {"estimated_time", calculateSequenceTime(sequence).count()},
        {"estimated_disk_space_mb", estimateDiskSpace(sequence) / (1024 * 1024)},
        {"strategy", sequence.value("strategy", 0)}
    };
    
    // Task breakdown
    std::map<std::string, int> taskCounts;
    int totalTasks = 0;
    
    if (sequence.contains("targets")) {
        for (const auto& target : sequence["targets"]) {
            if (target.contains("tasks")) {
                for (const auto& task : target["tasks"]) {
                    std::string taskType = task.value("type", "unknown");
                    taskCounts[taskType]++;
                    totalTasks++;
                }
            }
        }
    }
    
    report["task_breakdown"] = {
        {"total_tasks", totalTasks},
        {"by_type", taskCounts}
    };
    
    // Validation results
    report["validation"] = {
        {"resource_check", checkResourceAvailability(sequence)},
        {"estimated_valid", totalTasks > 0}
    };
    
    return report;
}

} // namespace TaskUtils
