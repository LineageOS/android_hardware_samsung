#!/vendor/bin/sh

for f in /sys/class/thermal/thermal_zone*
do
  tz_name=`cat $f/type`
  ln -s $f /dev/thermal/tz-by-name/$tz_name
done
for f in /sys/class/thermal/cooling_device*
do
  cdev_name=`cat $f/type`
  ln -s $f /dev/thermal/cdev-by-name/$cdev_name
done
setprop vendor.thermal.link_ready 1
