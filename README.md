# LAB-02 — Embedded Linux: Device Driver & File System

> **Nền tảng:** ARMv7-A (Cortex-A9), Linux Kernel 5.15.0, QEMU `vexpress-a9`, BusyBox.  
> **Trạng thái:** Đã thực hiện Part A–D; các thao tác đọc/ghi, PROCFS, SYSFS và tự động nạp module đã được kiểm tra thủ công.  
> **Ghi chú:** README tổng hợp kết quả thực hành; nếu môn học yêu cầu báo cáo PDF riêng thì vẫn cần xuất/nộp PDF theo quy định.

## 1. Mục tiêu

- Viết character device driver có `open()`, `read()`, `write()`, `release()`.
- Giao tiếp giữa user space và kernel space bằng `copy_to_user()` và `copy_from_user()`.
- Hiển thị thông tin driver qua PROCFS và SYSFS.
- Thử nghiệm NAND mô phỏng với MTD và JFFS2.
- Cấu hình BusyBox init để tự động nạp module khi QEMU khởi động.

## 2. Môi trường

| Thành phần | Cấu hình đã dùng |
|---|---|
| Máy host | Ubuntu Linux |
| Kernel target | Linux 5.15.0 |
| Kiến trúc | ARMv7-A, Cortex-A9 |
| Trình biên dịch | `arm-linux-gnueabihf-gcc-11` |
| Mô phỏng | `qemu-system-arm -M vexpress-a9` |
| Root filesystem | BusyBox + initramfs |
| Device driver | `lab2_driver.ko`, major 240 |
| Flash mô phỏng | `nandsim`, NAND 128 MiB, eraseblock 16 KiB |

## 3. Cấu trúc project

```text
embedded_lab2/
├── driver/
│   ├── lab2_driver.c
│   ├── Makefile
│   └── lab2_driver.ko
├── rootfs/
│   └── initramfs/
│       ├── etc/
│       │   ├── inittab
│       │   └── init.d/rcS
│       ├── lib/modules/5.15.0/lab2_driver.ko
│       └── usr/bin/
│           ├── test_driver.sh
│           └── test_procfs.sh
├── output/
│   └── initramfs_lab2.cpio.gz
└── logs/
    ├── boot_log.txt
    ├── driver_test.txt
    ├── procfs_test.txt
    └── mtd_jffs2.txt
```

*Lưu ý: cây thư mục trên chỉ hiển thị các file quan trọng, không liệt kê toàn bộ BusyBox rootfs.*

## 4. Kiến trúc hệ thống

```mermaid
flowchart TD
    A[Ứng dụng user space] -->|open / read / write / close| B[VFS / System calls]
    B --> C[lab2_driver.ko]
    C --> D[Buffer 1024 bytes]
    C --> E[/proc/lab2_info]
    C --> F[/sys/class/lab2_class/lab2/]
    G[BusyBox init] --> H[/etc/init.d/rcS]
    H -->|insmod| C
    H --> I[/dev/lab2 - major 240]
```

Driver đăng ký character device major `240`, sử dụng bộ đệm `1024` byte. Mutex được dùng để bảo vệ dữ liệu dùng chung trong các thao tác cập nhật.

## 5. Kết quả thực hành

### Part A — Character Device Driver: PASS (kiểm tra thủ công)

Các callback đã triển khai: `lab2_open`, `lab2_read`, `lab2_write`, `lab2_release`. Module được biên dịch cho ARM, với `vermagic` tương thích kernel target: `5.15.0 SMP mod_unload ARMv7 p2v8`.

```sh
echo "Hello from userspace, LAB-02!" > /dev/lab2
cat /dev/lab2
```

Kết quả đọc lại:

```text
Hello from userspace, LAB-02!
```

Log kernel ghi nhận mở thiết bị, ghi 30 byte, đọc 30 byte và đóng thiết bị.

### Part B — PROCFS và SYSFS: PASS (kiểm tra thủ công)

Sau khi ghi dữ liệu:

```sh
echo "Embedded Linux Lab2 Test Data" > /dev/lab2
cat /proc/lab2_info
cat /sys/class/lab2_class/lab2/buffer_len
cat /sys/class/lab2_class/lab2/open_count
cat /sys/class/lab2_class/lab2/last_data
```

Thông tin đã quan sát:

```text
Driver name : lab2
Major number : 240
Buffer size : 1024 bytes
Data length : 30 bytes
Open count : 7
Last data : [Embedded Linux Lab2 Test Data ...]
```

SYSFS trả về `buffer_len = 30`, `open_count = 7` và nội dung dữ liệu đã ghi. `echo` thêm ký tự xuống dòng nên chiều dài là 30 byte.

### Part C — MTD, NAND simulator và JFFS2: PASS

Thực hiện **trên Ubuntu host**, không phải trong QEMU. Hệ thống có `/dev/mtd0` là BIOS NOR, vì vậy thiết bị NAND mô phỏng thực tế là **`/dev/mtd1`**, mount qua **`/dev/mtdblock1`**.

- NAND mô phỏng: 128 MiB, eraseblock 16 KiB, page 512 byte.
- Xóa NAND bằng `flash_erase`.
- Tạo ảnh JFFS2 từ các file `hello.txt`, `info.txt`, `logs/boot.log` với `mkfs.jffs2 --eraseblock=0x4000`.
- Ghi ảnh bằng `nandwrite -p /dev/mtd1 /tmp/rootfs.jffs2`.
- Mount JFFS2 và đọc thành công `hello.txt`.
- Kiểm tra ảnh bằng `jffs2dump`, dữ liệu NAND bằng `nanddump`/`hexdump`; ECC failed/corrected đều bằng 0 trong lần dump đã ghi nhận.

Ví dụ dữ liệu đọc được:

```text
Hello from JFFS2 - LAB-02
```

### Part D — BusyBox init và QEMU: PASS

`/etc/inittab` chạy `/etc/init.d/rcS`; script này mount các filesystem cần thiết, nạp `lab2_driver.ko` và tạo `/dev/lab2`.

Tạo initramfs trên host:

```sh
cd ~/embedded_lab2/rootfs/initramfs
find . | cpio -H newc -o | gzip > ~/embedded_lab2/output/initramfs_lab2.cpio.gz
```

Khởi động QEMU trên host:

```sh
qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

Log boot thực tế:

```text
[ LAB-02 ] Starting system initialization...
[ LAB-02 ] Loading lab2_driver module...
lab2: /proc/lab2_info created
lab2: driver loaded, major=240
[ LAB-02 ] lab2_driver loaded OK
[ LAB-02 ] /dev/lab2 created (major=240, minor=0)
[ LAB-02 ] System initialization complete!
```

`lsmod` cho thấy module đã được nạp; `/dev/lab2` là character device major 240, minor 0. Việc ghi/đọc qua device và đọc PROCFS/SYSFS thành công.

**Lưu ý về test script:** `test_driver.sh` và `test_procfs.sh` hiện gọi `insmod` ngay khi chạy. Trong Part D, `rcS` đã tự động nạp module nên hai script báo `File exists` ở bước `insmod`. Đây là vấn đề logic của script, **không phải lỗi driver**; các chức năng đã được xác nhận bằng lệnh thủ công. Nên chỉnh script để bỏ qua `insmod` khi module đã có trong `lsmod` trước khi dùng script làm bằng chứng PASS.

## 6. So sánh các filesystem

| Tiêu chí | ext4 | JFFS2 | UBIFS | SquashFS | cramfs |
|---|---|---|---|---|---|
| Thiết bị phù hợp | Block device (eMMC/SD/SSD) | Raw flash qua MTD | Raw flash qua UBI | Block image chỉ đọc | Block image chỉ đọc |
| Ghi dữ liệu | Có | Có | Có | Không (ảnh gốc) | Không |
| Nén dữ liệu | Không mặc định | Có | Có | Có | Có |
| Xử lý đặc tính NAND raw | Cần lớp quản lý flash bên dưới | Thiết kế cho flash | Dựa trên UBI | Cần lớp lưu trữ bên dưới | Cần lớp lưu trữ bên dưới |
| Khả năng mở rộng khi flash lớn | Tốt trên block device | Hạn chế do quét flash khi mount | Tốt hơn JFFS2 | Tốt cho rootfs tĩnh | Hạn chế, ít được dùng cho thiết kế mới |
| Phù hợp OTA | Có nếu lưu trên block device | Có thể, cần thiết kế cập nhật an toàn | Phù hợp với UBI volume và chiến lược OTA | Phù hợp ảnh rootfs bất biến | Ít linh hoạt |

**Đề xuất cho thiết bị IoT có NAND 32 MiB và OTA:** Không dùng ext4 trực tiếp trên raw NAND. Với dung lượng nhỏ, JFFS2 có thể phù hợp cho vùng dữ liệu ghi được. Nếu ưu tiên cập nhật firmware OTA an toàn, nên cân nhắc phân vùng/volume A-B hoặc cơ chế rollback; một ảnh rootfs SquashFS chỉ đọc kết hợp vùng dữ liệu ghi được (JFFS2 hoặc UBIFS/UBI, tùy cấu hình) là phương án đáng xem xét. Lựa chọn cuối cùng còn phụ thuộc RAM, dung lượng firmware, khả năng rollback và layout flash; không thể khẳng định một filesystem duy nhất luôn tốt nhất.

## 7. Analysis Questions

**Q1. Vì sao không dùng `memcpy()` để trao đổi dữ liệu giữa kernel và user space?**  
Kernel space và user space có cơ chế kiểm soát truy cập bộ nhớ riêng. Con trỏ từ user space không nên được kernel truy cập trực tiếp bằng `memcpy()` vì có thể trỏ đến vùng nhớ không hợp lệ hoặc không truy cập được. Driver sử dụng `copy_from_user()` khi ghi và `copy_to_user()` khi đọc để kiểm tra và xử lý truy cập bộ nhớ người dùng; khi thất bại trả về `-EFAULT`.

**Q2. Vì sao driver cần mutex?**  
`device_buffer`, `buffer_len` và `open_count` là dữ liệu dùng chung. Nếu nhiều tiến trình cùng ghi hoặc cập nhật đồng thời mà không đồng bộ, nội dung và độ dài buffer có thể không nhất quán (race condition). Mutex giúp bảo vệ các đoạn mã quan trọng. Khi mở rộng driver, cũng nên xem xét đồng bộ thao tác đọc với thao tác ghi.

**Q3. PROCFS khác SYSFS như thế nào?**  
PROCFS thường hiển thị thông tin trạng thái hệ thống/kernel và có thể trình bày nhiều dòng thông tin tổng hợp; trong bài này là `/proc/lab2_info`. SYSFS mô tả thiết bị và thuộc tính dưới dạng file riêng; trong bài này là `buffer_len`, `open_count`, `last_data`. Với thuộc tính thiết bị mới, SYSFS phù hợp hơn vì giao diện rõ ràng, mỗi file thể hiện một thuộc tính.

**Q4. Vì sao JFFS2 cần thao tác erase còn ext4 không thao tác trực tiếp như vậy?**  
NAND flash không thể ghi đè tùy ý lên dữ liệu cũ và phải xóa theo eraseblock trước khi tái sử dụng. JFFS2 được thiết kế cho flash raw, sử dụng cơ chế log-structured và quản lý các vấn đề của flash. ext4 hoạt động trên thiết bị block và dựa vào lớp thiết bị/FTL bên dưới để xử lý các đặc tính vật lý của flash nếu có.

**Q5. BusyBox init, inittab và rcS có vai trò gì? Nếu `insmod` thất bại thì sao?**  
BusyBox `init` là tiến trình khởi tạo userspace. `/etc/inittab` chỉ định script khởi động; `/etc/init.d/rcS` thực hiện mount filesystem, nạp module và tạo device node. Nếu `insmod` thất bại, hệ thống có tiếp tục boot hay không tùy cách xử lý lỗi trong script. Trong LAB-02, nên thông báo lỗi rõ ràng và tiếp tục vào shell để có thể debug, thay vì làm cả hệ thống dừng không cần thiết.

## 8. Khó khăn và cách khắc phục

**Khó khăn 1 — Biên dịch kernel module lỗi instruction `isb`.**  
Ban đầu compiler/flags không phù hợp với cấu hình kernel ARMv7-A. Đã chọn `arm-linux-gnueabihf-gcc-11` giống compiler build kernel và bổ sung `ccflags-y += -march=armv7-a` trong Makefile. Không ép `-mfloat-abi=hard` cho kernel module. Sau đó module build thành công, `vermagic` khớp kernel 5.15.0.

**Khó khăn 2 — Nhầm MTD device.**  
Host có `/dev/mtd0` là BIOS NOR read-only, trong khi NAND simulator thực tế là `/dev/mtd1`. Đã kiểm tra `/proc/mtd`, `mtdinfo` và chuyển thao tác `flash_erase`, `nandwrite`, `nanddump` sang `/dev/mtd1`; mount bằng `/dev/mtdblock1`.

**Khó khăn 3 — Test script báo lỗi `File exists`.**  
Trong QEMU, `rcS` đã tự động nạp module trước khi chạy test. Script cố `insmod` lần nữa nên thất bại. Kiểm tra thủ công xác nhận driver hoạt động. Cần cải thiện script để chỉ nạp module khi chưa có.

**Khó khăn 4 — Sao chép device node sang rootfs.**  
Khi copy rootfs từ LAB-01, tài khoản thường không thể tạo các special file `/dev/console` và `/dev/null`. Đã cấu hình `rcS` để tạo node cần thiết khi boot nếu chưa tồn tại.

## 9. Kết luận

LAB-02 đã thực hành thành công quy trình xây dựng character device driver cho ARM Linux, xuất thông tin qua PROCFS/SYSFS, sử dụng NAND mô phỏng với JFFS2 và tự động nạp module bằng BusyBox init trong QEMU. Các chức năng chính được xác minh bằng log và kiểm tra thủ công. Để hoàn thiện repo, cần lưu đầy đủ log, cải thiện hai test script khi module đã được nạp và bổ sung ảnh chụp kết quả nếu giảng viên yêu cầu.

## 10. Minh chứng

- [`logs/boot_log.txt`](logs/boot_log.txt): boot QEMU và auto-load.
- [`logs/driver_test.txt`](logs/driver_test.txt): đọc/ghi character device.
- [`logs/procfs_test.txt`](logs/procfs_test.txt): thông tin PROCFS/SYSFS.
- [`logs/mtd_jffs2.txt`](logs/mtd_jffs2.txt): MTD/NAND/JFFS2.

> Không đưa lên GitHub các file cá nhân, khóa bí mật hoặc các thư mục build tạm không cần thiết.
