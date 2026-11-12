# USB Version (GTK3)

监听新插入的 U 盘并输出速度信息。

## 编译

```Bash
gcc -O2 -o usbver-gtk usbver-gtk.c $(pkg-config --cflags --libs gtk+-3.0 libusb-1.0)
```

## 使用

```Bash
sudo ./usbver-gtk
```
