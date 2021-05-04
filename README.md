# iPwnder32  
*A Tool for utilizing iOS devices using limera1n/checkm8  BootROM exploit*  

## Features  
This tool is intended to take advantage of the BootROM exploit present on iOS devices.  
It is written in the C and is only available on macosx.  

## Supported environments  
- macOS 10.13 or later (intel/x86_64)  
- macOS 11.0 - 11.2.3 (M1/arm64)  

## Library dependencies  
- [libcurl](https://github.com/curl/curl)  


## Make  
The `[option]` flag depends on which architecture you are building for:  
```
./BUILD [option]
  Usage: BUILD [option]
    --intel       Make for Intel Mac (x86_64)
    --M1          Make for M1 Mac (arm64)
    --universal   Make for both x86_64 and arm64
    --help        Show Usage
```
In order to build for arm64, you will need Xcode 12 or higher.  


## Usage  
```
./iPwnder32 [options]
    -p [flag]    Put device in pwned (soft)DFU mode
    [flag]
      --noibss:    Do not enter pwnediBSS (s5l895Xx only)
    -f <img>     Send image and boot it
    -t           Jailbreak iOS 10 with 32-bit devices [TBX32]
```

If you want to put your iOS device into pwned DFU mode, it will look like this:  
```
./iPwnder32 -p
```
For limera1n devices and s5l8960x, it is now possible to load unsigned images from pwned DFU mode.  

For s5l895Xx devices, the device is automatically loaded from DFU mode to pwned iBSS mode. it is now possible to load unsigned images from pwned iBSS mode.  
If you want to stop it in pwned DFU mode:  
```
./iPwnder32 -p --noibss
```

In this mode, your device will be waiting for the iBSS/LLB image. Next, you need to load the iBSS/LLB image into the device by sending the image using the `-f` flag:  
```
./iPwnder32 -f ibss
```
Note: The sent image will be loaded after the pwned DFU mode runs iBoot32Patcher to patch the RSA check.  


## License  
This project basically follows GPLv3. However, some of the currently private GPLv3 unused code in this project is not GPLv3 compliant.  


## Credits  
- [libimobiledevice](https://github.com/libimobiledevice) for [libirecovery](https://github.com/libimobiledevice/libirecovery)  
- [iH8sn0w](https://github.com/iH8sn0w) for [iBoot32Patcher](https://github.com/iH8sn0w/iBoot32Patcher)  
- [axi0mX](https://github.com/axi0mX) for [ipwndfu](https://github.com/axi0mX/ipwndfu)  
- [synackuk](https://github.com/synackuk) for [belladonna](https://github.com/synackuk/belladonna)  
- geohot for limera1n exploit  
- many creators of [iBoot32Patcher](https://github.com/dora2-iOS/iBoot32Patcher) fork  


README: updated on 2021/02/01  
