pipestats
=========

Ever wondered about the progress of something involving a pipe, where neither
side reported on it? Like loading a mongo dump from a file? Put this in between
and it'll tell you how much has moved and how fast it's moving! Or watch a log:


```bash
$ tail -F /var/log/mongo/mongodb.log | pipestats > /dev/null
```


You can use it to test the speed of devices and apps. Get a baseline
with `/dev/zero` & `/dev/null`, two really fast devices:

```bash
$ pipestats < /dev/zero > /dev/null
1.09 G/s, 2.19 G total, 2.19 G since last report, 7.15 secs until 10.00 G
1.07 G/s, 4.33 G total, 2.14 G since last report, 5.30 secs until 10.00 G
1.04 G/s, 6.41 G total, 2.08 G since last report, 3.45 secs until 10.00 G
^C
Got signal Interrupt: 2, aborting early.
6.74 G (7236808392 bytes) total over 6.30 sec, avg 1.07 G/s
```


Then compare that to `/dev/random`, for example:

```bash
$ pipestats < /dev/urandom > /dev/null
10.46 M/s, 21.00 M total, 21.00 M since last report, 2.77 secs until 50.00 M
10.25 M/s, 41.56 M total, 20.56 M since last report, 0.82 secs until 50.00 M
10.46 M/s, 62.50 M total, 20.94 M since last report, 41.84 secs until 500.00 M
^C
Got signal Interrupt: 2, aborting early.
67.13 M (70386492 bytes) total over 6.50 sec, avg 10.33 M/s
```


Or to writing to your disk:

```bash
$ pipestats < /dev/zero > zero.0
268.39 M/s, 536.78 M total, 536.78 M since last report, 1.82 secs until 1.00 G
259.47 M/s, 1.03 G total, 518.94 M since last report, 35.40 secs until 10.00 G
209.37 M/s, 1.44 G total, 419.29 M since last report, 41.86 secs until 10.00 G
^C
Got signal Interrupt: 2, aborting early.
1.52 G (1628763312 bytes) total over 6.39 sec, avg 0.24 G/s
```


Measure an app, like compression:

```bash
# Compressing random data with its best attempt:
$ pipestats < /dev/urandom | bzip2 --best > /dev/null
2.96 M/s, 6.08 M total, 6.08 M since last report, 1.32 secs until 10.00 M
2.93 M/s, 12.10 M total, 6.02 M since last report, 12.94 secs until 50.00 M
3.35 M/s, 18.81 M total, 6.71 M since last report, 9.31 secs until 50.00 M
^C
Got signal Interrupt: 2, aborting early.
Failed to flush stdout, err 32: Broken pipe
19.82 M (20779176 bytes) total over 6.57 sec, avg 3.02 M/s

# Compressing random data with a fast attempt:
$ pipestats < /dev/urandom | bzip2 --fast > /dev/null
3.50 M/s, 7.04 M total, 7.04 M since last report, 0.85 secs until 10.00 M
3.67 M/s, 14.38 M total, 7.34 M since last report, 9.71 secs until 50.00 M
3.41 M/s, 21.24 M total, 6.86 M since last report, 8.43 secs until 50.00 M
^C
Got signal Interrupt: 2, aborting early.
Got err 32 during a write: Broken pipe
Exiting with 4092 bytes still in buffer.
22.50 M (23594472 bytes) total over 6.39 sec, avg 3.52 M/s

# Compressing the same 0 byte stream:
$ pipestats < /dev/zero | bzip2 --fast > /dev/null
72.26 M/s, 145.97 M total, 145.97 M since last report, 4.90 secs until 500.00 M
71.48 M/s, 288.94 M total, 142.97 M since last report, 2.95 secs until 500.00 M
71.53 M/s, 432.88 M total, 143.94 M since last report, 0.94 secs until 500.00 M
^C
Got signal Interrupt: 2, aborting early.
455.82 M (477961968 bytes) total over 6.37 sec, avg 71.53 M/s
```


You can also get a count of each byte value:

```bash
$ pipestats --counts < pipestats.c > /dev/null
Count of byte values:
     0x00:   0.00B  0.00%     0x01:   0.00B  0.00%     0x02:   0.00B  0.00%     0x03:   0.00B  0.00%
     0x04:   0.00B  0.00%     0x05:   0.00B  0.00%     0x06:   0.00B  0.00%     0x07:   0.00B  0.00%
     0x08:   0.00B  0.00%     0x09:   0.00B  0.00%     0x0A: 457.00B  3.44%     0x0B:   0.00B  0.00%
     0x0C:   0.00B  0.00%     0x0D:   0.00B  0.00%     0x0E:   0.00B  0.00%     0x0F:   0.00B  0.00%
     0x10:   0.00B  0.00%     0x11:   0.00B  0.00%     0x12:   0.00B  0.00%     0x13:   0.00B  0.00%
     0x14:   0.00B  0.00%     0x15:   0.00B  0.00%     0x16:   0.00B  0.00%     0x17:   0.00B  0.00%
     0x18:   0.00B  0.00%     0x19:   0.00B  0.00%     0x1A:   0.00B  0.00%     0x1B:   0.00B  0.00%
     0x1C:   0.00B  0.00%     0x1D:   0.00B  0.00%     0x1E:   0.00B  0.00%     0x1F:   0.00B  0.00%
     0x20:   4.47K 34.43%   ! 0x21:  13.00B  0.10%   " 0x22:  82.00B  0.62%   # 0x23:  16.00B  0.12%
   $ 0x24:   0.00B  0.00%   % 0x25:  39.00B  0.29%   & 0x26:  36.00B  0.27%   ' 0x27:  43.00B  0.32%
   ( 0x28: 137.00B  1.03%   ) 0x29: 137.00B  1.03%   * 0x2A:  25.00B  0.19%   + 0x2B:  11.00B  0.08%
   , 0x2C: 161.00B  1.21%   - 0x2D:  60.00B  0.45%   . 0x2E:  94.00B  0.71%   / 0x2F:  61.00B  0.46%
   0 0x30:  67.00B  0.50%   1 0x31:  23.00B  0.17%   2 0x32:  17.00B  0.13%   3 0x33:   7.00B  0.05%
   4 0x34:   2.00B  0.02%   5 0x35:   5.00B  0.04%   6 0x36:   3.00B  0.02%   7 0x37:   0.00B  0.00%
   8 0x38:   0.00B  0.00%   9 0x39:   1.00B  0.01%   : 0x3A:  45.00B  0.34%   ; 0x3B: 168.00B  1.26%
   < 0x3C:  17.00B  0.13%   = 0x3D:  98.00B  0.74%   > 0x3E:  49.00B  0.37%   ? 0x3F:   4.00B  0.03%
   @ 0x40:   0.00B  0.00%   A 0x41:  13.00B  0.10%   B 0x42:  18.00B  0.14%   C 0x43:   5.00B  0.04%
   D 0x44:  17.00B  0.13%   E 0x45:  42.00B  0.32%   F 0x46:  19.00B  0.14%   G 0x47:  18.00B  0.14%
   H 0x48:   8.00B  0.06%   I 0x49:  29.00B  0.22%   J 0x4A:   1.00B  0.01%   K 0x4B:  10.00B  0.08%
   L 0x4C:  39.00B  0.29%   M 0x4D:   7.00B  0.05%   N 0x4E:  32.00B  0.24%   O 0x4F:  21.00B  0.16%
   P 0x50:   4.00B  0.03%   Q 0x51:   1.00B  0.01%   R 0x52:  16.00B  0.12%   S 0x53:  42.00B  0.32%
   T 0x54:  30.00B  0.23%   U 0x55:  28.00B  0.21%   V 0x56:   0.00B  0.00%   W 0x57:   2.00B  0.02%
   X 0x58:   4.00B  0.03%   Y 0x59:   6.00B  0.05%   Z 0x5A:   7.00B  0.05%   [ 0x5B:  12.00B  0.09%
   \ 0x5C:  27.00B  0.20%   ] 0x5D:  12.00B  0.09%   ^ 0x5E:   0.00B  0.00%   _ 0x5F: 192.00B  1.44%
   ` 0x60:   0.00B  0.00%   a 0x61: 423.00B  3.18%   b 0x62: 134.00B  1.01%   c 0x63: 196.00B  1.47%
   d 0x64: 218.00B  1.64%   e 0x65: 630.00B  4.74%   f 0x66: 183.00B  1.38%   g 0x67:  98.00B  0.74%
   h 0x68:  80.00B  0.60%   i 0x69: 458.00B  3.45%   j 0x6A:   9.00B  0.07%   k 0x6B:  37.00B  0.28%
   l 0x6C: 206.00B  1.55%   m 0x6D: 109.00B  0.82%   n 0x6E: 513.00B  3.86%   o 0x6F: 408.00B  3.07%
   p 0x70: 156.00B  1.17%   q 0x71:  17.00B  0.13%   r 0x72: 443.00B  3.33%   s 0x73: 512.00B  3.85%
   t 0x74: 810.00B  6.09%   u 0x75: 249.00B  1.87%   v 0x76:  51.00B  0.38%   w 0x77:  39.00B  0.29%
   x 0x78:   5.00B  0.04%   y 0x79:  78.00B  0.59%   z 0x7A:   7.00B  0.05%   { 0x7B:  49.00B  0.37%
   | 0x7C:   9.00B  0.07%   } 0x7D:  49.00B  0.37%   ~ 0x7E:   0.00B  0.00%     0x7F:   0.00B  0.00%
     0x80:   0.00B  0.00%     0x81:   0.00B  0.00%     0x82:   0.00B  0.00%     0x83:   0.00B  0.00%
     0x84:   0.00B  0.00%     0x85:   0.00B  0.00%     0x86:   0.00B  0.00%     0x87:   0.00B  0.00%
     0x88:   0.00B  0.00%     0x89:   0.00B  0.00%     0x8A:   0.00B  0.00%     0x8B:   0.00B  0.00%
     0x8C:   0.00B  0.00%     0x8D:   0.00B  0.00%     0x8E:   0.00B  0.00%     0x8F:   0.00B  0.00%
     0x90:   0.00B  0.00%     0x91:   0.00B  0.00%     0x92:   0.00B  0.00%     0x93:   0.00B  0.00%
     0x94:   0.00B  0.00%     0x95:   0.00B  0.00%     0x96:   0.00B  0.00%     0x97:   0.00B  0.00%
     0x98:   0.00B  0.00%     0x99:   0.00B  0.00%     0x9A:   0.00B  0.00%     0x9B:   0.00B  0.00%
     0x9C:   0.00B  0.00%     0x9D:   0.00B  0.00%     0x9E:   0.00B  0.00%     0x9F:   0.00B  0.00%
     0xA0:   0.00B  0.00%     0xA1:   0.00B  0.00%     0xA2:   0.00B  0.00%     0xA3:   0.00B  0.00%
     0xA4:   0.00B  0.00%     0xA5:   0.00B  0.00%     0xA6:   0.00B  0.00%     0xA7:   0.00B  0.00%
     0xA8:   0.00B  0.00%     0xA9:   0.00B  0.00%     0xAA:   0.00B  0.00%     0xAB:   0.00B  0.00%
     0xAC:   0.00B  0.00%     0xAD:   0.00B  0.00%     0xAE:   0.00B  0.00%     0xAF:   0.00B  0.00%
     0xB0:   0.00B  0.00%     0xB1:   0.00B  0.00%     0xB2:   0.00B  0.00%     0xB3:   0.00B  0.00%
     0xB4:   0.00B  0.00%     0xB5:   0.00B  0.00%     0xB6:   0.00B  0.00%     0xB7:   0.00B  0.00%
     0xB8:   0.00B  0.00%     0xB9:   0.00B  0.00%     0xBA:   0.00B  0.00%     0xBB:   0.00B  0.00%
     0xBC:   0.00B  0.00%     0xBD:   0.00B  0.00%     0xBE:   0.00B  0.00%     0xBF:   0.00B  0.00%
     0xC0:   0.00B  0.00%     0xC1:   0.00B  0.00%     0xC2:   0.00B  0.00%     0xC3:   0.00B  0.00%
     0xC4:   0.00B  0.00%     0xC5:   0.00B  0.00%     0xC6:   0.00B  0.00%     0xC7:   0.00B  0.00%
     0xC8:   0.00B  0.00%     0xC9:   0.00B  0.00%     0xCA:   0.00B  0.00%     0xCB:   0.00B  0.00%
     0xCC:   0.00B  0.00%     0xCD:   0.00B  0.00%     0xCE:   0.00B  0.00%     0xCF:   0.00B  0.00%
     0xD0:   0.00B  0.00%     0xD1:   0.00B  0.00%     0xD2:   0.00B  0.00%     0xD3:   0.00B  0.00%
     0xD4:   0.00B  0.00%     0xD5:   0.00B  0.00%     0xD6:   0.00B  0.00%     0xD7:   0.00B  0.00%
     0xD8:   0.00B  0.00%     0xD9:   0.00B  0.00%     0xDA:   0.00B  0.00%     0xDB:   0.00B  0.00%
     0xDC:   0.00B  0.00%     0xDD:   0.00B  0.00%     0xDE:   0.00B  0.00%     0xDF:   0.00B  0.00%
     0xE0:   0.00B  0.00%     0xE1:   0.00B  0.00%     0xE2:   0.00B  0.00%     0xE3:   0.00B  0.00%
     0xE4:   0.00B  0.00%     0xE5:   0.00B  0.00%     0xE6:   0.00B  0.00%     0xE7:   0.00B  0.00%
     0xE8:   0.00B  0.00%     0xE9:   0.00B  0.00%     0xEA:   0.00B  0.00%     0xEB:   0.00B  0.00%
     0xEC:   0.00B  0.00%     0xED:   0.00B  0.00%     0xEE:   0.00B  0.00%     0xEF:   0.00B  0.00%
     0xF0:   0.00B  0.00%     0xF1:   0.00B  0.00%     0xF2:   0.00B  0.00%     0xF3:   0.00B  0.00%
     0xF4:   0.00B  0.00%     0xF5:   0.00B  0.00%     0xF6:   0.00B  0.00%     0xF7:   0.00B  0.00%
     0xF8:   0.00B  0.00%     0xF9:   0.00B  0.00%     0xFA:   0.00B  0.00%     0xFB:   0.00B  0.00%
     0xFC:   0.00B  0.00%     0xFD:   0.00B  0.00%     0xFE:   0.00B  0.00%     0xFF:   0.00B  0.00%
12.98 K (13292 bytes) total over 0.01 sec, avg 1591.33 K/s
```
