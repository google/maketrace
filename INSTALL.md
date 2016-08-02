## Compiling a static Qt

Download a Qt5 source release from http://download.qt.io/official_releases/qt/.

Extract it and enter the `qtbase` directory.

    ./configure \
      -force-debug-info \
      -opensource \
      -confirm-license \
      -static \
      -nomake examples \
      -qt-doubleconversion \
      -no-dbus \
      -no-eglfs \
      -no-eglfs \
      -no-evdev \
      -no-fontconfig \
      -no-freetype \
      -no-gif \
      -no-gstreamer \
      -no-gui \
      -no-iconv \
      -no-icu \
      -no-libjpeg \
      -no-libpng \
      -no-linuxfb \
      -no-opengl \
      -no-pulseaudio \
      -no-qml-debug \
      -no-widgets \
      -no-xcb
