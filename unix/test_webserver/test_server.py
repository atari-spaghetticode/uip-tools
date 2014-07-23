#!/usr/bin/env python
import SimpleHTTPServer
import SocketServer

class MyRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.path = '/index.html'
        elif self.path == '/?dir':
            self.path = 'test_webserver/dir.json'
        elif self.path == '/d/?dir':
            self.path = 'test_webserver/dir_d.json'
        elif self.path == '/c/?dir':
            self.path = 'test_webserver/dir_c.json'
        elif self.path == '/e/?dir':
            self.path = 'test_webserver/dir_e.json'
        elif self.path == '/f/?dir':
            self.path = 'test_webserver/dir_f.json'
        elif self.path.endswith('?dir'):
            self.path = 'test_webserver/dir_d.json'
        else:
            self.path = 'test_webserver/' + self.path 

        print "path:" + self.path

        return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

Handler = MyRequestHandler
server = SocketServer.TCPServer(('0.0.0.0', 8080), Handler)

server.serve_forever()