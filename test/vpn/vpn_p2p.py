import socket
import select
import signal
import os


# TODO: change source/dst address accordingly
my_addr = ('10.0.2.5', 12001)
peer_addr = ('10.0.2.6', 12001)


# connect UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(my_addr)

sockfd = sock.fileno()

# open wpantap file system node
tapfd = os.open('/dev/net/wpantap', os.O_RDWR)


def sig_int(signal, frame):
    sock.close()
    os.close(tapfd)
    print('VPN_P2P exited.')
    exit(0)
    
signal.signal(signal.SIGINT, sig_int)


while True:

    rfds, _, _ = select.select([sockfd, tapfd], [], [], 0.5)
    
    if tapfd in rfds:
        buf = os.read(tapfd, 1024)
        print('Received a packet ({}) from wpantap'.format(len(buf)))
        sock.sendto(buf, peer_addr)
    if sockfd in rfds:
        buf, _ = sock.recvfrom(1024)
        print('Received a packet ({}) from UDP socket'.format(len(buf)))
        # when a packet arrives via socket, it has FCS
        # we need to discard the FCS before writing it into wpantap
        os.write(tapfd, new_buf[:-2])
    
