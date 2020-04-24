# WPANTAP - A TAP Interface for WPAN

## Progress

- [x] Implement file system read
- [ ] Implement file system write

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
- Build test programs in `./test`
- The kernel module creates one WPAN interface called `wpan0`. To set up the interface, call `sudo ip link set wpan0 up`.
- In the testing folder, run `af_packet_tx` to send some packets to WPAN interface
- Use `test_read` to read packets from file system. You can use `sudo ./test_read | xxd` to see the hex output.

