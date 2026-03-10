OpenKJ Professional
======

Cross-platform open source karaoke show hosting software.

OpenKJ Professional is a modern continuation of the OpenKJ project. This project originally began as a fork of OpenKJ (https://github.com/OpenKJ/OpenKJ.git) and has since been extensively refactored, optimized, and expanded with new features.

The codebase has been updated to utilize modern versions of key dependencies including GStreamer, Qt, TagLib, and other supporting libraries. Significant internal improvements have been made to improve performance, stability, and maintainability.

The user interface has also been refreshed and modernized. Several color themes have been added and can be selected from the settings window, allowing hosts to customize the look of the application. The interface has also been optimized to operate efficiently on mouse-free systems, providing a touch-friendly experience for touchscreen-based karaoke setups.

Future development for OpenKJ Professional includes several major additions such as:
* An integrated karaoke song store
* Spotify integration for break music playback
* Direct integration with the Singr Online Request Platform
* Additional hosting and automation tools designed for professional karaoke hosts

---

OpenKJ Professional is a full featured karaoke hosting program.

A few features:
* Save/track/load regular singers
* Key changer
* Tempo control
* EQ
* End of track silence detection (after last CDG draw command)
* Rotation ticker on the CDG display
* Option to use a custom background or display a rotating slide show on the CDG output dialog while idle
* Fades break music in and out automatically when karaoke tracks start/end
* Remote requests server integration allowing singers to look up and submit songs via the web
* Automatic performance recording
* Autoplay karaoke mode
* Lots of other little things

It currently handles media+g zip files (zip files containing an mp3, wav, or ogg file and a cdg file) and paired mp3 and cdg files. I'll be adding others in the future if anyone expresses interest. It also can play non-CDG based video files (mkv, mp4, mpg, avi) for both break music and karaoke.

Database entries for the songs are based on the file naming scheme. I've included the common ones I've come across which should cover 90% of what's out there. Custom patterns can also be defined in the program using regular expressions.
