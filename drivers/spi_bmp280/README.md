## For Raspberry PI 4

### 타겟에 업로드
- boot 디렉토리 mount
```
mount -t vfat /dev/mmcblk0p1 /boot/
```

- 생성된 dtbo 파일 /boot/overlays 폴더 내부로 복사

disable_spidev.dtbo 파일 복사

### SPI 모듈 설정

```
vi /boot/config.txt

dtoverlay=disable_spidev
dtparam=spi=on
```

```
modprobe spi_bcm2835
```

### 핀 설정 확인 (선택)

```
mount -t debugfs debugfs /sys/kernel/debug
cd /sys/kernel/debug
cat pinctrl/pinctrl-maps
```

### 모듈 삽입

```
insmod ~~~.ko
```
