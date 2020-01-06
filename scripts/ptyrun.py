from ptyprocess import PtyProcessUnicode
from sys import argv, stdout

pp = PtyProcessUnicode.spawn(argv[1:])
while True:
    line = pp.readline()
    stdout.write(line)
