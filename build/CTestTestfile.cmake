# CMake generated Testfile for 
# Source directory: /home/radu/Downloads/CodexBar/Linux_v2
# Build directory: /home/radu/Downloads/CodexBar/Linux_v2/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(appstreamtest "/usr/bin/cmake" "-DAPPSTREAMCLI=/usr/bin/appstreamcli" "-DINSTALL_FILES=/home/radu/Downloads/CodexBar/Linux_v2/build/install_manifest.txt" "-P" "/usr/share/ECM/kde-modules/appstreamtest.cmake")
set_tests_properties(appstreamtest PROPERTIES  _BACKTRACE_TRIPLES "/usr/share/ECM/kde-modules/KDECMakeSettings.cmake;173;add_test;/usr/share/ECM/kde-modules/KDECMakeSettings.cmake;191;appstreamtest;/usr/share/ECM/kde-modules/KDECMakeSettings.cmake;0;;/home/radu/Downloads/CodexBar/Linux_v2/CMakeLists.txt;20;include;/home/radu/Downloads/CodexBar/Linux_v2/CMakeLists.txt;0;")
subdirs("src/core")
subdirs("src/app")
