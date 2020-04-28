# Sample WPAN VPN

The `wpantap` driver allows us to connect multiple IoT device simulation into the same network.

## P2P VPN

`vpn_p2p.py` a sample P2P VPN program. To modify the IP settings of this machine and peer machine, compose a json file as follows and save it as `vpn_p2p_config.json` in this directory.

```json
{
  "me": {
    "ip": "10.0.2.5",
    "port": 12001
  },
  "peer": {
    "ip": "10.0.2.6",
    "port": 12001
  }
}
```
