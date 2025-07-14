import hashlib
import zlib
from unittest.mock import patch, MagicMock
import pytest
from .utils import ChecksumAlgo
from .exceptions import ChecksumError

# filepath: /home/max/lithium-next/python/tools/convert_to_header/test_checksum.py


# Use relative imports as the directory is a package
from .checksum import (
    generate_checksum, verify_checksum, _get_checksum_calculator,
    Md5Checksum, Sha1Checksum, Sha256Checksum, Sha512Checksum, Crc32Checksum
)

# Define some test data
TEST_DATA_BYTES = b"This is some test data for checksumming."
TEST_DATA_EMPTY_BYTES = b""

# Pre-calculate correct checksums for the test data
CORRECT_CHECKSUMS = {
    ChecksumAlgo.MD5: hashlib.md5(TEST_DATA_BYTES).hexdigest(),
    ChecksumAlgo.SHA1: hashlib.sha1(TEST_DATA_BYTES).hexdigest(),
    ChecksumAlgo.SHA256: hashlib.sha256(TEST_DATA_BYTES).hexdigest(),
    ChecksumAlgo.SHA512: hashlib.sha512(TEST_DATA_BYTES).hexdigest(),
    ChecksumAlgo.CRC32: f"{zlib.crc32(TEST_DATA_BYTES) & 0xFFFFFFFF:08x}",
}

# Pre-calculate correct checksums for empty data
CORRECT_EMPTY_CHECKSUMS = {
    ChecksumAlgo.MD5: hashlib.md5(TEST_DATA_EMPTY_BYTES).hexdigest(),
    ChecksumAlgo.SHA1: hashlib.sha1(TEST_DATA_EMPTY_BYTES).hexdigest(),
    ChecksumAlgo.SHA256: hashlib.sha256(TEST_DATA_EMPTY_BYTES).hexdigest(),
    ChecksumAlgo.SHA512: hashlib.sha512(TEST_DATA_EMPTY_BYTES).hexdigest(),
    ChecksumAlgo.CRC32: f"{zlib.crc32(TEST_DATA_EMPTY_BYTES) & 0xFFFFFFFF:08x}",
}


# --- Tests for verify_checksum ---

@pytest.mark.parametrize("algorithm", list(ChecksumAlgo))
def test_verify_checksum_success(algorithm: ChecksumAlgo):
    """Test successful checksum verification for all supported algorithms."""
    expected_checksum = CORRECT_CHECKSUMS[algorithm]
    assert verify_checksum(TEST_DATA_BYTES, expected_checksum, algorithm) is True


@pytest.mark.parametrize("algorithm", list(ChecksumAlgo))
def test_verify_checksum_success_empty_data(algorithm: ChecksumAlgo):
    """Test successful checksum verification for empty data."""
    expected_checksum = CORRECT_EMPTY_CHECKSUMS[algorithm]
    assert verify_checksum(TEST_DATA_EMPTY_BYTES, expected_checksum, algorithm) is True


@pytest.mark.parametrize("algorithm", list(ChecksumAlgo))
def test_verify_checksum_mismatch(algorithm: ChecksumAlgo):
    """Test checksum verification failure due to mismatch."""
    # Use a checksum from a different algorithm or a modified one
    wrong_checksum = "a" * len(CORRECT_CHECKSUMS[algorithm]) # Create a checksum of the correct length but wrong value
    if wrong_checksum == CORRECT_CHECKSUMS[algorithm]: # Handle edge case if the above creates the correct one
         wrong_checksum = "b" * len(CORRECT_CHECKSUMS[algorithm])

    assert verify_checksum(TEST_DATA_BYTES, wrong_checksum, algorithm) is False


@pytest.mark.parametrize("algorithm", list(ChecksumAlgo))
def test_verify_checksum_case_insensitivity(algorithm: ChecksumAlgo):
    """Test that checksum verification is case-insensitive."""
    expected_checksum_upper = CORRECT_CHECKSUMS[algorithm].upper()
    assert verify_checksum(TEST_DATA_BYTES, expected_checksum_upper, algorithm) is True

    expected_checksum_lower = CORRECT_CHECKSUMS[algorithm].lower()
    assert verify_checksum(TEST_DATA_BYTES, expected_checksum_lower, algorithm) is True


def test_verify_checksum_invalid_data_type():
    """Test checksum verification with invalid data type (not bytes)."""
    with pytest.raises(ChecksumError) as excinfo:
        verify_checksum("not bytes", "some_checksum", ChecksumAlgo.MD5)

    assert excinfo.value.error_code == "INVALID_INPUT_TYPE"
    assert "Data must be bytes" in str(excinfo.value)
    assert excinfo.value.context["algorithm"] == ChecksumAlgo.MD5
    assert excinfo.value.context["expected_checksum"] == "some_checksum"


def test_verify_checksum_invalid_expected_checksum_type():
    """Test checksum verification with invalid expected checksum type (not string)."""
    with pytest.raises(ChecksumError) as excinfo:
        verify_checksum(TEST_DATA_BYTES, 12345, ChecksumAlgo.MD5) # Pass an integer

    assert excinfo.value.error_code == "INVALID_INPUT_TYPE"
    assert "Expected checksum must be string" in str(excinfo.value)
    assert excinfo.value.context["algorithm"] == ChecksumAlgo.MD5
    assert excinfo.value.context["expected_checksum"] == "12345" # Should be converted to string in context


@patch('tools.convert_to_header.checksum.generate_checksum')
def test_verify_checksum_generate_checksum_error(mock_generate_checksum):
    """Test checksum verification when generate_checksum raises an error."""
    mock_generate_checksum.side_effect = ChecksumError(
        "Mock generation failed", algorithm=ChecksumAlgo.SHA256
    )

    with pytest.raises(ChecksumError) as excinfo:
        verify_checksum(TEST_DATA_BYTES, "some_checksum", ChecksumAlgo.SHA256)

    assert excinfo.value.error_code == "MOCK_GENERATION_FAILED" # ChecksumError propagates
    assert "Mock generation failed" in str(excinfo.value)
    assert excinfo.value.context["algorithm"] == ChecksumAlgo.SHA256


@patch('tools.convert_to_header.checksum.generate_checksum')
def test_verify_checksum_unexpected_error_during_generation(mock_generate_checksum):
    """Test checksum verification when generate_checksum raises an unexpected exception."""
    mock_generate_checksum.side_effect = Exception("Unexpected error during hash")

    with pytest.raises(ChecksumError) as excinfo:
        verify_checksum(TEST_DATA_BYTES, "some_checksum", ChecksumAlgo.SHA256)

    assert excinfo.value.error_code == "VERIFICATION_FAILED" # verify_checksum catches and wraps
    assert "Failed to verify SHA256 checksum: Unexpected error during hash" in str(excinfo.value)
    assert excinfo.value.context["algorithm"] == ChecksumAlgo.SHA256
    assert excinfo.value.context["expected_checksum"] == "some_checksum"
    assert isinstance(excinfo.value.__cause__, Exception)


@patch('tools.convert_to_header.checksum._get_checksum_calculator')
def test_verify_checksum_unsupported_algorithm(mock_get_calculator):
    """Test verification with an unsupported algorithm (should be caught by generate_checksum)."""
    # Mock _get_checksum_calculator to simulate an unsupported algorithm being passed through
    # (though generate_checksum should ideally catch this first based on its implementation)
    # We test the propagation here.
    mock_get_calculator.side_effect = ChecksumError(
        "Unsupported checksum algorithm: fake_algo",
        algorithm="fake_algo"
    )

    with pytest.raises(ChecksumError) as excinfo:
        # Use a string that is not in ChecksumAlgo enum to simulate unsupported input
        verify_checksum(TEST_DATA_BYTES, "some_checksum", "fake_algo") # type: ignore

    assert excinfo.value.error_code == "UNSUPPORTED_ALGORITHM" # Error from _get_checksum_calculator propagates
    assert "Unsupported checksum algorithm: fake_algo" in str(excinfo.value)
    assert excinfo.value.context["algorithm"] == "fake_algo"
