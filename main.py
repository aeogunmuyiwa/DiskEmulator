import sys
import os

file_name = 'temp_file'
specific_file_name = False
required_size = 0

if len(sys.argv) == 2:
    required_size = int(sys.argv[1])
elif len(sys.argv) == 3:
    file_name = sys.argv[1]
    required_size = int(sys.argv[2])
    specific_file_name = True
else:
    print("usage:\npython ./main.py <filename> <filesize>\npython ./main.py <filesize>")
    exit()

open(file_name, 'w').close()
i = 0
current_size = 0
while current_size < required_size:
    file = open(file_name, 'a')
    file.writelines(str(i) + " ")
    file.close()
    current_size = os.stat(file_name).st_size
    i += 1

if not specific_file_name:
    new_file_name = 'file' + str(current_size)
    os.rename(file_name, new_file_name)
    file_name = new_file_name
    

print("created file \'{}\' with {} bytes".format(file_name, current_size))