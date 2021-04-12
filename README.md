# DirectShow FlipCam

This DirectShow filter is a virtual webcam that suports flipping video input horizontaly or verticaly. Originaly based from Viveks [VCam](https://github.com/roman380/tmhare.mvps.org-vcam).


## How to use

Compile your own FlipCam.dll filters or **download** them [here](https://github.com/Lenart12/FlipCam/releases/).

Register dll using the command, for **both x86 and x64**
```
regsvr32 "$(SolutionDir)/bin/$(Platform)/$(Configuration)/FlipCam.dll"
```
and unregister with
```
regsvr32 /u "$(SolutionDir)/bin/$(Platform)/$(Configuration)/FlipCam.dll"
```

Configuration file is made when running the `regsvr32` command with the `/i:"configuration string"` option. Or edit it in `%appdata%/FlipCam/flipcam.cfg`

Possible configurations:

|Key|Value|Description|Default|
-|-|-|-
|res|WxHxFPS|Sets output resolution and framerate, can also be just WxH|1280x720x24
|device|Friendly name|Sets the input device based on the device friendly name|not set|
|vflip||Vertical flip|not set|
|hflip||Horizontal flip|not set|
|debug||Show debug information|not set|
|dvd||Floating DVD icon bouncing around|not set|

Multiple settings are seperated with `,` and settings that require a value are in format of `key:value`

Example configuration string is 
```
res:1920x1080x60,device:GENERAL WEBCAM,hflip
```

## or

Use Widows 10 SDK GraphEdit or [GraphStudioNext](https://github.com/cplussharp/graph-studio-next).

Locate "FlipCam" filter:

![](README-01.png)

Build a pipeline and run:

![](README-02.png)

## Further information

The VCam project has been discussed a lot in various forums. Most of them are archived and there are no active disussions, however there is still a lot of information online. Just a few pointers:

- [https://social.msdn.microsoft.com/Forums/...](https://social.msdn.microsoft.com/Forums/en-US/home?category=&forum=&filter=&sort=relevancedesc&brandIgnore=true&searchTerm=VCam)
- [https://groups.google.com/g/microsoft.public.win32.programmer.directx.video/...](https://groups.google.com/g/microsoft.public.win32.programmer.directx.video/search?q=VCam)
- [https://stackoverflow.com/...](https://stackoverflow.com/search?q=VCam)
