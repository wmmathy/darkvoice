# Source from Bash before cloud builds; no credentials are stored here.
export PATH="/workspace/.darkvoice-tools/cmake/data/bin:/workspace/.darkvoice-tools/bin:$PATH"
export PKG_CONFIG_SYSROOT_DIR=/workspace/sysroot
export PKG_CONFIG_PATH="/workspace/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/sysroot/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="/workspace/sysroot/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export LIBRARY_PATH="/workspace/sysroot/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
