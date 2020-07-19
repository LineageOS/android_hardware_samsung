# Samsung HIDL thermal HAL

The HAL uses the standard linux thermal interface and can be configured by
adding thermal zones and cooling devices present on the device in a
`thermal_info_config.json` file.

To probe them, just connect the phone via ADB and check the nodes available
under `/sys/class/thermal/`.  The name of each thermal zone and cooling device
can be found in the type node, e.g.

    /sys/class/thermal/thermal_zone0/type
    /sys/class/thermal/cooling_device0/type

For each thermal device it is possible to configure a "Sensor" node in
`thermal_info_config.json`, and setting up to 7 throttling levels, from NONE to
SHUTDOWN. At each severity level, the hal send signals to throttle the device to the 
framework, according to : https://source.android.com/devices/architecture/hidl/thermal-mitigation
In order to set temperature curve for the desired component you can 
took as a refererence the kernel trip points temperatures, for the specific devices,
available in the thermal_zoneX sysfs.
Each sensor can be classified as *CPU*, *GPU*, *USB_PORT* or *BATTERY* type.
If you have a thermal monitor which does not belong to any of this categories you can 
classify it as *UNKNOWN*.
You can specify hysteresis as well as if the interface should be monitored.
If you monitor the interface, the HAL takes action when the specifc treshold is passed.
You should not enable monitor if your kernel already implement thermal mitigatoin for 
the specified component.
Since each device reports temperatures multiplied by different factor of 10,
you should set the Multipler field such as
`/sys/class/thermal/thermal_zoneX/temp` is reported in Celusis degrees (e.g.
25100 reported by sysfs, multiplied by 0.001 is 25.1).

The same can be said for cooling devices. For each cooling devices can be
created a CoolingDevices node in `thermal_info_config.json`.
Each cooling interface can be classified as *CPU*, *GPU* or *BATTERY* type.
If you have a cooling device which does not belong to any of this categories you can 
classify it as *COMPONENT*.
The `thermal_info_config.json` should be copied under /vendor/etc.

For more details, refer on the sample config schema.
