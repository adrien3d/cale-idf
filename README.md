![CALE Logo](/config-examples/assets/cale-idf.svg)

This is the official IDF Firmware of our Web-Service [CALE.es](https://cale.es).

It does only 3 things at the moment and is very easy to set up:

1. It connects to cale.es and downloads a Screen bitmap.
2. In "Streaming mode" it pushes the pixels to Adafruit GFX buffer and at the end renders it in your Epaper.
3. It goes to sleep the amount of minutes you define in the ESP-IDF menuconfig

And of course wakes up after this deepsleep and goes back to point 1 making it an ideal Firmware if you want to refresh an Events calendar or weather Forecast display. It does need to be tied to our CALE service. You can use your own full url to your bitmap image. We just recommend to use CALE.es since you can easily connect it to external APIs and have a living epaper.

**CALE-IDF uses this components:**

- [CalEPD](https://github.com/martinberlin/CalEPD) the epaper component
- [Adafruit GFX for ESP-IDF](https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF) My own fork of Adafruit display library

They are at the moment included without git submodules so we an develop fast without updating them all the time. But they are also available to be used as your project ESP-IDF components.

## Configuration

Make sure to set the GPIOs that are connected from the Epaper to your ESP32. Data in in your epaper (DIN) should be connected to MOSI:
![CALE config](/config-examples/assets/menuconfig-display.png)

And then set the image configuration and deepsleep minutes. Here you can also set the rotation for your Eink display:
![Display config](/config-examples/assets/menuconfig-cale.png)

## CalEPD component

[CalEPD is an ESP-IDF component](https://github.com/martinberlin/CalEPD) to drive epaper displays with ESP32 / ESP32S2 and it's what is sending the graphics buffer to your epaper behind the scenes. It's designed to be a light C++ component to have a small memory footprint and run as fast as possible, leaving as much memory as possible for your Firmare. Note that still, the graphics library buffer, depending on your epaper size may need external PSRAM. Up to 800 * 480 pixels it runs stable and there is still free DRAM for more.

## Branches

**master**...    -> stable version

    v.0.9   Gdew0213i5f First testeable version with a 2.13" b/w epaper display Gdew0213i5f
    v.0.9.1 Gdew075T7   Added Waveshare/Good display 7.5" V2 800*480
    v.0.9.2 Wave12I48   Added Waveshare 12.48" multi epaper display (Note: Needs PSRAM if you want to make something useful)
    v.0.9.3 First official version with BMP download and render to the display (20.JUN.2020)

**refactor/oop** -> Making the components base, most actual branch, where new models are added. Only after successfull testing they will be merged in master. Inestable branch do not use on Firmware that you ship to a client.

tft_test         -> Original SPI master example from ESP-IDF 4 just refactored as a C++ class. Will be kept for historic reasons

The aim is to learn good how to code and link classes as git submodules in order to program the epaper display driver the same way. The goal is to have a tiny "human readable" code in cale.cpp main file and that the rest is encapsulated in classes.

### Epaper demos

Open the /main/CMakeLists.txt file to select what is the C++ file that will be compiled as your main program. Just uncomment one of the SRCS lines:

    idf_component_register(
      # Main CALE 
      #SRCS "cale.cpp"
       SRCS "demo-fonts.cpp"
      #SRCS "demo-epaper.cpp"
      INCLUDE_DIRS ".")

This configuration will just compile the Fonts demo to test your epaper display.

## Fonts support and German characters

Please check [Adafruit page on adding new fonts](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts#adding-new-fonts-2002831-18) as a start. In order to add the whole character range, you need to set from -> to ranges after calling font convert. Just go to the /components/Adafruit-GFX/fontconvert directory and run:

./fontconvert /usr/share/fonts/truetype/ubuntu/YOURTTF.ttf 18 32 252 > ../Fonts/YOURTTFNAME.h

Be aware that PROGMEM is not supported here since we are not in the Arduino framework. So just remove it from the generated fonts.

As an example with all characters including German umlauts ( ä ö ü and others like ß) I left Ubuntu_M18pt8b ready in the Fonts directory. Be aware that using the whole character spectrum will also take part of your programs memory.

### Submodules

Not being used at the moment since all test and development happens here. Only when there are new working models they will be pushed as new release in the component repository:
[CalEPD epaper component](https://github.com/martinberlin/CalEPD) is published on version 0.9

ESP-IDF uses relative locations as its submodules URLs (.gitmodules). So they link to GitHub. To update the submodules once you **git clone** this repository:

    git submodule update --init --recursive
    
to download the submodules (components) for this project.
Reminder for myself, in case you update the module library just execute:

    # pull all changes for the submodules
    git submodule update --remote

### Compile this 

If it's an ESP32

    idf.py -D IDF_TARGET=esp32 menuconfig

If it's an ESP32S2

    idf.py -D IDF_TARGET=esp32s2 menuconfig

Make sure to edit **Display configuration** in the Kconfig menuoptions.
**CALE configuration** has for the moment only a LEDBUILTIN_GPIO setting so it's not important at all. Later will be the place to configure CALE.es image url.

And then just build and flash

    idf.py build
    idf.py flash

To clean and start again in case you change target (But usually no need to run)

    idf.py fullclean

To open the serial monitor

    idf.py monitor

Please note that to exit the monitor in Espressif documentation says Ctrl+] but in Linux this key combination is:

    Ctrl+5

## config-examples/

In the config-examples folder we left samples of GPIO configurations. For example:

- Wave12I48 has the GPIOs ready to use w/Waveshare socket for ESP32-devKitC
- S2 has a sample GPIO config to be used after idf.py set-target esp32s2 (Only for S2 chips)

## SPI speed

If you instantiate display.init(true) it activates verbose debug and also lowers SPI frequency to 50000. Most epapers accept a frequency up to 4 Mhz. 
We did this on debug to be able to sniff with an ESP32 SPI slave "man in the middle" what commands are sent to the display so we can detect mistakes. Even if you print it, is not the same as to hear on the line, this is the real way to reverse engineer something. Hear what the master is saying in a library that works.

    +    uint16_t multiplier = 1000;
    +    if (debug_enabled) {
    +        frequency = 50;
    +        multiplier = 1;
    +    }

Due to restrictions in C++ that I'm not so aware about there is a limitation when using big integers in the structs { }
So SPI frequency is calculated like:

    spi_device_interface_config_t devcfg={
        .mode=0,  //SPI mode 0
        .clock_speed_hz=frequency*multiplier*1000,  // --> Like this being the default 4 Mhz
        .input_delay_ns=0,
        .spics_io_num=CONFIG_EINK_SPI_CS,
        .flags = (SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE),
        .queue_size=5
    };

Feel free to play with Espressif IDF SPI settings if you know what you are doing ;)

## Multi-SPI displays

A new breed of supported displays is coming being the first the [Wave12I48 12.48" b/w epaper from Waveshare](https://github.com/martinberlin/cale-idf/wiki/Model-wave12i48.h).
This is the first component that support this multi epaper displays abstracting their complexity so you can treat it as a normal 1304x984 single display and use all the Adafruit GFX methods and fonts to render graphics over it.
Please note that this big display needs a 160 Kb buffer leaving no DRAM available for anything more on your ESP32. So you can either make a very simple program that renders sensor information, or do everything you want, but adding PSRAM for the GFX buffer. Think about ESP32-WROOVER as a good candidate. 

## Watchdogs feeding for large buffers

In Buffers for big displays like 800*480 where the size is about 48000 bytes long is necessary to feed the watchdog timer and also make a small delay. I'm doing it this way:

    +    // Let CPU breath. Withouth delay watchdog will jump in your neck
    +    if (i%8==0) {
    +       rtc_wdt_feed();
    +       vTaskDelay(pdMS_TO_TICKS(1));
    +     }

Again, if you know more about this than me, feel free to suggest a faster way. It's possible to disable also the [watchdogs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html) but of course that is not a good practice to do so.

### References and related projects

[CALE.es Web-service](https://CALE.es) a Web-Service that prepares BMP & JPG Screens with the right size for your displays

[CALE.es Arduino-espressif32 firmware](https://github.com/martinberlin/eink-calendar)

[esp32.com "epaper" search](https://esp32.com/search.php?keywords=epaper&fid%5B0%5D=13)

[GxEPD Epaper library](https://CALE.es) GxEPD is to use with Espressif Arduino Framework. 

### History

This is the beginning, and a very raw try, to make CALE compile in the Espressif IOT Development Framework. At the moment to explore how difficult it can be to pass an existing ESP32 Arduino framework project to a ESP-IDF based one and to measure how far we can go compiling this with Espressif's own dev framework. 
UPDATE: Saved for historical reasons. After starting this project I heavily adopted ESP-IDF as an IoT framework and toolsuit to build firmares. This become also the start of [CalEPD that is our own IDF component to control Epapers](https://github.com/martinberlin/CalEPD) with ESP32 / ESP32S2.

### Credits 

GxEPD has been a great resource to start with. For CalEPD component, we mantain same Constants only without the **Gx prefix** and use the same driver nomenclature as GxEPD library, just in small case.
Hats off to Jean-Marc Zingg that was the first one to make such a great resource supporting so many Eink displays. Please note that there are no plans to port this to Arduino-framework. This repository was specially made with the purpouse to explore Espressif's own IoT development framework.

Thanks to all the developers interested to test this. Special mentions for @IoTPanic, Spectre and others that pushed me to improve my C++ skills.
