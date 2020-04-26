# WPANTAP - A TAP Interface for WPAN

Our goal: to bridge simulated WPAN devices together.

Authors:
- [Ziyue "Alan" Xiang](https://www.alanshawn.com)
- [Li Li](https://www.github.com/Li-Syr)

## Progress

- [x] Implement file system read
- [x] Implement file system write
- [ ] Implement file system polling
- [ ] Optimize FCS

## Building

The kernel module source is under `./kmodule`.

```bash
cd kmodule
# cleans the build files
sudo make clean
# uninstall the symlink in kernel module folder
sudo make uninstall
# build the module
sudo make build
# install the module (create symlink)
sudo make install
```

After calling `sudo make install`, to update the kernel module, we only need to call `sudo make build`.

## Testing
- To show more debug message, set the kernel log level with `dmesg -n 8`
- Use `dmesg -w` to monitor logs at real time.
- Build test programs in `./test`
- The kernel module creates one WPAN interface called `wpan0`. To set up the interface, call `sudo ip link set wpan0 up`.
- In the testing folder, run `af_packet_tx` to send some packets to WPAN interface
- Use `test_read` to read packets from file system node. You can use `sudo ./test_read | xxd` to see the hex output.
- Use `test_write` to write packets to the file system node. Use wireshark to monitor if the packet can be captured.

