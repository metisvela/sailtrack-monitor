<p align="center">
  <img src="https://raw.githubusercontent.com/metisvela/sailtrack/main/assets/sailtrack-logo.svg" width="180">
</p>

<p align="center">
  <img src="https://img.shields.io/github/license/metisvela/sailtrack-monitor" />
  <img src="https://img.shields.io/github/v/release/metisvela/sailtrack-monitor" />
</p>

# SailTrack Monitor

SailTrack Moitor is a component of the SailTrack system, it displays data to the onboard crew using the e-paper technology in order to achieve the best readability under bad sunlight conditions. To learn more about the SailTrack project, please visit the [project repository](https://github.com/metisvela/sailtrack).

The SailTrack Monitor module is based on a battery powered LilyGo EPD47, consisting of an [ESP32](https://www.espressif.com/en/products/socs/esp32) microcontroller, connected to an e-paper display. For a more detailed hardware description of the module, please refer to the [Bill Of Materials](hardware/BOM.csv). The 3D-printable enclosure con be found [here](hardware/STL).

The module performs the following tasks:

* It gets up to 4 metrics from the SailTrack Network and displays them in the e-paper display with a refresh rate up to 2 Hz.

<p align="center">
  <br/>
  <img src="hardware/Connection Diagram.svg">
</p>

![module-image](hardware/Module%20Image.jpg)

## Installation

Follow the instructions below to get the SailTrack Monitor firmware correctly installed. If you encounter any problem, please [open an issue](https://github.com/metisvela/sailtrack-monitor/issues/new).

1. [Install PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html).
2. Clone the SailTrack Monitor repository:
   ```
   git clone https://github.com/metisvela/sailtrack-monitor.git 
   ``` 
3. Cd into the directory:
   ```
   cd sailtrack-monitor
   ```
4. Connect the module with an USB cable.
5. Finally, flash the firmware:
   ```
   pio run
   ```

## Usage

Once the firmware is uploaded the module can work with the SailTrack system. When SailTrack Monitor is turned on, the SailTrack logo will appear on the screen, meaning that the module is trying to connect to the SailTrack Network. Once the module is connected the SailTrack logo will disappear and the metrics will start updating on the screen.

## Contributing

Contributors are welcome. If you are a student of the University of Padova, please apply for the Metis Sailing Team in the [website](http://metisvela.dii.unipd.it), specifying in the appliaction form that you are interested in contributing to the SailTrack Project. If you are not a student of the University of Padova, feel free to open Pull Requests and Issues to contribute to the project.

## License

Copyright © 2023, [Metis Sailing Team](https://github.com/metisvela). SailTrack Monitor is available under the [GPL-3.0 license](https://www.gnu.org/licenses/gpl-3.0.en.html). See the LICENSE file for more info.
