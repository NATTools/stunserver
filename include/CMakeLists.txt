install ( DIRECTORY ../include DESTINATION .
          PATTERN CMakeLists.txt EXCLUDE )
file(GLOB public_headers "*.h")
UncrustifyDir(public_headers)
