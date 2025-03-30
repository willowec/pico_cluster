

# Build:
1. Install the rpi pico sdk somewhere on your computer: [pico-sdk](https://github.com/raspberrypi/pico-sdk)
2. Enter a linux environment (WSL)
3. 
        mkdir build
        cd build
        export PICO_SDK_PATH=../relative/path/to/the/cloned/directory/named/pico-sdk
        cmake ..
        make

4. Copy the uf2 file of the build target you want to program the pico with to the pico's flash to program it.
