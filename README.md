[![Build Status](https://travis-ci.org/NWilson/javaInterruptHook.svg?branch=master)](https://travis-ci.org/NWilson/javaInterruptHook)

# How to write native Java methods which respond to `Thread.interrupt()`

This is a tutorial project, demonstrating how to hook a Java thread's `interrupt()`
method to cancel a long-running native method.

## Problem

For all the Java methods which block, such as `Thread.sleep()` or `Object.wait()`, it's possible
to interrupt the action by calling `Thread.interrupt()`.

What happens if you implement your own blocking procedure? Well, this isn't possible in Java
because you don't get much in the way of blocking calls, you have to build on top of one of the
existing mechanisms. But, if you're providing Java bindings for some native code which does its
own I/O, then it becomes a concern.

In particular, the question really is, "how do I receive a notification when `Thread.interrupt()`
is called, so I can wake up my native code?"

The solution uses a class which unfortunately has rather few hits on Google, hence this
writeup: `java.nio.channels.spi.AbstractSelector`.

## How do you get a notification from `Thread.interrupt()`?

Inside the Sun Java (and OpenJDK), there is a hook whereby an object can register itself as the thread's current
blocking operation, but the method is private so there's no way to get at it directly.

Using the `sun.misc.SharedSecrets` class lets other parts of the JVM get access to the notification
from the `Thread`. If you're happy to grub around itside the semi-private JRE implementation,
you can call `JavaLangAccess.blockedOn()` to get what you want.

As far as I can tell, there's only one way to get public access to that functionality, as of Java
1.7: using NIO selectors.

The `java.nio.channels.Selector` interface is probably something that Sun didn't expect many people to to
have to implement themselves, and on its own it's not very useful, but thankfully they created
a partial implementation of it in the form of `AbstractSelector` which does the cheeky hooking
using `SharedSecrets` for you.

## This sample code

This sample is tutorial/example code which shows how to extend `AbstractSelector` appropriately.
I couldn't find any other examples of this, so I hope I'm doing it the right way!

## Building

### Windows

    mkdir bld; cd bld
    cmake -G "Visual Studio 12 2013 Win64" .. # Or whichever version of VS you have
    # Build using VS...
    sh java.sh

### Mac/Linux

    mkdir bld; cd bld
    cmake ..
    make
    sh java.sh
