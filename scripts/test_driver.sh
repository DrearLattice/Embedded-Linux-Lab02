#!/bin/sh

echo "========================================"
echo " LAB-02 CHARACTER DRIVER TEST"
echo "========================================"

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
echo "[2] Creating /dev/lab2..."

if [ ! -e /dev/lab2 ]; then
    mknod /dev/lab2 c 240 0
fi

chmod 666 /dev/lab2

echo "[PASS] /dev/lab2 ready"

echo ""
echo "[3] Write test..."
echo "Hello from userspace, LAB-02!" > /dev/lab2

echo ""
echo "[4] Read test..."
cat /dev/lab2

echo ""
echo "[5] Multiple write/read test..."

echo "Test message 1" > /dev/lab2
cat /dev/lab2

echo "Test message 2" > /dev/lab2
cat /dev/lab2

echo ""
echo "[6] Driver kernel log..."
dmesg | grep lab2 | tail -10

echo ""
echo "========================================"
echo " DRIVER TEST COMPLETE"
echo "========================================"
