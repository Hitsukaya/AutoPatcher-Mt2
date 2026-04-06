# Nyx2 Launcher

> Lightweight, secure and modern game launcher & autopatcher  
> Built with **C++17** **C++ 2020**, native Windows APIs and clean architecture

---
<p align="center">
  <img src="https://img.shields.io/badge/C++-17-blue.svg">
  <img src="https://img.shields.io/badge/Platform-Windows-blue">
  <img src="https://img.shields.io/badge/Build-Release-success">
  <img src="https://img.shields.io/badge/Security-0%2F67%20VT%20Clean-brightgreen">
  <img src="https://img.shields.io/badge/License-Free-lightgrey">
</p>

## Overview

Nyx2 Launcher is a high-performance game launcher and patching system designed for stability, transparency and control.

Unlike many launchers that rely on heavy frameworks or questionable dependencies, Nyx2 is built using **native Windows technologies**, ensuring predictable behavior and minimal overhead.

---

## Features

- Manifest-based patching system  
- File download with progress tracking  
- File integrity verification  
- Modular architecture (easy to extend)  
- Embedded UI (WebView2)  
- Fast startup & low resource usage  
- Clean and transparent execution  

---

## Tech Stack

- **C++17**
- **WinHTTP** (native networking)
- **WebView2** (UI layer)
- **nlohmann/json**
- **CMake**

---

## Security

- ✔ 0 detections on VirusTotal  
- ✔ No obfuscated code  
- ✔ No hidden network activity  
- ✔ Uses only trusted Windows APIs  

---

## Preview

> *(Add screenshots here)*

---

Usage
Configure your manifest.json
Launch executable
Let the patcher sync files automatically

---
## Build Instructions

### Requirements

- Visual Studio 2022  
- CMake ≥ 3.16  
- vcpkg  

---

### Setup

```bash
git clone https://github.com/Hitsukaya/nyx2-launcher.git
cd nyx2-launcher
