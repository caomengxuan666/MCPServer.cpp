#!/usr/bin/env python3
"""
Build script for Python plugins
This script simplifies the process of building Python plugins into DLLs
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

def build_plugin(plugin_dir, build_dir="build"):
    """
    Build a Python plugin into a DLL
    
    Args:
        plugin_dir (str): Path to the plugin directory
        build_dir (str): Name of the build directory
    """
    
    plugin_path = Path(plugin_dir).absolute()
    build_path = plugin_path / build_dir
    
    # Create build directory
    build_path.mkdir(exist_ok=True)
    
    # Change to build directory
    os.chdir(build_path)
    
    # Run CMake configuration
    print("Configuring with CMake...")
    cmake_config_cmd = [
        "cmake", 
        "..",
        f"-DMCP_SERVER_ROOT={Path(__file__).parent.parent.parent.absolute()}"
    ]
    
    result = subprocess.run(cmake_config_cmd)
    if result.returncode != 0:
        print("CMake configuration failed!")
        return False
    
    # Build the plugin
    print("Building plugin...")
    cmake_build_cmd = ["cmake", "--build", ".", "--config", "Release"]
    result = subprocess.run(cmake_build_cmd)
    if result.returncode != 0:
        print("Build failed!")
        return False
    
    print("Build completed successfully!")
    return True

def main():
    parser = argparse.ArgumentParser(description="Build Python plugin for MCPServer++")
    parser.add_argument("--plugin-dir", default=".", help="Path to plugin directory (default: current directory)")
    parser.add_argument("--build-dir", default="build", help="Build directory name (default: build)")
    
    args = parser.parse_args()
    
    if build_plugin(args.plugin_dir, args.build_dir):
        print("Plugin built successfully!")
    else:
        print("Failed to build plugin!")
        sys.exit(1)

if __name__ == "__main__":
    main()