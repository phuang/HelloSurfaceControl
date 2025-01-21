# HelloSurfaceControl

This project demonstrates the use of Surface Control and Vulkan in an Android application.

## Features

- Initialize and use Surface Control in a C++ Android app.
- Integrate GL and Vulkan for rendering.
- Manage surfaces and buffers using `AHardwareBuffer`.

## Getting Started

### Prerequisites

- Android Studio
- Android NDK
- Vulkan SDK

### Building the Project

1. Clone the repository.
2. Open the project in Android Studio.
3. Build the project using the provided Gradle scripts.

### Running the App

1. Connect an Android device or start an emulator.
2. Run the app from Android Studio.

## Code Overview

### `HelloSurfaceControl`

This class handles the initialization and management of Surface Control and Vulkan.

### `native-lib.cpp`

Contains the JNI methods to initialize and update Surface Control from the Android app.

## License

This project is licensed under the MIT License.