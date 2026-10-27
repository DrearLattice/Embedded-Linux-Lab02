#!/bin/sh

echo "========================================"
echo " LAB-02 PROCFS + SYSFS TEST"
echo "========================================"

echo ""
echo "[1] Loading driver..."
if grep -q '^lab2_driver ' /proc/modules; then
    echo "[PASS] Driver already loaded"
else
    if insmod /lib/modules/5.15.0/lab2_driver.ko; then
        echo "[PASS] Driver loaded"
    else
        echo "[FAIL] Driver load failed"
        exit 1
    fi
fi

echo ""
echo "[2] Creating device node..."
if [ ! -e /dev/lab2 ]; then
    mknod /dev/lab2 c 240 0
fi
chmod 666 /dev/lab2

echo ""
echo "[3] Read /proc/lab2_info BEFORE write"
echo "----------------------------------------"
cat /proc/lab2_info

echo ""
echo "[4] Write test data..."
echo "Embedded Linux Lab2 Test Data" > /dev/lab2

echo ""
echo "[5] Read /proc/lab2_info AFTER write"
echo "----------------------------------------"
cat /proc/lab2_info

echo ""
echo "[6] Read SYSFS attributes"
echo "----------------------------------------"

echo "buffer_len:"
cat /sys/class/lab2_class/lab2/buffer_len

echo "open_count:"
cat /sys/class/lab2_class/lab2/open_count

echo "last_data:"
cat /sys/class/lab2_class/lab2/last_data

echo ""
echo "========================================"
echo " PROCFS + SYSFS TEST COMPLETE"
echo "========================================"
