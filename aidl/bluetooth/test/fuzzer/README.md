# Fuzzer for android.hardware.bluetooth@1.0-impl-test

## Plugin Design Considerations
The fuzzer plugin for android.hardware.bluetooth@1.0-impl-test is designed based on the understanding of the source code and tries to achieve the following:

##### Maximize code coverage
1. The configuration parameters are not hardcoded, but instead selected based on
incoming data. This ensures more code paths are reached by the fuzzer.

2. A new library *'libbt-vendor-fuzz.so'* is created that implements functions of `bt_vendor_interface_t` and calls them in order to maximize the code coverage

android.hardware.bluetooth@1.0-impl-test supports the following parameters:

1. Bluetooth Address (parameter name: `btAddress`)

| Parameter| Valid Values| Configured Value|
|------------- |-------------| ----- |
| `btAddress` | Values inside array ranges from `0x0` to `0xFF`| Value obtained from FuzzedDataProvider|

This also ensures that the plugin is always deterministic for any given input.

##### Maximize utilization of input data
The plugin feeds the entire input data to the module.
This ensures that the plugin tolerates any kind of input (empty, huge,
malformed, etc) and doesnt `exit()` on any input and thereby increasing the
chance of identifying vulnerabilities.

## Build

This describes steps to build bluetoothV1.0_fuzzer binary.

### Android

#### Steps to build
Build the fuzzer
```
  $ mm -j$(nproc) bluetoothV1.0_fuzzer
```
#### Steps to run
To run on device
```
  $ adb sync data
  $ adb shell LD_LIBRARY_PATH=/data/fuzz/${TARGET_ARCH}/lib/ /data/fuzz/${TARGET_ARCH}/bluetoothV1.0_fuzzer/bluetoothV1.0_fuzzer
```

## References:
 * http://llvm.org/docs/LibFuzzer.html
 * https://github.com/google/oss-fuzz
