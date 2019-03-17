#!/usr/bin/env python
import httplib
import sys
import hashlib
import os
import zlib
import pickle
import time

def print_usage():
    print("Usage:")
    print(sys.argv[0] + " local_dir remote_dir")

class http_io:
    def __init__(self, remote_host, remote_dir):
        self.remote_dir = remote_dir
        self.conn = httplib.HTTPConnection(remote_host)
        self.conn.connect()

    def get_file(self, file_name):
        remote_path = self.remote_dir + file_name
        print ("get: " + remote_path)
        self.conn.request("GET", remote_path)
        response = self.conn.getresponse()
        data = response.read()
        return data

    def put_file(self, file_name, data):
        remote_path = self.remote_dir + file_name
        print ("put: " + remote_path)
        self.conn.request("PUT", remote_path, data)
        response = self.conn.getresponse()
        response.read()
        if response.status != 201:
            print "Error (HTTP code: " + str(response.status) + ")"
            sys.exit(1)

    def del_file(self, file_name):
        remote_path = self.remote_dir + file_name
        print ("del: " + remote_path)
        self.conn.request("DELETE", remote_path)
        response = self.conn.getresponse()
        response.read()
        if response.status != 200:
            print "Error (HTTP code: " + str(response.status) + ")"
            sys.exit(1)

    def upload_files(self, file_list):
        for file in file_list:
            with open(file[0], "rb") as f:
                self.put_file(file[1], f)

    def delete_files(self, file_list):
        for file in file_list:
            self.del_file(file[1])

def get_remote_file_list(connection):
    list_gz = connection.get_file("md5sum.gz")
    if len(list_gz):
        list_bytes = zlib.decompress(list_gz, zlib.MAX_WBITS|32)
        return pickle.loads(list_bytes)
    return []

def send_remote_file_list(connection, files):
    json_list = pickle.dumps(files)
    gzip_compress = zlib.compressobj(9, zlib.DEFLATED, zlib.MAX_WBITS | 16)
    json_gz = gzip_compress.compress(json_list) + gzip_compress.flush()
    connection.put_file("md5sum.gz", json_gz)

def md5sum(filename, blocksize=65536):
    hash = hashlib.md5()
    with open(filename, "rb") as f:
        for block in iter(lambda: f.read(blocksize), b""):
            hash.update(block)
    return hash.hexdigest()

def get_local_files(path):
    local_files = []
    full_dir = os.path.abspath(path)
    for r, d, f in os.walk(path):
        for file in f:
            full_local_path = os.path.abspath(os.path.join(r, file))
            sub_path = full_local_path.replace(full_dir,"").replace('/','',1)
            local_files.append((full_local_path, sub_path, md5sum(full_local_path)))
    def key_func(s):
        return s[0]
    local_files.sort(key=key_func)
    return local_files

def get_changes(remote_list, local_list):
    # create ne w lists without checkums to find orphaned files
    stripped_remote_list = [(e[0], e[1], 0) for e in remote_list]
    stripped_local_list = [(e[0], e[1], 0) for e in local_list]
    delete_files = list(set(stripped_remote_list) - set(stripped_local_list))
    upload_files = list(set(local_list) - set(remote_list))
    return upload_files, delete_files

def main(args):
    if len(args) != 3:
        print_usage();
        return 1
    # Inputs
    local_dir = args[1]
    remote = args[2]
    remote_host = remote.split('/', 1)[0]
    remote_dir = '/' + remote.split('/', 1)[1]
    # Grab local file list
    local_files = get_local_files(local_dir)
    # Connect
    connection = http_io(remote_host, remote_dir)
    remote_files = get_remote_file_list(connection)
    # Calculate changes
    upload_list, delete_list = get_changes(remote_files, local_files)
    # Execute remote requests
    if upload_list or delete_list:
        connection.delete_files(delete_list)
        connection.upload_files(upload_list)
        send_remote_file_list(connection, local_files)
    else:
        print("All up-to-date.")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
