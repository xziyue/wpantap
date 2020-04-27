# WPANTAP - A TAP Interface for WPAN

Our goal: to bridge simulated WPAN devices together.

Authors:
- [Ziyue "Alan" Xiang](https://www.alanshawn.com)
- [Li Li](https://www.github.com/Li-Syr)

## Progress

- [x] Implement file system read
- [x] Implement file system write
- [x] Implement file system polling
- [ ] Replace device lookup loop with a specific device pointer
- [ ] Optimize FCS

## Building

- Tested on Ubuntu 18.04
- May need to install [wpan-tools](https://github.com/linux-wpan/wpan-tools)

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
- Use `test_select` to test the `select` system call. One can run `af_packet_tx` to send some message via the WPAN interface.

### ping Test between two VMs
Now we are able to run ping test between two VMs.

- On both VMs, compile, install and run `wpantap`
- On both VMs, run `./test/lowpan_setup.sh` to configure lowpan network
- Configure the IP address in `./test/vpn/vpn_p2p.py` accordingly.
- Run wireshark with `ip netns exec wpan0 wireshark -kSl -i lowpan0 &`
- On both VMs, run VPN program with `python3`
- Run ping utility with `ip netns exec wpan0 ping6 ff02::1%lowpan0`

