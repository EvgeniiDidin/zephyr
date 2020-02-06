.. _smp_pkgqueue:

SMP pkgqueue
###########

Overview
********

This sample application performs a simplified network layer forwarding function
(essentially checksum calculation from IP Header Validation) of the Internet protocol
suite specified in RFC1812 "Requirements for IP Version 4 Routers" which
can be found at http://www.faqs.org/rfcs/rfc1812.html.  The benchmark
provides an indication of the potential performance of a microprocessor in an
IP router system.

At the beginning of the application the array (size defined in SIZE_OF_QUEUE)
of package headers is initialized. Each header contains some random data of size
defined in SIZE_OF_HEADER and calculated crc16 in appropriate field. The contents of
header follows:
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |Version|  IHL  |Type of Service|          Total Length         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         Identification        |Flags|      Fragment Offset    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Time to Live |    Protocol   |         Header Checksum       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                       Source Address                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Destination Address                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Options                    |    Padding    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

The headers then are stored in "sender" queue.

After that several(defined in THREADS_NUM) threads are created and each
of them first pick the header from "sender" queue, calculates crc and if
crc is correct put the header to "receiver" queue. Only one thread in a
time can access to sender or receiver queue.

By changing the value of CONFIG_MP_NUM_CPUS on SMP systems, you
can see that using more cores takes almost linearly less time
to complete the computational task.

You can also edit the sample source code to change the
number of digits calculated (``DIGITS_NUM``), and the
number of threads to use (``THREADS_NUM``).

Building and Running
********************

This project outputs Pi values calculated by each thread and in the end total time
required for all the calculation to be done. It can be built and executed
on Synopsys ARC HSDK board as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/smp_pi
   :host-os: unix
   :board: qemu_x86_64
   :goals: run
   :compact:

Sample Output
=============

.. code-block:: console

    Calculate first 240 digits of Pi independently by 16 threads.
    Pi value calculated by thread #0: 3141592653589793238462643383279502884197...
    Pi value calculated by thread #1: 3141592653589793238462643383279502884197...
    ...
    Pi value calculated by thread #14: 314159265358979323846264338327950288419...
    Pi value calculated by thread #15: 314159265358979323846264338327950288419...
    All 16 threads executed by 4 cores in 28 msec
