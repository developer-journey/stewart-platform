# DualShock control of Stewart platform

## Pre-requisites

```bash
sudo apt-get install libudev-dev libusb-1.0-0 libusb-1.0-0-dev build-essential git
sudo npm install -g node-gyp node-pre-gyp
```

You need to add udev entries for the controller:
```bash
cat << EOF | sudo tee /etc/udev/rules.d/61-dualshock.rules
SUBSYSTEM=="input", GROUP="input", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="054c", ATTRS{idProduct}=="0268", MODE:="666", GROUP="plugdev"
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", MODE="0664", GROUP="plugdev"
 
SUBSYSTEM=="input", GROUP="input", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="054c", ATTRS{idProduct}=="05c4", MODE:="666", GROUP="plugdev"
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", MODE="0664", GROUP="plugdev"
EOF
```

Re-load the udev rules:
```bash
sudo udevadm control --reload-rules
```

Then you can install the dualshock module listed in `package.json`:

```bash
npm install
```

## Running

Plug in your DualShock controller to your USB port. If you want to use the contoller 
over BlueTooth, I haven't been able to do that reliably with my setup... so I just
plug it in via USB.

Once plugged in, launch the program via:

```bash
node stewie | sudo ../bin/transform
```
