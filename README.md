pipestats
=========

Ever wondered about the progress of something involving a pipe, where neither side reported on it? Like loading a mongo dump from a file? Put this in between and it'll tell you how much has moved and how fast it's moving!

```
$ cat a_file | pipestats | gzip > a_smaller_file.tgz
```
