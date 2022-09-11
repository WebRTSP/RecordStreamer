#!/bin/bash -e

wsdl2h -c++11 -d -p -O4 -o onvif.h \
    http://www.onvif.org/ver10/device/wsdl/devicemgmt.wsdl \
    https://www.onvif.org/ver10/media/wsdl/media.wsdl \
    http://www.onvif.org/ver10/events/wsdl/event.wsdl

# -c++11 Generate C++ source code optimized for C++11 (compile with -std=c++11).
# -2     Generate SOAP 1.2 source code.
# -C     Generate client-side source code only.
# -j     Generate C++ service proxies and objects that share a soap struct.
# -x     Do not generate sample XML message files.
# -t     Generate source code for fully xsi:type typed SOAP/XML messages.
soapcpp2 -c++11 -2 -C -j -x -t onvif.h
