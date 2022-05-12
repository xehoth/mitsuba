FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
WORKDIR /home/mitsuba
COPY . /home/mitsuba

RUN apt update -y && apt upgrade -y && apt install -y build-essential apt-utils python3 python3-pip scons cmake libglewmx-dev libxxf86vm-dev libpcrecpp0v5 libglu1-mesa-dev freeglut3-dev mesa-common-dev git vim curl qt5-default libqt5xmlpatterns5-dev \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Concurrent.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtConcurrent.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Core.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtCore.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Gui.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtGui.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Network.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtNetwork.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5OpenGL.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtOpenGL.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5OpenGLExtensions.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtOpenGLExtensions.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5PrintSupport.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtPrintSupport.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Sql.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtSql.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Test.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtTest.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Widgets.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtWidgets.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5Xml.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtXml.pc \
    && ln -s /usr/lib/x86_64-linux-gnu/pkgconfig/Qt5XmlPatterns.pc /usr/lib/x86_64-linux-gnu/pkgconfig/QtXmlPatterns.pc \
    && cd /home/ \
    && pip install scons-compiledb conan \
    && conan profile new default --detect \
    && conan profile update settings.compiler.libcxx=libstdc++11 default \
    && apt clean \
    && cd /home/mitsuba \
    && cp build/config-linux-gcc.py config.py \
    && echo 'source /home/mitsuba/setpath.sh' >> ~/.bashrc \
    && cd /home/mitsuba/build \
    && conan install .. --build=missing \
    && cd /home/mitsuba/ \
    && scons -j 8

