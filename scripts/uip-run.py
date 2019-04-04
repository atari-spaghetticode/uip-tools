#!/usr/bin/env python

REC_UDP_PORT = 33334
SEND_UDP_PORT = 33332

import socket
import fcntl, os
import sys, select, tty, termios
import httplib
import curses

class NonBlockingConsole(object):
    def __enter__(self):
        self.old_settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
        self.fd = sys.stdin.fileno()
        self.fl = fcntl.fcntl(self.fd, fcntl.F_GETFL)
        fcntl.fcntl(self.fd, fcntl.F_SETFL, self.fl | os.O_NONBLOCK)
        return self

    def __exit__(self, type, value, traceback):
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)

    def get_data(self):
        try:
            data = ""
            while select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
                data = data + sys.stdin.read(32)
            return data
        except:
            return '[CTRL-C]'
        return False

curses.setupterm()
inverse_seq = curses.tigetstr('rev').decode("utf-8")
normal_seq = curses.tigetstr('sgr0').decode("utf-8")

local_to_atari_terminal = {
    ('\x7f', '\x00\x0e\x00\x08'),                                       #backspace
    # F-keys
    (curses.tigetstr('kf1').decode("utf-8"),  '\x00\x3b\x00\x00'),      #f1
    (curses.tigetstr('kf2').decode("utf-8"),  '\x00\x3c\x00\x00'),
    (curses.tigetstr('kf3').decode("utf-8"),  '\x00\x3d\x00\x00'),
    (curses.tigetstr('kf4').decode("utf-8"),  '\x00\x3e\x00\x00'),
    (curses.tigetstr('kf5').decode("utf-8"),  '\x00\x3f\x00\x00'),
    (curses.tigetstr('kf6').decode("utf-8"),  '\x00\x40\x00\x00'),
    (curses.tigetstr('kf7').decode("utf-8"),  '\x00\x41\x00\x00'),
    (curses.tigetstr('kf8').decode("utf-8"),  '\x00\x42\x00\x00'),
    (curses.tigetstr('kf9').decode("utf-8"),  '\x00\x43\x00\x00'),
    (curses.tigetstr('kf10').decode("utf-8"),  '\x00\x44\x00\x00'),     #f10
    # Arrow keys
    (curses.tigetstr('kcuu1').decode("utf-8"),  '\x00\x48\x00\x00'),    #up
    (curses.tigetstr('kcud1').decode("utf-8"),  '\x00\x50\x00\x00'),    #down
    (curses.tigetstr('kcub1').decode("utf-8"),  '\x00\x4b\x00\x00'),    #left
    (curses.tigetstr('kcuf1').decode("utf-8"),  '\x00\x4d\x00\x00'),    #right

    (curses.tigetstr('cuu1').decode("utf-8"),  '\x00\x48\x00\x00'),    #up
    (curses.tigetstr('cud1').decode("utf-8"),  '\x00\x50\x00\x00'),    #down
    (curses.tigetstr('cub1').decode("utf-8"),  '\x00\x4b\x00\x00'),    #left
    (curses.tigetstr('cuf1').decode("utf-8"),  '\x00\x4d\x00\x00'),    #right

    #TODO: remaining keys
}

atari_to_local_terminal = {
    ('\33p', inverse_seq),   # inverse mode on
    ('\33q', normal_seq),  # normal mode on/inverse off
    ('\33J', curses.tigetstr('ed').decode("utf-8")),    # clear to end of screen
    ('\33K', curses.tigetstr('el').decode("utf-8")),    # clear to end of line
    ('\33l', curses.tigetstr('el1').decode("utf-8") + curses.tigetstr('el').decode("utf-8")),    # clear current line
    ('\33o', curses.tigetstr('el1').decode("utf-8")),    # clear to start of line
    ('\33d', curses.tigetstr('clear').decode("utf-8")),    # clear screen up to cursor
    ('\33e', curses.tigetstr('cnorm').decode("utf-8")), # cursor on
    ('\33f', curses.tigetstr('civis').decode("utf-8")), # cursor off
    ('\33w', curses.tigetstr('rmam').decode("utf-8")), # wrap off
    ('\33v', curses.tigetstr('smam').decode("utf-8")), # wrap on

    ('\33H', curses.tigetstr('home').decode("utf-8")), # move home
    ('\33B', curses.tigetstr('cuu1').decode("utf-8")), # move up
    ('\33D', curses.tigetstr('cub1').decode("utf-8")), # move left
    ('\33C', curses.tigetstr('cuf1').decode("utf-8")), # move right
    ('\33A', curses.tigetstr('cud1').decode("utf-8")), # move down

    ('\33M', curses.tigetstr('dl1').decode("utf-8")), # del line

    ('\33k', curses.tigetstr('rc').decode("utf-8")), # restore cursor pos
    ('\33j', curses.tigetstr('sc').decode("utf-8")), # save cursor pos

    # TODO: add remaining sequences
    ('\33Y', ""), # set cursor pos
    ('\33b', ""), # set fb col
    ('\33c', ""), # set bg col
}

def translate_and_print(data):
    for seq in atari_to_local_terminal:
        data = data.replace(seq[0], seq[1])
    sys.stderr.write(data)

def main(args):

    remote_host = args[1].split('/', 1)[0]
    remote_path = "/" + args[1].split('/', 1)[1]

    remote_ip = socket.gethostbyname(remote_host)
    print("IP: " + remote_ip)

    rec_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.getprotobyname('udp'))
    rec_sock.bind(("", REC_UDP_PORT))
    rec_sock.setblocking(0)
    rec_sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
    rec_sock.setsockopt(socket.SOL_IP, socket.IP_TTL, 100)
    rec_sock.connect((remote_ip, SEND_UDP_PORT))

    conn = httplib.HTTPConnection(remote_ip)
    conn.connect()
    print ("running: " + remote_path)
    conn.request("GET", remote_path + "?run=" + " ".join(args[2:]))
    response = conn.getresponse()
    response.read()
    if response.status != 200:
        print "Error (HTTP code: " + str(response.status) + ")"
        return 1

    print(inverse_seq + "Remote output:" + normal_seq)

    with NonBlockingConsole() as nbc:
        try:
            c = ""
            while 1:
                try:
                    data, addr = rec_sock.recvfrom(1500)
                    if len(data) == 1 and data[0] == '\0':
                        return
                    translate_and_print(data)
                except KeyboardInterrupt:
                    pass
                except:
                    pass
                c = c + nbc.get_data()
                if len(c) > 0 and c != False:
                    if c == '[CTRL-C]':
                        # send spece key just in case remote app would exit on that
                        # TODO: send a sequence which remove terminal redirection
                        rec_sock.send(" ")
                        return
                    elif c[0] != '\x1b' and c[0] != '\x7f':
                        code = '\0\0\0' + c[0]
                        rec_sock.send(code)
                        c = ""
                    else:
                        for s in local_to_atari_terminal:
                            if c == s[0]:
                                c = ""
                                rec_sock.send(s[1])
                        if len(c) > 4 and c[0] == '\x1b':
                            c = ""
        except KeyboardInterrupt:
            rec_sock.send(" ")
            return

def print_usage():
    print("Usage:")
    print(sys.argv[0] + " remote_executable [args]")
    sys.exit(1)

if __name__ == "__main__":
    try:
        if len(sys.argv) < 2:
            print_usage()
        main(sys.argv)
        print(inverse_seq + "Program terminated.." + normal_seq)
    except:
        pass
