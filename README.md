pipestats
=========

Ever wondered about the progress of something involving a pipe, where neither side reported on it? Like loading a mongo dump from a file? Put this in between and it'll tell you how much has moved and how fast it's moving!

```bash
# Watch log stats:
tail -F /var/log/mongo/mongodb.log | pipestats > /dev/null
```

You can use it to test the speed of devices and apps:

```bash

# Get a baseline with /dev/zero & /dev/null
pipestats < /dev/zero > /dev/null

# Compare that to the speed of random number generation
pipestats < /dev/urandom > /dev/null

# Or to writing to your disk
pipestats < /dev/zero > zero.0

# Measure an app, like gzip
pipestats < /dev/urandom | gzip > /dev/null
```
