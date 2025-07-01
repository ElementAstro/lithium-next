"""Setup script for dotnet_manager package."""

from setuptools import setup, find_packages

setup(
    name="dotnet_manager",
    version="3.0.0",
    description="A comprehensive utility for managing .NET Framework installations on Windows systems",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    author="Developer",
    author_email="developer@example.com",
    url="https://github.com/example/dotnet_manager",
    packages=find_packages(),
    install_requires=[
        "loguru>=0.6.0",
        "tqdm>=4.64.0",
        "aiohttp>=3.8.0",
        "aiofiles>=0.8.0",
    ],
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Intended Audience :: System Administrators",
        "License :: OSI Approved :: MIT License",
        "Operating System :: Microsoft :: Windows",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: System :: Installation/Setup",
        "Topic :: System :: Systems Administration",
        "Topic :: Utilities",
    ],
    entry_points={
        "console_scripts": [
            "dotnet-manager=dotnet_manager.cli:main",
        ],
    },
)