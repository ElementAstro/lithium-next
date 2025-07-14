import pytest
from .core_types import CppVersion

# filepath: /home/max/lithium-next/python/tools/compiler_helper/test_core_types.py


# Use relative imports as the directory is a package


# --- Tests for CppVersion ---


def test_cppversion_enum_values():
    """Test that CppVersion enum members have the correct string values."""
    assert CppVersion.CPP98.value == "c++98"
    assert CppVersion.CPP03.value == "c++03"
    assert CppVersion.CPP11.value == "c++11"
    assert CppVersion.CPP14.value == "c++14"
    assert CppVersion.CPP17.value == "c++17"
    assert CppVersion.CPP20.value == "c++20"
    assert CppVersion.CPP23.value == "c++23"
    assert CppVersion.CPP26.value == "c++26"


def test_cppversion_str_representation():
    """Test the human-readable string representation of CppVersion members."""
    assert str(CppVersion.CPP98) == "C++98 (First Standard)"
    assert str(CppVersion.CPP11) == "C++11 (Modern C++)"
    assert str(CppVersion.CPP20) == "C++20 (Concepts & Modules)"
    assert str(CppVersion.CPP23) == "C++23 (Latest)"


def test_cppversion_is_modern():
    """Test the is_modern property."""
    assert not CppVersion.CPP98.is_modern
    assert not CppVersion.CPP03.is_modern
    assert CppVersion.CPP11.is_modern
    assert CppVersion.CPP14.is_modern
    assert CppVersion.CPP17.is_modern
    assert CppVersion.CPP20.is_modern
    assert CppVersion.CPP23.is_modern
    assert CppVersion.CPP26.is_modern


def test_cppversion_supports_modules():
    """Test the supports_modules property."""
    assert not CppVersion.CPP98.supports_modules
    assert not CppVersion.CPP03.supports_modules
    assert not CppVersion.CPP11.supports_modules
    assert not CppVersion.CPP14.supports_modules
    assert not CppVersion.CPP17.supports_modules
    assert CppVersion.CPP20.supports_modules
    assert CppVersion.CPP23.supports_modules
    assert CppVersion.CPP26.supports_modules


def test_cppversion_supports_concepts():
    """Test the supports_concepts property."""
    assert not CppVersion.CPP98.supports_concepts
    assert not CppVersion.CPP03.supports_concepts
    assert not CppVersion.CPP11.supports_concepts
    assert not CppVersion.CPP14.supports_concepts
    assert not CppVersion.CPP17.supports_concepts
    assert CppVersion.CPP20.supports_concepts
    assert CppVersion.CPP23.supports_concepts
    assert CppVersion.CPP26.supports_concepts


@pytest.mark.parametrize(
    "input_version, expected_version",
    [
        (CppVersion.CPP17, CppVersion.CPP17),  # Already an enum
        ("c++17", CppVersion.CPP17),
        ("C++17", CppVersion.CPP17),
        ("cpp17", CppVersion.CPP17),
        ("CPP17", CppVersion.CPP17),
        ("17", CppVersion.CPP17),  # Numeric
        ("2017", CppVersion.CPP17),  # Year
        ("c++20", CppVersion.CPP20),
        ("20", CppVersion.CPP20),
        ("2020", CppVersion.CPP20),
        ("c++23", CppVersion.CPP23),
        ("23", CppVersion.CPP23),
        ("2023", CppVersion.CPP23),
        ("c++98", CppVersion.CPP98),
        ("98", CppVersion.CPP98),
        ("1998", CppVersion.CPP98),
        ("c++03", CppVersion.CPP03),
        ("03", CppVersion.CPP03),
        ("2003", CppVersion.CPP03),
        ("c++11", CppVersion.CPP11),
        ("11", CppVersion.CPP11),
        ("2011", CppVersion.CPP11),
        ("c++14", CppVersion.CPP14),
        ("14", CppVersion.CPP14),
        ("2014", CppVersion.CPP14),
        ("c++26", CppVersion.CPP26),
        ("26", CppVersion.CPP26),
        ("2026", CppVersion.CPP26),
        ("c+++17", CppVersion.CPP17),  # Extra +
        ("c++++20", CppVersion.CPP20),  # More extra +
    ],
)
def test_cppversion_resolve_version_valid(input_version, expected_version):
    """Test resolve_version with various valid inputs."""
    resolved = CppVersion.resolve_version(input_version)
    assert resolved == expected_version


@pytest.mark.parametrize(
    "input_version",
    [
        "c++18",
        "c++21",
        "c++99",
        "18",
        "21",
        "2018",
        "invalid",
        "",
        None,
        17,  # Integer, not string/enum
    ],
)
def test_cppversion_resolve_version_invalid(input_version):
    """Test resolve_version with invalid inputs raises ValueError."""
    with pytest.raises(ValueError) as excinfo:
        CppVersion.resolve_version(input_version)

    assert "Invalid C++ version:" in str(excinfo.value)
    # Check that the original input is mentioned in the error message
    assert str(input_version) in str(excinfo.value)
    assert "Valid versions:" in str(excinfo.value)
