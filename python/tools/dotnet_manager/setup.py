"""Enhanced setup script for dotnet_manager package."""

from setuptools import setup, find_packages
from pathlib import Path

# Read the README file for long description
readme_path = Path(__file__).parent / "README.md"
try:
    with open(readme_path, encoding="utf-8") as f:
        long_description = f.read()
except FileNotFoundError:
    long_description = "A comprehensive utility for managing .NET Framework installations on Windows systems"

setup(
    name="dotnet_manager",
    version="3.1.0",
    description="Enhanced .NET Framework manager with modern Python features and robust error handling",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Max Qian",
    author_email="astro_air@126.com",
    url="https://github.com/max-qian/lithium-next",
    packages=find_packages(),
    python_requires=">=3.9",
    install_requires=[
        "loguru>=0.6.0",
        "tqdm>=4.64.0",
        "aiohttp>=3.8.0",
        "aiofiles>=0.8.0",
    ],
    extras_require={
        "dev": [
            "pytest>=7.0.0",
            "pytest-asyncio>=0.20.0",
            "mypy>=1.0.0",
            "black>=22.0.0",
            "isort>=5.10.0",
            "flake8>=5.0.0",
        ],
        "test": [
            "pytest>=7.0.0",
            "pytest-asyncio>=0.20.0",
            "pytest-cov>=4.0.0",
        ],
    },
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Intended Audience :: System Administrators",
        "License :: OSI Approved :: MIT License",
        "Operating System :: Microsoft :: Windows",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: System :: Installation/Setup",
        "Topic :: System :: Systems Administration",
        "Topic :: Utilities",
        "Typing :: Typed",
    ],
    keywords="dotnet framework windows installer manager async",
    entry_points={
        "console_scripts": [
            "dotnet-manager=dotnet_manager.cli:main",
        ],
    },
    project_urls={
        "Bug Reports": "https://github.com/max-qian/lithium-next/issues",
        "Source": "https://github.com/max-qian/lithium-next",
        "Documentation": "https://github.com/max-qian/lithium-next/tree/master/python/tools/dotnet_manager",
    },
    include_package_data=True,
    zip_safe=False,
)
